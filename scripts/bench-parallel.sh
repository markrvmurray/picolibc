#!/bin/bash
# bench-parallel.sh — MC6809 parallel-build + parallel-tally driver.
#
# Build phase, per -O level (parallel via xargs -P):
#   1. meson setup builddir-mc6809-<O>  (once, reuses on re-run)
#   2. ninja -t clean                    (ninja is blind to compiler mtime)
#   3. ninja -k 0 -C ...                (parallel across waves)
#
# Lit phase, once per invocation (cheap, ~5s):
#   3.5 llvm-lit -v test/CodeGen/MC6809/ + test/MC/MC6809/Execution/
#       + clang/test/{CodeGen,Sema,Driver}/mc6809* + the OS-9 runtime
#       cases (which must actually run: see the count check)
#       → one runs row with opt_level='lit' + one results row per lit
#       test.  The on-usim execution suite (Bug #358) is part of this
#       gate — it runs each .s end-to-end on usim09batch/MAME.
#
# Tally phase, once across all levels:
#   4. run-mc6809-tests --record --jobs N --multi LEVEL:DIR,LEVEL:DIR,…
#
# Each builddir is its own object store — no cross-contamination
# during the build phase. The tally phase shares a single SQLite
# handle (WAL mode) across every level's results.
#
# Tally phase: SINGLE flat parallel pool across every (level, test)
# pair via run-mc6809-tests --multi. The driver collects every test
# from every per-level builddir into one queue, sorts longest-first
# using prior cycle counts (per level), and feeds --test-jobs workers
# from that pool. The wall time of the tally is therefore bounded by
# (longest test wall time) + (queue tail / workers) — instead of
# previously (sum-over-levels of per-level slowest-test wall time).
#
# Usage:
#   bench-parallel.sh [options]
#   bench-parallel.sh --levels O0,O1,O2,Os --test-jobs 8
#
# Options:
#   --levels LIST        comma-sep subset (default: O0,O1,O2,O3,Og,Os,Oz,Ofast,Os-lto)
#   --build-jobs N       parallel waves during build phase (default: 6)
#   --test-jobs N        workers in the shared tally pool (default: 6)
#   --slow               (deprecated; no-op since Bug #288) — the
#                        trimmed slow tests (Bug #248) now run by
#                        default. ~7 min wall premium on a 35-min bench.
#   --fast               opt OUT of the formerly-slow tests
#                        (SLOW_TESTS at all levels + MAME_SLOW_TESTS
#                        at MAME-backed levels). Saves ~7 min wall.
#   --skip-build         reuse existing binaries (tally-only rerun)
#   --skip-tally         build phase only
#   --no-preflight       pass through to run-mc6809-tests --multi
#   --tests SUBSTRS      comma-sep substrings; only matching tests run.
#                        Combine with --skip-build --levels O1 for fast
#                        single-test sanity checks (~5-30s vs ~13min full).
#                        Bug #168.
#   --simulator BACKEND  usim (default, 6809-only) or mame (HD6309-capable
#                        llvm6309 SBC). MAME runs use the
#                        cross-clang-mc6809-unknown-elf-mame.txt cross-
#                        file, append `-mame` to builddirs and ledger
#                        opt_level rows. Bug #194 Phase B.
#   -h, --help           show this header
#
# Env:
#   BENCH_HD6309_BACKEND  usim (default) or mame — executor for the -hd6309-mame
#                         levels. USim is ~11x faster and result-identical to
#                         MAME; their cycle models differ (non-comparable), so
#                         results record under -hd6309-usim vs -hd6309-mame
#                         opt_levels respectively. Set =mame for a ground-truth
#                         cross-check.
#   MC6809_BENCH_LOCK     the lock that keeps this to one bench at a time
#                         (default /tmp/mc6809-bench.lock)
#   MC6809_BENCH_LOG      this run's log (default /tmp/bench-parallel.log); the
#                         lit log and delta table sit beside it
#
# Exit status: 1 if a gate failed, and the last line says which: a level that
# did not build or hit the verifier, a lit run that did not pass or did not
# run, a test that did not pass, a tally harness that failed, or a level with
# no OK results or a collapsed OK count.  Otherwise 0.
#
# A test that did not pass is FAIL, TIMEOUT, ERROR or UNEXPECTEDPASS.
# EXPECTEDFAIL and SKIP pass, and so do BUILDFAIL, TOOBIG and OSLIMIT: those
# never ran, and are gated by the build and OK-count checks instead.

set -u
bench_failed=0
gate_failures=""

# Record a failed gate: the bench carries on, so that the ledger still gets
# whatever can be measured, and exits 1 at the end naming every one of these.
_gate_fail() {
  bench_failed=1
  gate_failures="${gate_failures:+$gate_failures; }$1"
}

# Locations. Defaults assume the MC6809 repos are siblings of this
# picolibc checkout (PICO/../llvm-mc6809, PICO/../usim, PICO/../mame).
# Override any of these by exporting the matching variable. See
# scripts/README-mc6809.md.
PICO=${PICO:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}
PARENT=$(dirname "$PICO")
LLVM_REPO=${LLVM_REPO:-$PARENT/llvm-mc6809}
LLVM_BUILD_SUBDIR=${LLVM_BUILD_SUBDIR:-llvm/cmake-build-debug-system}
# MC6809_TOOLCHAIN points the whole bench at an installed toolchain instead
# of a build tree: its bin/ is the compiler, and the cross files generated
# below follow.  Without it, nothing changes.
if [ -n "${MC6809_TOOLCHAIN:-}" ]; then
  LLVM_BIN=${LLVM_BIN:-$MC6809_TOOLCHAIN/bin}
fi
LLVM_BIN=${LLVM_BIN:-$LLVM_REPO/$LLVM_BUILD_SUBDIR/bin}
USIM=${USIM:-$PARENT/usim}
MAME_RUNNER=${MAME_RUNNER:-$PARENT/mame/run-mc6809-mame}
DB=${MC6809_BENCH_DB:-$HOME/Documents/mc6809-bench/results.sqlite}
SQLITE_BUSY_TIMEOUT_MS=${MC6809_BENCH_DB_BUSY_TIMEOUT_MS:-60000}
LOG=${MC6809_BENCH_LOG:-/tmp/bench-parallel.log}

sqlite_bench() {
  sqlite3 -cmd ".timeout $SQLITE_BUSY_TIMEOUT_MS" "$DB" "$@"
}

# Ensure the ledger exists with the current schema BEFORE anything writes to
# it. The lit gate (and other shell-side recording below) use the sqlite3 CLI,
# which does not create tables — so on a fresh machine, or a ledger predating a
# column, those writes would fail ("no such table" / "no column named
# opt_level"). create-bench-db.sh is idempotent and non-destructive
# (CREATE ... IF NOT EXISTS + additive ALTERs only), so this is safe to run
# against an existing populated ledger every time.
"$PICO/scripts/create-bench-db.sh" "$DB" >/dev/null

# Bug #194 Phase B: simulator backend selector. Default is usim
# (canonical for 6809). MAME's `llvm6309` SBC is the HD6309-capable
# alternative; identical memory map, different exe_wrapper. Selected
# via `--simulator=mame` on the CLI; bench rows are tagged with
# `-mame` suffixed opt_level (e.g. `Os-mame`) for easy DB filtering
# and to avoid stomping the canonical usim history.
SIMULATOR="usim"
USIM_CROSS=$PICO/scripts/cross-clang-mc6809-unknown-elf.txt
MAME_CROSS=$PICO/scripts/cross-clang-mc6809-unknown-elf-mame.txt
OS9_CROSS=$PICO/scripts/cross-clang-mc6809-unknown-os9.txt
CROSS=$USIM_CROSS
BD_SUFFIX=""

# Cross-platform mtime helper. macOS uses `stat -f %m`, Linux `stat -c %Y`.
file_mtime() {
  stat -f %m "$1" 2>/dev/null || stat -c %Y "$1" 2>/dev/null || echo ""
}

# The commit the binaries were built from.  It is marked "+dirty" only
# when tracked files differ from HEAD right now: the binaries may then
# hold changes no commit records.  A binary newer than HEAD is not a
# sign of that -- any rebuild after a commit makes one -- so its mtime
# is recorded on its own (get_llvm_binary_mtime) instead of changing
# the commit.  run-mc6809-tests records the commit the same way.
get_llvm_commit() {
  local sha
  sha=$(cd "$LLVM_REPO" && git rev-parse HEAD 2>/dev/null || echo unknown)
  local dirty=""
  if (cd "$LLVM_REPO" && ! git diff --quiet HEAD -- 2>/dev/null); then
    dirty="+dirty"
  fi
  echo "${sha}${dirty}"
}

get_llvm_binary_mtime() {
  if [ -x "$LLVM_BIN/llc" ]; then
    file_mtime "$LLVM_BIN/llc"
  else
    echo ""
  fi
}

export PATH="$LLVM_BIN:$USIM:$PATH"
export LLVM_OBJCOPY="$LLVM_BIN/llvm-objcopy"
export LLVM_NM="$LLVM_BIN/llvm-nm"

