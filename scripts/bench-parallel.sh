#!/bin/bash
# bench-parallel.sh — MC6809 parallel-build + parallel-tally driver.
#
# Build phase, per -O level (parallel via xargs -P):
#   1. meson setup builddir-mc6809-<O>  (once, reuses on re-run)
#   2. delete stale *.c.o               (ninja is blind to compiler mtime)
#   3. ninja -k 0 -C ...                (parallel across waves)
#
# Lit phase, once per invocation (cheap, ~1s):
#   3.5 llvm-lit -v test/CodeGen/MC6809/ → one runs row with
#       opt_level='lit' + one results row per lit test.
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
#   --slow               include SLOW_TESTS (test-memmem, ~25 min wall)
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

set -u

PICO=/Users/markmurray/GitHub/picolibc
LLVM_BIN=/Users/markmurray/GitHub/llvm-mc6809/llvm/cmake-build-debug-system/bin
LLVM_REPO=/Users/markmurray/GitHub/llvm-mc6809
USIM=/Users/markmurray/GitHub/usim
DB=${MC6809_BENCH_DB:-$HOME/Documents/mc6809-bench/results.sqlite}
LOG=/tmp/bench-parallel.log

# Bug #194 Phase B: simulator backend selector. Default is usim
# (canonical for 6809). MAME's `llvm6309` SBC is the HD6309-capable
# alternative; identical memory map, different exe_wrapper. Selected
# via `--simulator=mame` on the CLI; bench rows are tagged with
# `-mame` suffixed opt_level (e.g. `Os-mame`) for easy DB filtering
# and to avoid stomping the canonical usim history.
SIMULATOR="usim"
USIM_CROSS=$PICO/scripts/cross-clang-mc6809-unknown-elf.txt
MAME_CROSS=$PICO/scripts/cross-clang-mc6809-unknown-elf-mame.txt
CROSS=$USIM_CROSS
BD_SUFFIX=""

# Cross-platform mtime helper. macOS uses `stat -f %m`, Linux `stat -c %Y`.
file_mtime() {
  stat -f %m "$1" 2>/dev/null || stat -c %Y "$1" 2>/dev/null || echo ""
}

# Bug #191: bench typically launches BEFORE the user commits the
# changes that produced the binaries — so plain `git rev-parse HEAD`
# records the previous commit, not the binary's actual provenance.
# We can't synthesise a SHA for uncommitted edits, but we CAN flag
# the discrepancy two ways:
#   (1) tag the recorded SHA with "+dirty" if the source tree has
#       uncommitted changes RIGHT NOW, or if the binary's mtime is
#       later than HEAD's commit time (indicating a build-then-
#       commit window we haven't crossed yet).
#   (2) Capture the binary mtime separately so a future analyst can
#       correlate to the actual commit (the one immediately AFTER
#       the recorded SHA in chronological order).
get_llvm_commit() {
  local sha
  sha=$(cd "$LLVM_REPO" && git rev-parse HEAD 2>/dev/null || echo unknown)
  local dirty=""
  # (a) Compare binary mtime to HEAD commit time.
  if [ -x "$LLVM_BIN/llc" ]; then
    local bin_mtime head_ctime
    bin_mtime=$(file_mtime "$LLVM_BIN/llc")
    head_ctime=$(cd "$LLVM_REPO" && git log -n1 --format=%ct HEAD 2>/dev/null)
    if [ -n "$bin_mtime" ] && [ -n "$head_ctime" ] && \
       [ "$bin_mtime" -gt "$head_ctime" ]; then
      dirty="+dirty"
    fi
  fi
  # (b) Tree has uncommitted changes RIGHT NOW.
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
  -Dprintf-aliases=false
  -Dstdio-float=false
  -Dposix-extensions=false
  -Dsearch-extensions=true
  -Dwant-libm=false
  -Dmb-capable=false
  -Dio-long-long=false
  -Dtests=true
  -Dtests-enable-stack-protector=false
)