# Generate the meson cross files for this machine's toolchain location.
# They are gitignored (machine-specific absolute paths) — see
# scripts/gen-mc6809-cross.sh. Regenerating is idempotent and cheap.
PICO="$PICO" LLVM_BIN="$LLVM_BIN" MC6809_TOOLCHAIN="${MC6809_TOOLCHAIN:-}" \
  "$PICO/scripts/gen-mc6809-cross.sh" >>"$LOG" 2>&1 \
  || { echo "bench-parallel: failed to generate cross files (see $LOG)" >&2; exit 1; }

# Suppress LLVM crash-handler symbolisation. Without FP support, every
# build wave generates dozens of expected clang aborts on FP-using
# picolibc sources (G_FCMP / G_FNEG legalisation). Each abort forks
# llvm-symbolizer which loads megabytes of DWARF only to produce a
# stack trace nobody is going to read. Pointing LLVM_SYMBOLIZER_PATH
# at a non-executable path makes the symbolizer lookup fail
# immediately and LLVM falls back to printing raw addresses (cheap).
# LLVM_DISABLE_CRASH_REPORT=1 also skips the macOS Apple-Crash
# integration. Net result: builds finish noticeably faster and the
# wave logs stay clean. Remove these once FP is enabled.
export LLVM_DISABLE_CRASH_REPORT=1
export LLVM_SYMBOLIZER_PATH=/dev/null

# Canonical meson options, taken verbatim from the single-O builddir.
# The only between-wave delta is the opt-level args injected by level_opts.
# Bug #220: --cross-file is NOT in COMMON — `level_cross` returns the
# correct cross-file per level, so a single sweep can mix usim and
# MAME wrappers (e.g. `Os-hd6309-mame` alongside `O2` in DEFAULT_LEVELS).
COMMON=(
  -Dformat-default=integer
  # printf-aliases stays off: turning it on is global.  It emits
  # -D_PICOLIBC_PRINTF for every test built with printf_compile_args_d,
  # which pulls the double vfprintf into all of them -- and on a 64K
  # target most of them then do not fit.  Measured at Os-fp: build
  # failures 69 -> 187, every one of them a section overflow.  (The
  # first measurement, when this was tried and reverted in May 2026,
  # was OK 96 -> 69.)  A test that genuinely needs a printf variant
  # opts in by itself, the way libc-testsuite/sscanf does with
  # -D_PICOLIBC_SCANF='d' plus a --defsym on its own rule.
  -Dprintf-aliases=false
  -Dstdio-float=false
  -Dposix-extensions=false
  -Dsearch-extensions=true
  -Dwant-libm=false
  -Dmb-capable=false
  -Dio-long-long=false
  # malloc and realloc return memory uncleared, as C allows; calloc
  # clears.  Clearing every block cost about 14 cycles a byte.
  -Dmalloc-clear-allocated=false
  -Dtests=true
  -Dtests-enable-stack-protector=false
  # The MC6809 cross files set this, but only a fresh build directory reads
  # options from a cross file; this keeps the bench's existing ones in step.
  -Dfast-strcmp=false
)

level_opts() {
  # Levels without a native meson mapping (z, fast) go through
  # optimization=plain + an explicit -O flag injected into c/cpp_args.
  #
  # Naming convention:
  #   <opt>           plain MC6809, no LTO
  #   <opt>-lto       plain MC6809 with LTO (whole-program optimisation)
  #   <opt>-fp        FP-enabled (libm + stdio-float), no LTO
  #   <opt>-lto-fp    FP-enabled with LTO
  #   <opt>-hd6309-mame   HD6309 instructions, MAME simulator
  #   <opt>-lto-hd6309-mame  HD6309 + LTO + MAME
  case "$1" in
    # ---- plain MC6809, no LTO ----
    O0)    echo "-Doptimization=0" ;;
    O1)    echo "-Doptimization=1" ;;
    O2)    echo "-Doptimization=2" ;;
    O3)    echo "-Doptimization=3" ;;
    Os)    echo "-Doptimization=s" ;;
    # Bug #269: at Og variants, _setup_wave additionally appends
    # `-mllvm -verify-machineinstrs` to c_args/cpp_args via a separate
    # meson configure call (the comma-separated single-string form
    # accepted by `-Dc_args=` here cannot encode multi-word args, but
    # a separate `meson configure` step CAN take a quoted string).
    Og)    echo "-Doptimization=g" ;;
    Oz)    echo "-Doptimization=plain -Dc_args=-Oz -Dcpp_args=-Oz" ;;
    # -O3 -ffast-math is what -Ofast means; clang now warns that -Ofast is
    # deprecated, and picolibc probes every one of its common_options with
    # -Werror, so a bare -Ofast in c_args made every probe fail and silently
    # dropped the whole list (-Werror=vla, -fno-builtin-copysignl,
    # -fno-stack-protector, ...) from the Ofast builddirs.
    Ofast) echo "-Doptimization=plain -Dc_args=['-O3','-ffast-math'] -Dcpp_args=['-O3','-ffast-math']" ;;

    # ---- OS-9, hosted on NitrOS-9 ----
    # A different library altogether: picolibc's OS-9 system layer instead
    # of the bare-metal semihost one, no picocrt (the OS-9 CRT comes from
    # compiler-rt), and a console that is the shell's.  os9-Os runs at
    # Level 1 and os9-Os-fp at Level 2 -- floating point needs the FPO9
    # module beside the program, which only Level 2 has room for.
    os9-Os) echo "-Doptimization=s -Dpicocrt=false -Dsemihost=false -Dos-os9=true -Dposix-console=true" ;;
    os9-Os-fp) echo "-Doptimization=s -Dpicocrt=false -Dsemihost=false -Dos-os9=true -Dposix-console=true -Dstdio-float=true -Dwant-libm=true" ;;

    # ---- plain MC6809, LTO ----
    O2-lto) echo "-Doptimization=2 -Db_lto=true" ;;
    O3-lto) echo "-Doptimization=3 -Db_lto=true" ;;
    Os-lto) echo "-Doptimization=s -Db_lto=true" ;;

    # ---- HD6309 (USim), kept for ad-hoc use only — not in DEFAULT_LEVELS
    # because USim is MC6809-only and HD6309 instructions trap at runtime.
    Os-hd6309) echo "-Doptimization=s -Dc_args=-mcpu=hd6309 -Dcpp_args=-mcpu=hd6309 -Dc_link_args=-mcpu=hd6309 -Dcpp_link_args=-mcpu=hd6309" ;;

    # ---- HD6309 (MAME) ----
    # Deliberate asymmetry: only the bench-worthy tier is wired up
    # (Os/Os-lto/O2/Og and their `-fp` variants below).  The
    # O0/O1/O3/Oz/Ofast-hd6309-mame combinations are intentionally NOT
    # here — they add MAME wall time (1–2s per test for emulator
    # startup × ~100 tests × 5 levels = ~15 min) for a tier that has
    # never surfaced a unique regression vs. its USim sibling.  Add
    # the case arm if a specific investigation needs it; passing one
    # of these names today prints `unknown level <name>` and skips
    # only that level's setup.
    Os-hd6309-mame)         echo "-Doptimization=s -Dc_args=-mcpu=hd6309 -Dcpp_args=-mcpu=hd6309 -Dc_link_args=-mcpu=hd6309 -Dcpp_link_args=-mcpu=hd6309" ;;
    Os-lto-hd6309-mame)     echo "-Doptimization=s -Db_lto=true -Dc_args=-mcpu=hd6309 -Dcpp_args=-mcpu=hd6309 -Dc_link_args=-mcpu=hd6309 -Dcpp_link_args=-mcpu=hd6309" ;;
    O2-hd6309-mame)         echo "-Doptimization=2 -Dc_args=-mcpu=hd6309 -Dcpp_args=-mcpu=hd6309 -Dc_link_args=-mcpu=hd6309 -Dcpp_link_args=-mcpu=hd6309" ;;
    # Debugger-friendly: -Og preserves enough source-level structure
    # for lldb/gdb walks while still doing safe peepholes.  Useful for
    # the bug-#222 lldb-on-MAME workflow when an HD6309-specific
    # codegen issue needs source-level stepping.
    Og-hd6309-mame)         echo "-Doptimization=g -Dc_args=-mcpu=hd6309 -Dcpp_args=-mcpu=hd6309 -Dc_link_args=-mcpu=hd6309 -Dcpp_link_args=-mcpu=hd6309" ;;

    # ---- HD6309 (MAME) + FP ----
    # The same expectation applies as for the plain FP non-LTO levels:
    # many of these will overflow on a 64 KB target (HD6309 instruction
    # encodings are larger on average) and surface as BUILDFAIL via #237.
    Os-hd6309-mame-fp)         echo "-Doptimization=s -Dstdio-float=true -Dwant-libm=true -Dc_args=-mcpu=hd6309 -Dcpp_args=-mcpu=hd6309 -Dc_link_args=-mcpu=hd6309 -Dcpp_link_args=-mcpu=hd6309" ;;
    Os-lto-hd6309-mame-fp)     echo "-Doptimization=s -Db_lto=true -Dstdio-float=true -Dwant-libm=true -Dc_args=-mcpu=hd6309 -Dcpp_args=-mcpu=hd6309 -Dc_link_args=-mcpu=hd6309 -Dcpp_link_args=-mcpu=hd6309" ;;
    O2-hd6309-mame-fp)         echo "-Doptimization=2 -Dstdio-float=true -Dwant-libm=true -Dc_args=-mcpu=hd6309 -Dcpp_args=-mcpu=hd6309 -Dc_link_args=-mcpu=hd6309 -Dcpp_link_args=-mcpu=hd6309" ;;
    # Debugger-friendly + LTO + FP on HD6309: the most aggressive
    # configuration with -Og's source-level structure preserved.
    # Pairs with Og-hd6309-mame for FP-aware lldb-on-MAME work.
    Og-lto-hd6309-mame-fp)     echo "-Doptimization=g -Db_lto=true -Dstdio-float=true -Dwant-libm=true -Dc_args=-mcpu=hd6309 -Dcpp_args=-mcpu=hd6309 -Dc_link_args=-mcpu=hd6309 -Dcpp_link_args=-mcpu=hd6309" ;;

    # ---- FP-enabled levels (Bug #162) ----
    # Override the COMMON float-disabled flags; meson takes the last
    # -D value so these come after COMMON in the setup call.  Many of
    # these are expected to overflow the 64 KB address space (test
    # binaries land in BUILDFAIL via Bug #237) — that's the point: the
    # bench surfaces which combinations need slimming.  format-default
    # stays 'integer' (the COMMON default); per-test #224-style alias
    # routing pulls in FP printf/scanf only where needed.
    O0-fp)     echo "-Doptimization=0 -Dstdio-float=true -Dwant-libm=true" ;;
    O1-fp)     echo "-Doptimization=1 -Dstdio-float=true -Dwant-libm=true" ;;
    O2-fp)     echo "-Doptimization=2 -Dstdio-float=true -Dwant-libm=true" ;;
    O3-fp)     echo "-Doptimization=3 -Dstdio-float=true -Dwant-libm=true" ;;
    Og-fp)     echo "-Doptimization=g -Dstdio-float=true -Dwant-libm=true" ;;
    # Os-fp is now distinct from Os-lto-fp: non-LTO baseline.  Pre-2026-05-09
    # it was identical to Os-lto-fp (both LTO=true) — the dedup was the
    # change.
    Os-fp)     echo "-Doptimization=s -Dstdio-float=true -Dwant-libm=true" ;;
    Oz-fp)     echo "-Doptimization=plain -Dc_args=-Oz -Dcpp_args=-Oz -Dstdio-float=true -Dwant-libm=true" ;;
    Ofast-fp)  echo "-Doptimization=plain -Dc_args=['-O3','-ffast-math'] -Dcpp_args=['-O3','-ffast-math'] -Dstdio-float=true -Dwant-libm=true" ;;
    Os-lto-fp) echo "-Doptimization=s -Db_lto=true -Dstdio-float=true -Dwant-libm=true" ;;

    *)     echo "unknown level $1" >&2; return 2 ;;
  esac
}

# Bug #220: per-level cross-file selection. Levels whose name ends in
# `-mame` use the MAME cross-file (exe_wrapper = run-mc6809-mame); all
# others use the global $CROSS (controlled by --simulator). This lets
# `Os-hd6309-mame` be a first-class level in DEFAULT_LEVELS alongside
# the usim levels — a single sweep covers both simulator backends.
level_cross() {
  # 2026-05-09: also match *-mame-fp so HD6309 FP variants get the
  # MAME cross-file. Glob `*-mame*` would over-match anything containing
  # the substring; the explicit alternation keeps intent legible.
  case "$1" in
    os9-*)            echo "$OS9_CROSS" ;;
    *-mame|*-mame-fp) echo "$MAME_CROSS" ;;
    *)                echo "$CROSS" ;;
  esac
}

# Bug #220: per-level builddir suffix. Returns "-mame" for *-mame
# levels (so Os-hd6309-mame's builddir is `builddir-mc6809-Os-hd6309-mame`
# whether reached via `--levels Os-hd6309-mame` directly or via
# legacy `--simulator mame --levels Os-hd6309`) and "" otherwise.
# Falls back to the global $BD_SUFFIX for compatibility with the
# legacy --simulator flag.
level_bd_suffix() {
  case "$1" in
    *-mame|*-mame-fp) echo "" ;;  # already in label
    *)                echo "$BD_SUFFIX" ;;
  esac
}

# --- Per-wave phase implementations (also invoked via self-reinvoke) ----

# Bug #220: wave_log / wave_bd use level_bd_suffix so *-mame levels
# resolve to `builddir-mc6809-<level>` (the -mame is in the label
# itself), while non-mame levels still pick up the legacy global
# BD_SUFFIX (set by `--simulator mame`).
wave_log() { local s; s="$(level_bd_suffix "$1")"; echo "/tmp/bench-parallel-$1${s}.log"; }
wave_bd()  { local s; s="$(level_bd_suffix "$1")"; echo "$PICO/builddir-mc6809-$1${s}"; }
ts()       { date '+%H:%M:%S'; }

# HD6309 execution backend — USim is now the default (gen-mc6809-cross.sh points
# the HD6309 cross's exe_wrapper at run-mc6809-usim6309 unless BENCH_HD6309_BACKEND
# =mame). The build is backend-identical, so the builddir + cross routing keep the
# historical -hd6309-mame names; only the RECORDED opt_level is made backend-aware
# (level_record_label) so the non-comparable USim vs MAME cycle series stay
# separate in the ledger (-hd6309-usim vs -hd6309-mame).
HD6309_BACKEND="${BENCH_HD6309_BACKEND:-usim}"
level_record_label() {
  case "$1" in
    *-hd6309-mame|*-hd6309-mame-fp)
      if [ "$HD6309_BACKEND" = "mame" ]; then echo "$1"
      else echo "${1/-hd6309-mame/-hd6309-usim}"; fi ;;
    *) echo "$1" ;;
  esac
}

_setup_wave() {
  local label="$1"
  local bd wl opts cross
  bd="$(wave_bd "$label")"
  wl="$(wave_log "$label")"
  opts="$(level_opts "$label")" || return 2
  cross="$(level_cross "$label")"

  # Bug #269: at Og variants, append `-mllvm -verify-machineinstrs`
  # to c_args / cpp_args. The MachineVerifier catches the Bug #247 /
  # #251 / #266-class regressions (pseudos lying about Defs/Uses) at
  # COMPILE TIME — turning multi-session bisects into 1-bench-cycle
  # diagnoses. Done as a separate `meson configure` call so the
  # quoted multi-word string survives (meson's `-Dc_args=foo,bar`
  # comma form is treated as a single-element array, useless here).
  local existing_c existing_cpp full_c full_cpp
  case "$label" in
    Og|Og-fp|Og-hd6309-mame|Og-lto-hd6309-mame-fp)
      # Read whatever the level's level_opts already set, append the
      # verifier flag. The existing values appear in $opts as
      # `-Dc_args=...` / `-Dcpp_args=...`. Use Bash array splitting to
      # preserve atoms, then post-process.
      existing_c=$(echo "$opts" | tr ' ' '\n' | grep -E '^-Dc_args=' | head -1 | sed 's/^-Dc_args=//')
      existing_cpp=$(echo "$opts" | tr ' ' '\n' | grep -E '^-Dcpp_args=' | head -1 | sed 's/^-Dcpp_args=//')
      full_c="${existing_c:+${existing_c} }-mllvm -verify-machineinstrs"
      full_cpp="${existing_cpp:+${existing_cpp} }-mllvm -verify-machineinstrs"
      # Strip the original -Dc_args= / -Dcpp_args= tokens from $opts
      # (we'll re-add via meson configure with the full quoted value).
      opts=$(echo "$opts" | tr ' ' '\n' | grep -v -E '^-D(c|cpp)_args=' | tr '\n' ' ')
      ;;
  esac

  echo "==[ $(ts) $label setup ]== bd=$bd cross=$cross opts=$opts" >>"$wl"
  if [ ! -f "$bd/build.ninja" ]; then
    # shellcheck disable=SC2086
    ( cd "$PICO" && meson setup "$bd" "${COMMON[@]}" --cross-file="$cross" $opts ) >>"$wl" 2>&1
  else
    # `meson setup --reconfigure` re-reads the cross-file (so any
    # edits to scripts/cross-clang-mc6809-unknown-elf*.txt — e.g. a
    # MEMORY-layout change — propagate). `meson configure` after
    # then applies the per-level options. Both are idempotent and
    # cheap when nothing has changed.
    # shellcheck disable=SC2086
    ( cd "$PICO" \
      && meson setup --reconfigure --cross-file="$cross" "$bd" \
      && meson configure "$bd" "${COMMON[@]}" $opts ) >>"$wl" 2>&1
  fi

  # Bug #269: apply c_args / cpp_args separately so the verifier flag's
  # space survives. The empty-default branch (full_c unset) skips.
  if [ -n "${full_c:-}" ]; then
    ( cd "$PICO" && meson configure "$bd" \
        -Dc_args="$full_c" -Dcpp_args="$full_cpp" ) >>"$wl" 2>&1
  fi
}