level_opts() {
  # Levels without a native meson mapping (z, fast) go through
  # optimization=plain + an explicit -O flag injected into c/cpp_args.
  case "$1" in
    O0)    echo "-Doptimization=0" ;;
    O1)    echo "-Doptimization=1" ;;
    O2)    echo "-Doptimization=2" ;;
    O3)    echo "-Doptimization=3" ;;
    Os)    echo "-Doptimization=s" ;;
    Og)    echo "-Doptimization=g" ;;
    Oz)     echo "-Doptimization=plain -Dc_args=-Oz -Dcpp_args=-Oz" ;;
    Ofast)  echo "-Doptimization=plain -Dc_args=-Ofast -Dcpp_args=-Ofast" ;;
    Os-lto) echo "-Doptimization=s -Db_lto=true" ;;
    Os-hd6309) echo "-Doptimization=s -Dc_args=-mcpu=hd6309 -Dcpp_args=-mcpu=hd6309 -Dc_link_args=-mcpu=hd6309 -Dcpp_link_args=-mcpu=hd6309" ;;
    Os-hd6309-mame) echo "-Doptimization=s -Dc_args=-mcpu=hd6309 -Dcpp_args=-mcpu=hd6309 -Dc_link_args=-mcpu=hd6309 -Dcpp_link_args=-mcpu=hd6309" ;;
    *)     echo "unknown level $1" >&2; return 2 ;;
  esac
}

# Bug #220: per-level cross-file selection. Levels whose name ends in
# `-mame` use the MAME cross-file (exe_wrapper = run-mc6809-mame); all
# others use the global $CROSS (controlled by --simulator). This lets
# `Os-hd6309-mame` be a first-class level in DEFAULT_LEVELS alongside
# the usim levels — a single sweep covers both simulator backends.
level_cross() {
  case "$1" in
    *-mame) echo "$MAME_CROSS" ;;
    *)      echo "$CROSS" ;;
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
    *-mame) echo "" ;;  # already in label
    *)      echo "$BD_SUFFIX" ;;
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

_setup_wave() {
  local label="$1"
  local bd wl opts cross
  bd="$(wave_bd "$label")"
  wl="$(wave_log "$label")"
  opts="$(level_opts "$label")" || return 2
  cross="$(level_cross "$label")"
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
      && meson configure "$bd" $opts ) >>"$wl" 2>&1
  fi
}