_scrub_wave() {
  local label="$1"
  local bd wl
  bd="$(wave_bd "$label")"
  wl="$(wave_log "$label")"
  # ninja is blind to compiler-binary mtime: a rebuilt clang/llc with
  # unchanged picolibc sources leaves every object, libc.a and test
  # binary "up to date", and the tally records the OLD codegen against
  # the NEW SHA. Deleting only *.c.o was not enough in practice (a
  # libc.a or test ELF that survived kept stale members / stale links),
  # so scrub every output ninja knows about -- objects, archives, ELFs,
  # generated headers -- and then re-run the file scan for anything
  # outside ninja's graph. `ninja -t clean` is fast (seconds) and, unlike
  # `meson setup --wipe`, keeps the configured build dir intact.
  [ -f "$bd/build.ninja" ] && ninja -C "$bd" -t clean >>"$wl" 2>&1
  find "$bd" \( -name '*.c.o' -o -name '*.cpp.o' -o -name '*.S.o' \
                -o -name 'libc.a' -o -name 'libm.a' \) -delete 2>/dev/null
}

_build_wave() {
  local label="$1"
  local bd wl
  bd="$(wave_bd "$label")"
  wl="$(wave_log "$label")"
  echo "==[ $(ts) $label build start ]==" >>"$wl"
  ninja -C "$bd" -k 0 >>"$wl" 2>&1
  local rc=$?
  # ninja non-zero is expected for FP-gated targets; libc.a + non-FP
  # test binaries still build along the way. (project_fp_not_enabled.md)
  echo "==[ $(ts) $label build end rc=$rc ]==" >>"$wl"
  # Sidecar for the driver to surface in the top-level summary so a
  # silent libc.a-link failure is no longer indistinguishable from a
  # clean build. (bug #157)
  local failed_n bin_n verifier_n
  # Bug #277: scope FAILED/VERIFY greps to the most recent build wave.
  # The preflight scrub at the top of the driver truncates $wl, so the
  # only way old content reaches the log is via external appenders
  # between scrub and wave. The scoping below sets a print-from-here
  # flag at the most recent `==[ ... build start ]==` marker and pipes
  # only post-marker lines into grep. Streaming form (not an
  # accumulating buffer) so it's O(n) — earlier O(n^2) version stalled
  # on multi-hundred-MB logs.
  failed_n=$(awk '/^==\[ .* build start \]==/ {seen=1; next} seen' "$wl" 2>/dev/null \
             | grep -c '^FAILED: ' || echo 0)
  # Count picolibc test binaries (bug #170): they live under $bd/test,
  # have no `.elf` extension, and are owner-executable. The previous
  # `find -name '*.elf'` returned 0 unconditionally because picolibc
  # doesn't suffix its test executables. `-perm -u+x` is POSIX (BSD
  # find on macOS plus GNU find both accept it). The `*.p/*` exclusion
  # skips meson's intermediate object directories.
  bin_n=$(find "$bd/test" -type f -perm -u+x -not -path '*.p/*' \
                2>/dev/null | wc -l | tr -d ' ')
  # Bug #269: count MachineVerifier hits from `-mllvm -verify-machineinstrs`
  # (enabled at Og variants in level_opts above). Zero for non-Og levels
  # because the flag isn't on. Any non-zero count surfaces in the
  # per-level summary as a VERIFY=N column + WARN line.
  # Bug #277: scoped to the most recent build wave (see failed_n above).
  verifier_n=$(awk '/^==\[ .* build start \]==/ {seen=1; next} seen' "$wl" 2>/dev/null \
               | grep -c '\*\*\* Bad machine code:' || echo 0)
  printf '%s\t%s\t%s\t%s\n' "$rc" "$failed_n" "$bin_n" "$verifier_n" > "/tmp/bench-parallel-$label.rc"
}

_refresh_builtins() {
  local llvm_build="$LLVM_REPO/$LLVM_BUILD_SUBDIR"
  local clang="$LLVM_BIN/clang"
  [ -x "$clang" ] || return 0
  local bins target
  for bins in "$llvm_build"/runtimes/builtins-mc6809-*-bins; do
    [ -d "$bins" ] || continue
    target=$(basename "$bins" -bins)
    # Up to date iff at least one builtins object post-dates the compiler
    # (they are all compiled in one go, so one is as good as all).
    if [ -n "$(find "$bins" \( -name '*.obj' -o -name '*.o' \) -newer "$clang" -print -quit 2>/dev/null)" ]; then
      echo "==[ $(ts) builtins $target up to date ]==" | tee -a "$LOG"
      continue
    fi
    echo "==[ $(ts) builtins $target older than clang -- rebuilding ]==" | tee -a "$LOG"
    find "$bins" \( -name '*.obj' -o -name '*.o' -o -name '*.a' \) -delete 2>/dev/null
    # Build the builtins' own build directory, not the LLVM tree's
    # "$target": that target depends on clang, so it would first rebuild
    # clang from whatever the LLVM working tree holds -- benching
    # uncommitted edits under the HEAD commit's name.
    if ! cmake --build "$bins" >>"$LOG" 2>&1; then
      echo "==[ $(ts) builtins $target REBUILD FAILED -- refusing to bench with stale builtins (see $LOG) ]==" | tee -a "$LOG"
      return 1
    fi
  done
  return 0
}

# Tally phase is now a single flat invocation of
# run-mc6809-tests --multi (see _tally_multi below). No per-level
# self-reinvoke needed.