_clean_stale_c_o() {
  local label="$1"
  local bd
  bd="$(wave_bd "$label")"
  find "$bd" \( -name '*.c.o' -o -name '*.cpp.o' \) -delete 2>/dev/null
  # ninja is blind to compiler-binary mtime. Without this, a rebuilt
  # clang/llc with unchanged picolibc sources leaves stale .c.o / .cpp.o
  # on disk — the tally then records the OLD codegen against the NEW SHA.
  # (feedback_ninja_compiler_dep.md, bug #169)
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
  local failed_n bin_n
  failed_n=$(grep -c '^FAILED: ' "$wl" 2>/dev/null || echo 0)
  # Count picolibc test binaries (bug #170): they live under $bd/test,
  # have no `.elf` extension, and are owner-executable. The previous
  # `find -name '*.elf'` returned 0 unconditionally because picolibc
  # doesn't suffix its test executables. `-perm -u+x` is POSIX (BSD
  # find on macOS plus GNU find both accept it). The `*.p/*` exclusion
  # skips meson's intermediate object directories.
  bin_n=$(find "$bd/test" -type f -perm -u+x -not -path '*.p/*' \
                2>/dev/null | wc -l | tr -d ' ')
  printf '%s\t%s\t%s\n' "$rc" "$failed_n" "$bin_n" > "/tmp/bench-parallel-$label.rc"
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
  #   clang/test/CodeGen/MC6809/         -- clang CodeGen tests (builtins)
  #   clang/test/Sema/asm-mc6809-*.c     -- inline asm constraint Sema tests
  #   clang/test/Sema/mc6809-*.c         -- target builtin Sema tests
  #   clang/test/Sema/mc6809-directpage.c
  local lit="$LLVM_BIN/llvm-lit"
  local llvm_root="$(cd "$LLVM_BIN/../.." && pwd)"
  local clang_root="$(cd "$llvm_root/../clang" && pwd)"
  local outfile="/tmp/bench-parallel-lit.log"

  echo "==[ $(ts) lit tally start ]==" | tee -a "$LOG"

  if [ ! -x "$lit" ] || [ ! -d "$llvm_root/test/CodeGen/MC6809" ]; then
    echo "  lit: skipped (binary or testdir missing)" | tee -a "$LOG"
    return 0
  fi

  # Build the test list: always the llvm backend tests; add clang tests if present.
  local testpaths="$llvm_root/test/CodeGen/MC6809"
  if [ -d "$clang_root/test/CodeGen/MC6809" ]; then
    testpaths="$testpaths $clang_root/test/CodeGen/MC6809"
  fi
  for f in "$clang_root"/test/Sema/asm-mc6809-*.c \
            "$clang_root"/test/Sema/mc6809-*.c; do
    [ -f "$f" ] && testpaths="$testpaths $f"
  done

  # shellcheck disable=SC2086
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
  sqlite3 "$DB" "ALTER TABLE runs ADD COLUMN llvm_binary_mtime INTEGER;" 2>/dev/null

  local run_id
  run_id=$(sqlite3 "$DB" "
    INSERT INTO runs (timestamp, llvm_commit, picolibc_commit, usim_commit, host, opt_level, llvm_binary_mtime)
    VALUES ('$timestamp', '$commit', '$picolibc_commit', '$usim_commit', '$host', 'lit', ${bin_mtime:-NULL});
    SELECT last_insert_rowid();
  ")

  # Parse lit verbose output; emit one INSERT per test.
  awk -v rid="$run_id" '
    /^(PASS|FAIL|XFAIL|UNRESOLVED|UNSUPPORTED|XPASS|TIMEOUT|FLAKY): / {
      status=$1; sub(/:$/,"",status)
      path=$4
      n=split(path,parts,"/")
      tn=parts[n]
      printf "INSERT INTO results (run_id, test_name, suite, status, opt_level) VALUES (%s, '\''%s'\'', '\''lit-codegen'\'', '\''%s'\'', '\''lit'\'');\n", rid, tn, status
    }
  ' "$outfile" | sqlite3 "$DB"

  local pass fail xfail unsupp unres
  pass=$(grep -c   '^PASS: '        "$outfile" 2>/dev/null)
  fail=$(grep -c   '^FAIL: '        "$outfile" 2>/dev/null)
  xfail=$(grep -c  '^XFAIL: '       "$outfile" 2>/dev/null)
  unsupp=$(grep -c '^UNSUPPORTED: ' "$outfile" 2>/dev/null)
  unres=$(grep -c  '^UNRESOLVED: '  "$outfile" 2>/dev/null)

  local tag=" ok "
  [ "${fail:-0}"  -gt 0 ] 2>/dev/null && tag="FAIL"
  [ "${unres:-0}" -gt 0 ] 2>/dev/null && tag="UNRESOLVED"

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
  _setup_wave|_clean_stale_c_o|_build_wave)
    BD_SUFFIX="${BENCH_BD_SUFFIX-}"
    CROSS="${BENCH_CROSS-$USIM_CROSS}"
    fn="$1"; shift
    "$fn" "$@"; exit $?
    ;;
esac

# --- CLI parsing --------------------------------------------------------

DEFAULT_LEVELS="O0,O1,O2,O3,Og,Os,Oz,Ofast,Os-lto,Os-hd6309-mame"
levels="$DEFAULT_LEVELS"
build_jobs=6
test_jobs=6
slow=0
skip_build=0
skip_tally=0
no_preflight=0
tests_filter=""

while [ $# -gt 0 ]; do
  case "$1" in
    --levels)           levels="$2"; shift 2 ;;
    --build-jobs)       build_jobs="$2"; shift 2 ;;
    --test-jobs)        test_jobs="$2"; shift 2 ;;
    --parallel-tallies) shift ;;  # accepted for backward-compat; tally is always one flat pool now
    --slow)             slow=1; shift ;;
    --skip-build)       skip_build=1; shift ;;
    --skip-tally)       skip_tally=1; shift ;;
    --no-preflight)     no_preflight=1; shift ;;
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
    if [ ! -x "$HOME/GitHub/mame/run-mc6809-mame" ]; then
      echo "bench-parallel: --simulator=mame but $HOME/GitHub/mame/run-mc6809-mame missing/non-executable" >&2
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
      if [ ! -x "$HOME/GitHub/mame/run-mc6809-mame" ]; then
        echo "bench-parallel: level '$_lvl' requires MAME runner at $HOME/GitHub/mame/run-mc6809-mame (missing/non-executable)" >&2
        exit 1
      fi
      ;;
  esac
done

# --- Guardrails ---------------------------------------------------------

# Refuse to start if another tally is already running — two concurrent
# meson-test processes against the same builddir will trip over each
# other's meson-logs/. (Cross-builddir concurrency is fine; that's the
# whole point of this driver.)
if pgrep -f "run-mc6809-tests|usim09batch" >/dev/null 2>&1; then
  echo "bench-parallel: another mc6809 test process is running; aborting" >&2
  pgrep -af "run-mc6809-tests|usim09batch" >&2
  exit 1
fi

# WAL mode on the ledger. Safe for serial and parallel writes; harmless
# to re-apply. Needed when --parallel-tallies is on because multiple
# processes open the DB concurrently.
if [ -f "$DB" ]; then
  sqlite3 "$DB" 'PRAGMA journal_mode=WAL;' >/dev/null
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
  echo "==[ $(ts) setup waves ($build_jobs-way) ]==" | tee -a "$LOG"
  dispatch "$build_jobs" _setup_wave "${LEVEL_LIST[@]}"

  echo "==[ $(ts) delete stale *.c.o ]==" | tee -a "$LOG"
  dispatch 8 _clean_stale_c_o "${LEVEL_LIST[@]}"

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
      read rc fn en < "$rcfile"
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
  for lvl in "${LEVEL_LIST[@]}"; do
    rcfile="/tmp/bench-parallel-$lvl.rc"
    if [ ! -f "$rcfile" ]; then
      echo "  $lvl: NO SIDECAR (wave didn't run?)" | tee -a "$LOG"
      continue
    fi
    read rc fn en < "$rcfile"
    # WARN when this level produced zero binaries OR substantially fewer
    # binaries than the best-performing level — both are strong signals
    # that libc.a (or one of its objects) failed to link. The "less than
    # half" threshold catches per-level partial failures; the max_bin==0
    # arm catches the case where all levels failed identically (was the
    # bug #170 false negative).
    if [ "$max_bin" -eq 0 ]; then
      tag="WARN"
    elif [ "$en" -lt $(( max_bin / 2 )) ]; then
      tag="WARN"
    else
      tag=" ok "
    fi
    echo "  $lvl: $tag rc=$rc FAILED=$fn bins=$en/$max_bin" | tee -a "$LOG"
  done
fi

if [ $skip_tally -eq 0 ]; then
  # Lit gate first (cheap; records its own runs row with opt_level='lit'
  # so a broken lit suite is captured in the ledger before the long
  # picolibc tally runs).
  _lit_tally

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
    lvl_suffix="$(level_bd_suffix "$lvl")"
    multi="${multi:+$multi,}${lvl}${lvl_suffix}:${bd}"
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
    echo "==[ $(ts) tally pool done ]==" | tee -a "$LOG"
  fi
fi

echo "==[ $(ts) bench-parallel END ]==" | tee -a "$LOG"

# Disambiguate the cross-O summary's "0 0 0 0 0 0" rows. With #144
# closed, an Og row of all zeros is no longer the expected build-wall
# state — it now means the build silently failed and the tally pool
# found no test binaries for that level. Fish the offending levels out
# of the ledger and emit a visible WARN before the table prints. (bug #157)
zero_levels=""
for lvl in "${LEVEL_LIST[@]}"; do
  count=$(sqlite3 "$DB" "
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

# Cross-O summary of the most recent recorded run per opt_level.
sqlite3 "$DB" <<SQL | tee -a "$LOG"
.headers on
.mode column
SELECT opt_level,
       SUM(CASE WHEN status='OK'           THEN 1 ELSE 0 END) AS ok,
       SUM(CASE WHEN status='EXPECTEDFAIL' THEN 1 ELSE 0 END) AS xfail,
       SUM(CASE WHEN status='SKIP'         THEN 1 ELSE 0 END) AS skip,
       SUM(CASE WHEN status='FAIL'         THEN 1 ELSE 0 END) AS fail,
       SUM(CASE WHEN status='TIMEOUT'      THEN 1 ELSE 0 END) AS tout,
       SUM(cycles)                                            AS total_cycles
  FROM results
 WHERE opt_level IN ('O0','O1','O2','O3','Os','Og','Oz','Ofast','Os-lto')
   AND run_id IN (
     SELECT MAX(run_id) FROM runs
      WHERE opt_level IS NOT NULL
      GROUP BY opt_level
   )
 GROUP BY opt_level
 ORDER BY CASE opt_level
            WHEN 'O0' THEN 1 WHEN 'O1' THEN 2 WHEN 'O2' THEN 3
            WHEN 'O3' THEN 4 WHEN 'Og' THEN 5 WHEN 'Os' THEN 6
            WHEN 'Oz' THEN 7 WHEN 'Ofast' THEN 8 WHEN 'Os-lto' THEN 9 END;
SQL