_lit_tally() {
  # Run llvm-lit against all MC6809 codegen + Sema tests and record one runs
  # row (opt_level='lit') + one results row per test. Cheap (~1s) and
  # independent of the picolibc build dirs.
  #
  # Test directories:
  #   llvm/test/CodeGen/MC6809/          -- backend codegen sentinels
  #   llvm/test/MC/MC6809/Execution/     -- end-to-end on-usim execution
  #                                         tests (Bug #358; needs
  #                                         USIM09BATCH + MAMELLVM6309)
  #   clang/test/CodeGen/MC6809/         -- clang CodeGen tests (builtins)
  #   clang/test/Sema/asm-mc6809-*.c     -- inline asm constraint Sema tests
  #   clang/test/Sema/mc6809-*.c         -- target builtin Sema tests
  #   clang/test/Sema/mc6809-directpage.c
  local lit="$LLVM_BIN/llvm-lit"
  local llvm_root="$(cd "$LLVM_BIN/../.." && pwd)"
  local clang_root="$(cd "$llvm_root/../clang" && pwd)"
  local outfile="${LOG%.log}-lit.log"

  echo "==[ $(ts) lit tally start ]==" | tee -a "$LOG"

  if [ ! -x "$lit" ] || [ ! -d "$llvm_root/test/CodeGen/MC6809" ]; then
    echo "  lit: skipped (binary or testdir missing)" | tee -a "$LOG"
    # An installed toolchain ships no llvm-lit and no tests, so there the
    # gate does not apply.  From a build tree, it has failed to run.
    [ -n "${MC6809_TOOLCHAIN:-}" ] || _gate_fail "lit did not run"
    return 0
  fi

  # Build the test list: always the llvm backend tests; add clang tests if present.
  local testpaths="$llvm_root/test/CodeGen/MC6809"
  if [ -d "$clang_root/test/CodeGen/MC6809" ]; then
    testpaths="$testpaths $clang_root/test/CodeGen/MC6809"
  fi
  # Bug #358: the on-usim execution suite is now part of the bench lit
  # gate.  It was excluded while the $FFDx→$FFCx IO-map mismatch made it
  # hang 50/54; now that the Inputs/ map is fixed it runs each .s
  # end-to-end on usim09batch (a few HD6309 cases via MAME).  Each test
  # uses its own Output/<test>.s.tmp.hex, so lit's parallel workers don't
  # collide on a shared temp file (unlike run-mc6809-tests --record).
  if [ -d "$llvm_root/test/MC/MC6809/Execution" ]; then
    testpaths="$testpaths $llvm_root/test/MC/MC6809/Execution"
  fi
  for f in "$clang_root"/test/Sema/asm-mc6809-*.c \
            "$clang_root"/test/Sema/mc6809-*.c; do
    [ -f "$f" ] && testpaths="$testpaths $f"
  done
  # What the driver actually emits -- the libraries, the linker script, the
  # module name.  These cost milliseconds and were outside the gate, which
  # is how one of them stayed broken through several benches that each said
  # no regression.
  for f in "$clang_root"/test/Driver/mc6809*.c; do
    [ -f "$f" ] && testpaths="$testpaths $f"
  done
  # The end-to-end OS-9 cases: each boots NitrOS-9 and checks a real
  # console.  They need the opt-in below, and self-skip without it -- see
  # the count check after the run.
  local os9_runtime="$llvm_root/../compiler-rt/test/mc6809-os9-runtime"
  local os9_runtime_ran=0
  if [ -d "$os9_runtime" ]; then
    testpaths="$testpaths $os9_runtime"
    os9_runtime_ran=1
  fi

  # USIM09BATCH must be in the environment or the Execution suite
  # self-skips every test as UNSUPPORTED.  HD6309 execution tests run on
  # the same backend as the -hd6309 levels: usim unless
  # BENCH_HD6309_BACKEND=mame.
  # shellcheck disable=SC2086
  USIM09BATCH="$USIM/usim09batch" \
  MAMELLVM6309="$MAME_RUNNER" \
  MC6809_HD6309_SIM="$HD6309_BACKEND" \
  MC6809_OS9_RUNTIME_TESTS=1 \
  NITROS9_USIM09PT="$USIM/usim09pt" \
    "$lit" -v --no-progress-bar $testpaths > "$outfile" 2>&1
  local lit_rc=$?

  local commit picolibc_commit usim_commit timestamp host bin_mtime
  commit=$(get_llvm_commit)
  bin_mtime=$(get_llvm_binary_mtime)
  picolibc_commit=$(cd "$PICO" && git rev-parse HEAD 2>/dev/null || echo unknown)
  usim_commit=$(cd "$USIM" && git rev-parse HEAD 2>/dev/null || echo unknown)
  timestamp=$(date -u +%Y-%m-%dT%H:%M:%S+00:00)
  host=$(hostname -s)

  # Idempotent schema migration for bug #191's new column.
  sqlite_bench "ALTER TABLE runs ADD COLUMN llvm_binary_mtime INTEGER;" 2>/dev/null

  local run_id
  run_id=$(sqlite_bench "
    INSERT INTO runs (timestamp, llvm_commit, picolibc_commit, usim_commit, host, opt_level, llvm_binary_mtime)
    VALUES ('$timestamp', '$commit', '$picolibc_commit', '$usim_commit', '$host', 'lit', ${bin_mtime:-NULL});
    SELECT last_insert_rowid();
  ")

  # Parse lit verbose output; emit one INSERT per test.  A test is named by
  # lit's own identifier, "SUITE :: PATH", the only name lit keeps unique: a
  # suite's name can hold spaces, and the same file name can be in two suites.
  # The pattern is the whole result line, because -v also prints what failing
  # tests said, and that can begin "FAIL: " too.
  local result_line='^(PASS|FAIL|XFAIL|UNRESOLVED|UNSUPPORTED|XPASS|TIMEOUT|FLAKY): .+ :: .+ [(][0-9]+ of [0-9]+[)]$'
  awk -v rid="$run_id" -v re="$result_line" -v q="'" '
    $0 ~ re {
      status=$0; sub(/:.*/,"",status)
      tn=$0; sub(/^[A-Z]+: /,"",tn); sub(/ [(][0-9]+ of [0-9]+[)]$/,"",tn)
      gsub(q,q q,tn)
      printf "INSERT INTO results (run_id, test_name, suite, status, opt_level) VALUES (%s, %s%s%s, %slit-codegen%s, %s%s%s, %slit%s);\n", rid, q, tn, q, q, q, q, status, q, q, q
    }
  ' "$outfile" | sqlite_bench

  # A rejected INSERT only reaches stderr, so count what was kept.
  local reported recorded
  reported=$(grep -cE "$result_line" "$outfile" 2>/dev/null)
  recorded=$(sqlite_bench "SELECT COUNT(*) FROM results WHERE run_id = $run_id;")
  if [ "${reported:-0}" != "${recorded:-0}" ]; then
    _gate_fail "the ledger kept ${recorded:-0} of ${reported:-0} lit results, see $outfile"
  fi

  local pass fail xfail unsupp unres
  pass=$(grep -c   '^PASS: '        "$outfile" 2>/dev/null)
  fail=$(grep -c   '^FAIL: '        "$outfile" 2>/dev/null)
  xfail=$(grep -c  '^XFAIL: '       "$outfile" 2>/dev/null)
  unsupp=$(grep -c '^UNSUPPORTED: ' "$outfile" 2>/dev/null)
  unres=$(grep -c  '^UNRESOLVED: '  "$outfile" 2>/dev/null)

  local tag=" ok "
  [ "${fail:-0}"  -gt 0 ] 2>/dev/null && tag="FAIL"
  [ "${unres:-0}" -gt 0 ] 2>/dev/null && tag="UNRESOLVED"

  # lit's own status is the verdict.  It also covers what the counts above do
  # not: TIMEOUT, XPASS, and a lit that died before reporting anything.
  if [ "$lit_rc" -ne 0 ]; then
    [ "$tag" = " ok " ] && tag="FAIL"
    _gate_fail "lit exited $lit_rc (FAIL=${fail:-0} UNRESOLVED=${unres:-0}, see $outfile)"
  fi

  # A suite that reports UNSUPPORTED for every case has not passed, it has
  # not run: without its NitrOS-9 images the OS-9 cases skip themselves, and
  # a gate that counts that as green is a gate nobody should trust.
  if [ "$os9_runtime_ran" = 1 ]; then
    local os9_pass
    os9_pass=$(grep -c '^PASS: MC6809 OS-9 Runtime' "$outfile" 2>/dev/null)
    if [ "${os9_pass:-0}" -eq 0 ] 2>/dev/null; then
      tag="OS9-RUNTIME-DID-NOT-RUN"
      echo "==[ $(ts) FAIL: the OS-9 runtime cases reported nothing --" \
           "NitrOS-9 images not found?  See $outfile ]==" | tee -a "$LOG"
      # A gate that ran nothing must not pass, for the same reason a level
      # that built nothing must not.
      _gate_fail "the OS-9 runtime cases did not run"
    fi
  fi

  echo "==[ $(ts) lit tally done: $tag PASS=${pass:-0} FAIL=${fail:-0} XFAIL=${xfail:-0} UNSUPP=${unsupp:-0} UNRES=${unres:-0} run_id=$run_id ]==" | tee -a "$LOG"
}

# --- Self-reinvoke hooks (bash 3.2 compatible; avoids export -f) --------
# xargs can't see shell functions; re-exec $0 with an internal tag instead.
# The xargs re-exec spawns a fresh bash that doesn't see the parent's
# CLI parsing — restore BD_SUFFIX/CROSS from env vars the parent exports
# before dispatch (BENCH_BD_SUFFIX/BENCH_CROSS). Without this, --simulator
# mame would set BD_SUFFIX="-mame" in the parent, but each xargs subshell
# would default to BD_SUFFIX="" and build the wrong directory
# (builddir-mc6809-Os-hd6309 instead of builddir-mc6809-Os-hd6309-mame).
case "${1:-}" in
  _setup_wave|_scrub_wave|_build_wave)
    BD_SUFFIX="${BENCH_BD_SUFFIX-}"
    CROSS="${BENCH_CROSS-$USIM_CROSS}"
    fn="$1"; shift
    "$fn" "$@"; exit $?
    ;;
esac

# --- CLI parsing --------------------------------------------------------

DEFAULT_LEVELS="\
O0,O1,O2,O3,Og,Os,Oz,Ofast,\
O2-lto,O3-lto,Os-lto,\
O0-fp,O1-fp,O2-fp,O3-fp,Og-fp,Os-fp,Oz-fp,Ofast-fp,\
Os-lto-fp,\
O2-hd6309-mame,Og-hd6309-mame,Os-hd6309-mame,Os-lto-hd6309-mame,\
O2-hd6309-mame-fp,Og-lto-hd6309-mame-fp,Os-hd6309-mame-fp,Os-lto-hd6309-mame-fp,\
os9-Os,os9-Os-fp"
# 2026-05-09 expansion: previously 12 levels.  Now 28 levels:
#   - plain non-LTO:       O0..Ofast                       (8, unchanged)
#   - plain LTO:           O2-lto, O3-lto, Os-lto          (3, +2 new)
#   - FP non-LTO:          O0-fp..Ofast-fp                 (8, +7 new + Os-fp redefined non-LTO)
#   - FP LTO:              Os-lto-fp                       (1, unchanged)
#   - HD6309 MAME:         O2/Og/Os/Os-lto-hd6309-mame     (4, +3 new — Og is debugger-friendly)
#   - HD6309 MAME + FP:    O2/Og-lto/Os/Os-lto-hd6309-mame-fp  (4, all new — Og-lto is FP-debugger)
#
# Many of the FP non-LTO and HD6309-FP levels are expected to BUILDFAIL
# (Bug #237 surfaces them in the ledger now).  The point of the
# expansion is to learn which combinations need slimming work and which
# are fundamentally too big for a 64 KB target.  HD6309+FP is a
# particular new frontier: HD6309 instructions average larger encodings
# than plain MC6809, so the FP machinery is even tighter.
levels="$DEFAULT_LEVELS"
build_jobs=6
test_jobs=6
slow=1     # Bug #288: default flipped 2026-05-12. Inputs are post-Bug-#248,
           # so test-memmem/test-memchr/strcmp-1/etc. run in seconds on USim
           # and 60-375s under MAME. Parallel pool wall premium: ~7 min on
           # a 35-min bench. Opt-out with --fast.
skip_build=0
skip_tally=0
no_preflight=0
tests_filter=""
verify_fail=1   # Bug #269: VERIFY>0 at any level fails the bench by default.
                #          --no-verify-fail keeps the old WARN-only behavior.

bench_cmdline="$0 $*"
while [ $# -gt 0 ]; do
  case "$1" in
    --levels)           levels="$2"; shift 2 ;;
    --build-jobs)       build_jobs="$2"; shift 2 ;;
    --test-jobs)        test_jobs="$2"; shift 2 ;;
    --parallel-tallies) shift ;;  # accepted for backward-compat; tally is always one flat pool now
    --slow)             slow=1; shift ;;  # Bug #288: now the default; kept for back-compat
    --fast)             slow=0; shift ;;  # Bug #288: opt out of formerly-slow tests
    --skip-build)       skip_build=1; shift ;;
    --skip-tally)       skip_tally=1; shift ;;
    --no-preflight)     no_preflight=1; shift ;;
    --no-verify-fail)   verify_fail=0; shift ;;
    --tests)            tests_filter="$2"; shift 2 ;;
    --simulator)        SIMULATOR="$2"; shift 2 ;;
    --simulator=*)      SIMULATOR="${1#--simulator=}"; shift ;;
    -h|--help)          sed -n '2,/^$/p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
    *)                  echo "unknown option: $1" >&2; exit 2 ;;
  esac
done

# Bug #194 Phase B: dispatch to MAME llvm6309 SBC. Switches the cross-
# file (so meson `exe_wrapper` resolves to `run-mc6809-mame`) and tags
# all builddirs and ledger opt_level rows with `-mame` so MAME and
# usim runs don't stomp each other's history.
case "$SIMULATOR" in
  usim)
    CROSS=$USIM_CROSS
    BD_SUFFIX=""
    ;;
  mame)
    CROSS=$MAME_CROSS
    BD_SUFFIX="-mame"
    if [ ! -x "$MAME_RUNNER" ]; then
      echo "bench-parallel: --simulator=mame but $MAME_RUNNER missing/non-executable" >&2
      exit 1
    fi
    ;;
  *)
    echo "bench-parallel: unknown --simulator value '$SIMULATOR' (use usim or mame)" >&2
    exit 2
    ;;
esac
export SIMULATOR BD_SUFFIX CROSS

IFS=',' read -ra LEVEL_LIST <<< "$levels"

# Bug #220: any *-mame level requires the MAME runner. Check once
# up-front so a missing runner errors loudly before any builds run.
for _lvl in "${LEVEL_LIST[@]}"; do
  case "$_lvl" in
    *-mame)
      if [ ! -x "$MAME_RUNNER" ]; then
        echo "bench-parallel: level '$_lvl' requires MAME runner at $MAME_RUNNER (missing/non-executable)" >&2
        exit 1
      fi
      ;;
  esac
done

# --- Guardrails ---------------------------------------------------------

# One bench at a time on this machine: two would scrub each other's build
# directories, overwrite each other's logs and interleave in the ledger.  This
# is a kernel lock, so nothing can leave it stale, and it holds for the whole
# run, build phase included.  Whatever the bench starts inherits fd 9 and so
# holds the lock with it: if the bench is killed, it stays held until the last
# worker that could still write has gone.  run-mc6809-tests --record takes the
# same lock, and recognises the one it inherits from here.
BENCH_LOCK=${MC6809_BENCH_LOCK:-/tmp/mc6809-bench.lock}
if ! exec 9>>"$BENCH_LOCK"; then
  echo "bench-parallel: cannot open $BENCH_LOCK" >&2
  exit 1
fi
python3 -c '
import errno, fcntl, sys
try:
    fcntl.flock(9, fcntl.LOCK_EX | fcntl.LOCK_NB)
except OSError as e:
    sys.exit(3 if e.errno in (errno.EWOULDBLOCK, errno.EACCES) else 1)
'
lock_rc=$?
if [ "$lock_rc" -eq 3 ]; then
  echo "bench-parallel: another bench is running; aborting" >&2
  echo "  last holder recorded in $BENCH_LOCK:" >&2
  sed 's/^/    /' "$BENCH_LOCK" >&2
  echo "  processes holding it now:" >&2
  lsof "$BENCH_LOCK" 2>/dev/null | sed 's/^/    /' >&2
  exit 1
elif [ "$lock_rc" -ne 0 ]; then
  echo "bench-parallel: cannot lock $BENCH_LOCK (python3 status $lock_rc)" >&2
  exit 1
fi
printf 'bench-parallel pid %s since %s: %s\n' "$$" "$(date '+%F %T')" "$bench_cmdline" > "$BENCH_LOCK"
export MC6809_BENCH_LOCK="$BENCH_LOCK" MC6809_BENCH_LOCK_FD=9

# WAL mode on the ledger. Safe for serial and parallel writes; harmless
# to re-apply. Needed when --parallel-tallies is on because multiple
# processes open the DB concurrently.
if [ -f "$DB" ]; then
  sqlite_bench 'PRAGMA journal_mode=WAL;' >/dev/null
fi

# --- Drive phases -------------------------------------------------------

{
  echo "==[ $(ts) bench-parallel START ]=="
  echo "  levels=$levels"
  echo "  build_jobs=$build_jobs  test_jobs=$test_jobs (single shared tally pool)"
  echo "  slow=$slow  skip_build=$skip_build  skip_tally=$skip_tally"
  echo "  db=$DB"
} | tee "$LOG"

dispatch() {
  # dispatch PAR_N FN ARGS...
  # Invokes "$0 FN arg" for each arg, with PAR_N concurrent slots via
  # xargs -P.  Uses NUL-delimited input so arbitrary whitespace is safe.
  # Set BENCH_BD_SUFFIX/BENCH_CROSS in xargs's env so the re-exec'd
  # subshells (which skip CLI parsing) recover the simulator selection.
  # xargs propagates its environment into spawned processes by default.
  local n="$1"; shift
  local fn="$1"; shift
  printf '%s\0' "$@" | \
    BENCH_BD_SUFFIX="$BD_SUFFIX" BENCH_CROSS="$CROSS" \
    xargs -0 -P "$n" -n 1 "$0" "$fn"
}

if [ $skip_build -eq 0 ]; then
  # Bug #277: scrub stale per-level state before launching the waves.
  # The per-level build logs are `>>`-appended by _setup_wave /
  # _scrub_wave / _build_wave; without truncation, the FAILED=
  # and VERIFY= counters in the build-summary block accumulate hits
  # from every past invocation of that level. False WARN tags result,
  # masking real regressions. Also remove any .rc sidecar from a
  # past run so a level that fails to even reach _build_wave (e.g.
  # unknown_level or meson setup failure) is visibly NO-SIDECAR
  # rather than inheriting yesterday's tag. The lit-tally log is in
  # the same boat.
  echo "==[ $(ts) preflight scrub stale state ]==" | tee -a "$LOG"
  for lvl in "${LEVEL_LIST[@]}"; do
    : > "$(wave_log "$lvl")"   2>/dev/null || true
    rm -f "/tmp/bench-parallel-$lvl.rc"
  done
  : > "${LOG%.log}-lit.log" 2>/dev/null || true

  # The compiler-rt builtins (libclang_rt.builtins.a: soft-float,
  # complex, long-long helpers) live in the LLVM build tree and are
  # just as blind to a compiler change as picolibc's objects: a backend
  # fix leaves them compiled by the OLD compiler and tests that link
  # them then FALSE-FAIL against the new one. Rebuild them whenever the
  # compiler is newer than the newest builtins object.
  _refresh_builtins || exit 1

  echo "==[ $(ts) setup waves ($build_jobs-way) ]==" | tee -a "$LOG"
  dispatch "$build_jobs" _setup_wave "${LEVEL_LIST[@]}"

  echo "==[ $(ts) scrub every built output (ninja -t clean) ]==" | tee -a "$LOG"
  dispatch 8 _scrub_wave "${LEVEL_LIST[@]}"

  echo "==[ $(ts) build waves ($build_jobs-way) ]==" | tee -a "$LOG"
  dispatch "$build_jobs" _build_wave "${LEVEL_LIST[@]}"
  echo "==[ $(ts) build waves done ]==" | tee -a "$LOG"

  # Per-level build summary, derived from sidecar files written by
  # _build_wave. Surfaces silent libc.a-link failures as a visible
  # WARN line at the top level, instead of leaving them only in the
  # per-wave logs. (bug #157)
  echo "==[ $(ts) build summary per level ]==" | tee -a "$LOG"
  max_bin=0
  for lvl in "${LEVEL_LIST[@]}"; do
    rcfile="/tmp/bench-parallel-$lvl.rc"
    if [ -f "$rcfile" ]; then
      read rc fn en vn < "$rcfile"
      [ "$en" -gt "$max_bin" ] 2>/dev/null && max_bin=$en
    fi
  done
  # Top-level WARN when EVERY level produced zero test binaries (bug
  # #170). Without this, the per-level "fewer than half of best" check
  # below silently produces ` ok ` for every level when max_bin=0 — a
  # confusing false negative when libc.a failed to link across all
  # levels (which is the most likely cause of zero binaries everywhere).
  if [ "$max_bin" -eq 0 ]; then
    echo "==[ $(ts) WARN: every level produced zero test binaries — libc.a likely failed to link ]==" | tee -a "$LOG"
  fi
  # Bug #269 WARN→FAIL: track whether ANY level failed the verifier gate
  # so the overall bench exit status reflects it. The build-summary tag
  # itself is FAIL (red flag in the per-level line) when:
  #  - max_bin == 0 (everything failed to build)
  #  - bins less than half the best-performing level (partial build collapse)
  #  - VERIFY > 0 AND --no-verify-fail not passed (MachineVerifier caught
  #    a lies-about-Defs/Uses regression — Bug #247/#251/#266 class).
  # When --no-verify-fail is passed, verifier hits demote to WARN
  # (informational), and the bench still exits 0. Default is the strict
  # gate.
  failed_levels=""
  for lvl in "${LEVEL_LIST[@]}"; do
    rcfile="/tmp/bench-parallel-$lvl.rc"
    if [ ! -f "$rcfile" ]; then
      echo "  $lvl: NO SIDECAR (wave didn't run?)" | tee -a "$LOG"
      continue
    fi
    read rc fn en vn < "$rcfile"
    if [ "$max_bin" -eq 0 ]; then
      tag="FAIL"
      _gate_fail "$lvl built no binaries"
      failed_levels="${failed_levels:+$failed_levels }$lvl(no-binaries)"
    elif [ "$en" -lt $(( max_bin / 2 )) ]; then
      tag="FAIL"
      _gate_fail "$lvl built under half the binaries"
      failed_levels="${failed_levels:+$failed_levels }$lvl(partial-build)"
    elif [ "${vn:-0}" -gt 0 ] 2>/dev/null; then
      if [ "$verify_fail" -eq 1 ]; then
        tag="FAIL"
        _gate_fail "$lvl had $vn verifier hits"
        failed_levels="${failed_levels:+$failed_levels }$lvl(verify=${vn})"
      else
        tag="WARN"
      fi
    else
      tag=" ok "
    fi
    echo "  $lvl: $tag rc=$rc FAILED=$fn bins=$en/$max_bin VERIFY=${vn:-0}" | tee -a "$LOG"
  done
  if [ "$bench_failed" -eq 1 ]; then
    echo "==[ $(ts) bench-parallel: BUILD GATE FAILED at: $failed_levels ]==" | tee -a "$LOG"
    if [ "$verify_fail" -eq 1 ] && echo "$failed_levels" | grep -q 'verify='; then
      echo "  (pass --no-verify-fail to demote verifier-hit FAILs to WARN)" | tee -a "$LOG"
    fi
  fi
fi

if [ $skip_tally -eq 0 ]; then
  # Lit gate first (cheap; records its own runs row with opt_level='lit'
  # so a broken lit suite is captured in the ledger before the long
  # picolibc tally runs).
  _lit_tally

  # Everything recorded after this belongs to this bench's tally.
  tally_after_run_id=$(sqlite_bench "SELECT COALESCE(MAX(run_id), 0) FROM runs;")

  # Build "LEVEL:DIR,LEVEL:DIR,…" for run-mc6809-tests --multi.
  # Bug #194 Phase B: when --simulator=mame, tag the LEVEL with
  # `-mame` so the bench ledger records MAME runs as e.g. `Os-mame`,
  # `O0-mame`, ... — distinct from the canonical usim history.
  multi=""
  for lvl in "${LEVEL_LIST[@]}"; do
    bd="$(wave_bd "$lvl")"
    if [ ! -f "$bd/build.ninja" ]; then
      echo "==[ $(ts) skip $lvl: $bd not configured (no build.ninja) ]==" \
        | tee -a "$LOG"
      continue
    fi
    # Bug #220: per-level suffix so *-mame levels record under their
    # own opt_level label without double-stamping the -mame.
    # Record label is backend-aware (level_record_label): -hd6309-mame levels
    # executed on USim (the default) record as -hd6309-usim, keeping the two
    # backends' incomparable cycle series apart. The builddir ($bd) is unchanged.
    lvl_suffix="$(level_bd_suffix "$lvl")"
    rec_label="$(level_record_label "$lvl")"
    case "$lvl" in
      # Each OS-9 level boots a different NitrOS-9, and the level it wants
      # is an environment variable -- one shared pool cannot hold two of
      # them, so they are tallied one at a time below.
      os9-*) os9_levels="${os9_levels:+$os9_levels }${lvl}:${bd}" ;;
      *)     multi="${multi:+$multi,}${rec_label}${lvl_suffix}:${bd}" ;;
    esac
  done
  if [ -z "$multi" ]; then
    echo "==[ $(ts) no tally levels available ]==" | tee -a "$LOG"
  else
    slow_arg="";   [ $slow         -eq 1 ] && slow_arg="--slow"
    np_arg="";     [ $no_preflight -eq 1 ] && np_arg="--no-preflight"
    tests_arg="";  [ -n "$tests_filter" ]   && tests_arg="--tests $tests_filter"
    echo "==[ $(ts) tally SHARED POOL (--jobs $test_jobs over $multi${tests_filter:+ tests=$tests_filter}) ]==" \
      | tee -a "$LOG"
    MC6809_BENCH_DB="$DB" \
      "$PICO/scripts/run-mc6809-tests" \
        --record --jobs "$test_jobs" --multi "$multi" \
        $slow_arg $np_arg $tests_arg 2>&1 | tee -a "$LOG"
    harness_rc=${PIPESTATUS[0]}
    [ "$harness_rc" -eq 0 ] || _gate_fail "the tally harness exited $harness_rc"
    echo "==[ $(ts) tally pool done ]==" | tee -a "$LOG"
  fi

  # OS-9 levels, one at a time.  --include-fake-fs because the tests those
  # cover need a real filesystem, which is exactly what OS-9 has and the
  # semihost layer does not.  No cycles: a NitrOS-9 boot is most of what a
  # run costs, so a cycle count here would be measuring the operating
  # system, not the program (see the note in run-mc6809-tests).
  for entry in ${os9_levels:-}; do
    lvl="${entry%%:*}"; bd="${entry#*:}"
    case "$lvl" in
      os9-Os) n9level=1 ;;
      *)      n9level=2 ;;
    esac
    tests_arg="";  [ -n "$tests_filter" ] && tests_arg="--tests $tests_filter"
    echo "==[ $(ts) tally $lvl (NitrOS-9 Level $n9level, --jobs $test_jobs) ]==" \
      | tee -a "$LOG"
    MC6809_BENCH_DB="$DB" NITROS9_LEVEL="$n9level" PICOLIBC_BUILDDIR="$bd" \
      "$PICO/scripts/run-mc6809-tests" \
        --record --jobs "$test_jobs" --opt-level "$lvl" --include-fake-fs \
        $tests_arg 2>&1 | tee -a "$LOG"
    harness_rc=${PIPESTATUS[0]}
    [ "$harness_rc" -eq 0 ] || _gate_fail "the $lvl tally harness exited $harness_rc"
  done
fi

echo "==[ $(ts) bench-parallel END ]==" | tee -a "$LOG"

# Disambiguate the cross-O summary's "0 0 0 0 0 0" rows. With #144
# closed, an Og row of all zeros is no longer the expected build-wall
# state — it now means the build silently failed and the tally pool
# found no test binaries for that level. Fish the offending levels out
# of the ledger and emit a visible WARN before the table prints. (bug #157)
zero_levels=""
for lvl in "${LEVEL_LIST[@]}"; do
  count=$(sqlite_bench "
    SELECT COUNT(*) FROM results
     WHERE run_id = (SELECT MAX(run_id) FROM runs WHERE opt_level='$lvl');
  " 2>/dev/null)
  if [ "${count:-0}" = "0" ]; then
    zero_levels="${zero_levels:+$zero_levels }$lvl"
  fi
done
if [ -n "$zero_levels" ]; then
  echo "==[ $(ts) WARN: zero-test levels — likely silent build failure ]==" | tee -a "$LOG"
  for z in $zero_levels; do
    echo "  $z: tally returned 0 results — see $(wave_log $z) for build errors" \
      | tee -a "$LOG"
  done
fi

# Bug #279: delta-vs-previous-run summary table.
# For each opt_level that THIS bench touched, fetch counts for the
# most-recent run (= this bench's tally) AND the immediately-prior run,
# then show absolute counts + parenthesised deltas. Emits REGRESSION
# WARN when any |d_ok| >= 5 or d_fail > 0 — surfaces silent regressions
# that the absolute-only summary masks (e.g. today's Og-fp 89→0).
# Quote each level for embedding into a SQL IN(...) clause -- under the
# name the harness records it by, as the gates below do. A 6309 level asked
# for as *-hd6309-mame is recorded as *-hd6309-usim unless the MAME backend
# was chosen; selecting the requested name instead picked up the last rows
# ever written under it (months old) and showed them as this run's.
this_run_levels=""
for lvl in "${LEVEL_LIST[@]}"; do
  rec="$(level_record_label "$lvl")$(level_bd_suffix "$lvl")"
  this_run_levels="${this_run_levels:+$this_run_levels,}'$rec'"
done
sqlite3 -cmd ".timeout $SQLITE_BUSY_TIMEOUT_MS" -separator $'\t' "$DB" <<SQL > "${LOG%.log}-delta.tsv" 2>/dev/null
WITH ranked AS (
  SELECT opt_level, run_id,
         ROW_NUMBER() OVER (PARTITION BY opt_level ORDER BY run_id DESC) AS rn
    FROM runs
   WHERE opt_level IN ($this_run_levels)
),
curr_r AS (SELECT opt_level, run_id FROM ranked WHERE rn = 1),
prev_r AS (SELECT opt_level, run_id FROM ranked WHERE rn = 2),
agg AS (
  SELECT run_id,
         SUM(status='OK')           AS ok,
         SUM(status='EXPECTEDFAIL') AS xfail,
         SUM(status='SKIP')         AS skip,
         SUM(status='FAIL')         AS fail,
         SUM(status='TIMEOUT')      AS tout,
         SUM(status='BUILDFAIL')    AS bfail,
         COUNT(*)                   AS total,
         COALESCE(SUM(cycles), 0)   AS cyc
    FROM results
   GROUP BY run_id
)
SELECT curr_r.opt_level,
       cc.ok,    (cc.ok    - COALESCE(pc.ok,    cc.ok)),
       cc.xfail, (cc.xfail - COALESCE(pc.xfail, cc.xfail)),
       cc.skip,  (cc.skip  - COALESCE(pc.skip,  cc.skip)),
       cc.fail,  (cc.fail  - COALESCE(pc.fail,  cc.fail)),
       cc.tout,  (cc.tout  - COALESCE(pc.tout,  cc.tout)),
       cc.bfail, (cc.bfail - COALESCE(pc.bfail, cc.bfail)),
       cc.total, (cc.total - COALESCE(pc.total, cc.total)),
       cc.cyc,   (cc.cyc   - COALESCE(pc.cyc,   cc.cyc)),
       CASE WHEN prev_r.run_id IS NULL THEN 'first' ELSE 'delta' END
  FROM curr_r
  JOIN agg cc ON cc.run_id = curr_r.run_id
  LEFT JOIN prev_r ON prev_r.opt_level = curr_r.opt_level
  LEFT JOIN agg pc ON pc.run_id = prev_r.run_id;
SQL

# Format per-level lines, accumulating a REGRESSION WARN flag.
awk -F'\t' '
function dpart(n,    s) {
  if (n == 0)           return "";
  if (n > 0)            return "(+" n ")";
                        return "(" n ")";   # already has minus
}
function metric(name, val, d) {
  if (val == 0 && d == 0) return "";   # uninteresting
  if (d == 0)             return sprintf("%s=%d", name, val);
                          return sprintf("%s=%d %s", name, val, dpart(d));
}
{
  lvl=$1; ok=$2; d_ok=$3; xf=$4; d_xf=$5; sk=$6; d_sk=$7;
  fl=$8; d_fl=$9; to=$10; d_to=$11; bf=$12; d_bf=$13;
  tot=$14; d_tot=$15; cyc=$16; d_cyc=$17; tag=$18;

  parts="";
  for (i in arr) delete arr[i];
  arr[1]=metric("OK",    ok, d_ok);
  arr[2]=metric("XFAIL", xf, d_xf);
  arr[3]=metric("SKIP",  sk, d_sk);
  arr[4]=metric("FAIL",  fl, d_fl);
  arr[5]=metric("TOUT",  to, d_to);
  arr[6]=metric("BFAIL", bf, d_bf);
  for (i=1; i<=6; i++) if (arr[i] != "") parts = parts (parts != "" ? "  " : "") arr[i];
  printf("  [%-23s] %s  TOTAL=%d%s\n", lvl, parts, tot,
         (d_tot != 0 ? "  " dpart(d_tot) : ""));
  if (tag == "delta") {
    if (d_ok <= -5 || d_ok >= 5 || d_fl > 0) regress[lvl] = sprintf("d_ok=%d d_fail=%d", d_ok, d_fl);
  }
}
END {
  if (length(regress) > 0) {
    print "==[ REGRESSION WARN: levels with |d_ok|>=5 or new FAILs ]==";
    for (l in regress) print "  " l ": " regress[l];
  } else if (NR > 0) {
    print "==[ no regression vs previous run ]==";
  }
}
' "${LOG%.log}-delta.tsv" | tee -a "$LOG"
rm -f "${LOG%.log}-delta.tsv"

# A level whose latest run has results but not a single OK is a level
# whose libc.a (or every test link) failed -- BUILDFAIL rows are not
# FAILs, so the delta table's "new FAILs" test never fires, and the next
# sweep's delta normalises to the broken state. Two whole levels sat at
# 0 OK for six weeks that way. Gate on it, and on an OK count that has
# collapsed to less than half of the previous run of the same level.
# Skipped when --tests filters the pool: a handful of tests against a
# full previous run is not comparable.
if [ $skip_tally -eq 0 ] && [ -z "$tests_filter" ]; then
  for lvl in "${LEVEL_LIST[@]}"; do
    rec="$(level_record_label "$lvl")$(level_bd_suffix "$lvl")"
    read -r ok_now ok_prev tot_now <<<"$(sqlite_bench "
      WITH r AS (SELECT run_id, ROW_NUMBER() OVER (ORDER BY run_id DESC) rn
                   FROM runs WHERE opt_level='$rec')
      SELECT (SELECT COUNT(*) FROM results WHERE run_id=(SELECT run_id FROM r WHERE rn=1) AND status='OK'),
             (SELECT COUNT(*) FROM results WHERE run_id=(SELECT run_id FROM r WHERE rn=2) AND status='OK'),
             (SELECT COUNT(*) FROM results WHERE run_id=(SELECT run_id FROM r WHERE rn=1));
    " 2>/dev/null | tr '|' ' ')"
    ok_now=${ok_now:-0}; ok_prev=${ok_prev:-0}; tot_now=${tot_now:-0}
    if [ "$tot_now" -gt 0 ] && [ "$ok_now" -eq 0 ]; then
      echo "==[ $(ts) FAIL: $rec produced ZERO OK results out of $tot_now -- libc.a or every test link is broken at this level ]==" | tee -a "$LOG"
      _gate_fail "$rec produced no OK results"
    elif [ "$ok_prev" -gt 0 ] && [ $(( ok_now * 2 )) -lt "$ok_prev" ]; then
      echo "==[ $(ts) FAIL: $rec OK count collapsed $ok_prev -> $ok_now vs the previous run of this level ]==" | tee -a "$LOG"
      _gate_fail "$rec OK count collapsed $ok_prev -> $ok_now"
    fi
  done
fi

# Cross-O wide table at the end for at-a-glance comparison
# (covers ALL 28 levels, not just the 9 plain ones).
sqlite_bench <<SQL | tee -a "$LOG"
.headers on
.mode column
SELECT opt_level,
       SUM(CASE WHEN status='OK'           THEN 1 ELSE 0 END) AS ok,
       SUM(CASE WHEN status='EXPECTEDFAIL' THEN 1 ELSE 0 END) AS xfail,
       SUM(CASE WHEN status='SKIP'         THEN 1 ELSE 0 END) AS skip,
       SUM(CASE WHEN status='FAIL'         THEN 1 ELSE 0 END) AS fail,
       SUM(CASE WHEN status='TIMEOUT'      THEN 1 ELSE 0 END) AS tout,
       SUM(CASE WHEN status='BUILDFAIL'    THEN 1 ELSE 0 END) AS bfail,
       SUM(cycles)                                            AS total_cycles
  FROM results
 WHERE opt_level IN ($this_run_levels)
   AND run_id IN (
     SELECT MAX(run_id) FROM runs
      WHERE opt_level IN ($this_run_levels)
      GROUP BY opt_level
   )
 GROUP BY opt_level
 ORDER BY opt_level;
SQL

# A test that did not pass fails the bench, at any level and whether or not it
# also failed last time: comparing with the previous run would let a failure
# through for as long as it stays broken.  This also applies under --tests,
# which is what a quick check of one test is asking.
if [ $skip_tally -eq 0 ]; then
  if ! not_passed=$(sqlite_bench -separator $'\t' "
      SELECT r.opt_level, res.status, COUNT(*), GROUP_CONCAT(res.test_name, ' ')
        FROM results res JOIN runs r USING (run_id)
       WHERE res.run_id > ${tally_after_run_id:-0}
         AND res.status IN ('FAIL', 'TIMEOUT', 'ERROR', 'UNEXPECTEDPASS')
       GROUP BY r.opt_level, res.status
       ORDER BY r.opt_level, res.status;"); then
    _gate_fail "could not read this tally's results from the ledger"
  fi
  while IFS=$'\t' read -r np_level np_status np_count np_tests; do
    [ -n "$np_level" ] || continue
    echo "==[ $(ts) FAIL: $np_level: $np_count $np_status: $np_tests ]==" | tee -a "$LOG"
    _gate_fail "$np_level had $np_count $np_status"
  done <<<"$not_passed"
fi

# Surface the failed gates in the exit status so CI / wrappers see the
# bench as failed. --no-verify-fail demotes verifier hits to WARN only (build-collapse FAILs
# still gate).
if [ "${bench_failed:-0}" -eq 1 ]; then
  echo "==[ $(ts) bench-parallel: GATE FAILED: $gate_failures ]==" | tee -a "$LOG"
  exit 1
fi
exit 0
