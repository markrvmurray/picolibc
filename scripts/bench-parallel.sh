#!/bin/bash
# bench-parallel.sh — MC6809 parallel-build + parallel-tally driver.
#
# Build phase, per -O level (parallel via xargs -P):
#   1. meson setup builddir-mc6809-<O>  (once, reuses on re-run)
#   2. delete stale *.c.o               (ninja is blind to compiler mtime)
#   3. ninja -k 0 -C ...                (parallel across waves)
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
#   --levels LIST        comma-sep subset (default: O0,O1,O2,O3,Og,Os,Oz,Ofast)
#   --build-jobs N       parallel waves during build phase (default: 6)
#   --test-jobs N        workers in the shared tally pool (default: 6)
#   --slow               include SLOW_TESTS (test-memmem, ~25 min wall)
#   --skip-build         reuse existing binaries (tally-only rerun)
#   --skip-tally         build phase only
#   --no-preflight       pass through to run-mc6809-tests --multi
#   -h, --help           show this header

set -u

PICO=/Users/markmurray/GitHub/picolibc
LLVM_BIN=/Users/markmurray/GitHub/llvm-mc6809/llvm/cmake-build-debug-system/bin
USIM=/Users/markmurray/GitHub/usim
CROSS=$PICO/scripts/cross-clang-mc6809-unknown-elf.txt
DB=${MC6809_BENCH_DB:-$HOME/Documents/mc6809-bench/results.sqlite}
LOG=/tmp/bench-parallel.log

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
  --cross-file=$CROSS
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
    Oz)    echo "-Doptimization=plain -Dc_args=-Oz -Dcpp_args=-Oz" ;;
    Ofast) echo "-Doptimization=plain -Dc_args=-Ofast -Dcpp_args=-Ofast" ;;
    *)     echo "unknown level $1" >&2; return 2 ;;
  esac
}

# --- Per-wave phase implementations (also invoked via self-reinvoke) ----

wave_log() { echo "/tmp/bench-parallel-$1.log"; }
wave_bd()  { echo "$PICO/builddir-mc6809-$1"; }
ts()       { date '+%H:%M:%S'; }

_setup_wave() {
  local label="$1"
  local bd wl opts
  bd="$(wave_bd "$label")"
  wl="$(wave_log "$label")"
  opts="$(level_opts "$label")" || return 2
  echo "==[ $(ts) $label setup ]== bd=$bd opts=$opts" >>"$wl"
  if [ ! -f "$bd/build.ninja" ]; then
    # shellcheck disable=SC2086
    ( cd "$PICO" && meson setup "$bd" "${COMMON[@]}" $opts ) >>"$wl" 2>&1
  else
    # shellcheck disable=SC2086
    ( cd "$PICO" && meson configure "$bd" $opts ) >>"$wl" 2>&1
  fi
}

_clean_stale_c_o() {
  local label="$1"
  local bd
  bd="$(wave_bd "$label")"
  find "$bd" -name '*.c.o' -delete 2>/dev/null
  # ninja is blind to compiler-binary mtime. Without this, a rebuilt
  # clang/llc with unchanged picolibc sources leaves stale .c.o on disk
  # — the tally then records the OLD codegen against the NEW SHA.
  # (feedback_ninja_compiler_dep.md)
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
}

# Tally phase is now a single flat invocation of
# run-mc6809-tests --multi (see _tally_multi below). No per-level
# self-reinvoke needed.

# --- Self-reinvoke hooks (bash 3.2 compatible; avoids export -f) --------
# xargs can't see shell functions; re-exec $0 with an internal tag instead.
case "${1:-}" in
  _setup_wave|_clean_stale_c_o|_build_wave)
    fn="$1"; shift
    "$fn" "$@"; exit $?
    ;;
esac

# --- CLI parsing --------------------------------------------------------

DEFAULT_LEVELS="O0,O1,O2,O3,Og,Os,Oz,Ofast"
levels="$DEFAULT_LEVELS"
build_jobs=6
test_jobs=6
slow=0
skip_build=0
skip_tally=0
no_preflight=0

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
    -h|--help)          sed -n '2,/^$/p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
    *)                  echo "unknown option: $1" >&2; exit 2 ;;
  esac
done

IFS=',' read -ra LEVEL_LIST <<< "$levels"

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
  local n="$1"; shift
  local fn="$1"; shift
  printf '%s\0' "$@" | xargs -0 -P "$n" -n 1 "$0" "$fn"
}

if [ $skip_build -eq 0 ]; then
  echo "==[ $(ts) setup waves ($build_jobs-way) ]==" | tee -a "$LOG"
  dispatch "$build_jobs" _setup_wave "${LEVEL_LIST[@]}"

  echo "==[ $(ts) delete stale *.c.o ]==" | tee -a "$LOG"
  dispatch 8 _clean_stale_c_o "${LEVEL_LIST[@]}"

  echo "==[ $(ts) build waves ($build_jobs-way) ]==" | tee -a "$LOG"
  dispatch "$build_jobs" _build_wave "${LEVEL_LIST[@]}"
  echo "==[ $(ts) build waves done ]==" | tee -a "$LOG"
fi

if [ $skip_tally -eq 0 ]; then
  # Build "LEVEL:DIR,LEVEL:DIR,…" for run-mc6809-tests --multi.
  multi=""
  for lvl in "${LEVEL_LIST[@]}"; do
    bd="$(wave_bd "$lvl")"
    if [ ! -f "$bd/build.ninja" ]; then
      echo "==[ $(ts) skip $lvl: $bd not configured (no build.ninja) ]==" \
        | tee -a "$LOG"
      continue
    fi
    multi="${multi:+$multi,}${lvl}:${bd}"
  done
  if [ -z "$multi" ]; then
    echo "==[ $(ts) no tally levels available ]==" | tee -a "$LOG"
  else
    slow_arg="";  [ $slow         -eq 1 ] && slow_arg="--slow"
    np_arg="";    [ $no_preflight -eq 1 ] && np_arg="--no-preflight"
    echo "==[ $(ts) tally SHARED POOL (--jobs $test_jobs over $multi) ]==" \
      | tee -a "$LOG"
    MC6809_BENCH_DB="$DB" \
      "$PICO/scripts/run-mc6809-tests" \
        --record --jobs "$test_jobs" --multi "$multi" \
        $slow_arg $np_arg 2>&1 | tee -a "$LOG"
    echo "==[ $(ts) tally pool done ]==" | tee -a "$LOG"
  fi
fi

echo "==[ $(ts) bench-parallel END ]==" | tee -a "$LOG"

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
 WHERE opt_level IN ('O0','O1','O2','O3','Os','Og','Oz','Ofast')
   AND run_id IN (
     SELECT MAX(run_id) FROM runs
      WHERE opt_level IS NOT NULL
      GROUP BY opt_level
   )
 GROUP BY opt_level
 ORDER BY CASE opt_level
            WHEN 'O0' THEN 1 WHEN 'O1' THEN 2 WHEN 'O2' THEN 3
            WHEN 'O3' THEN 4 WHEN 'Og' THEN 5 WHEN 'Os' THEN 6
            WHEN 'Oz' THEN 7 WHEN 'Ofast' THEN 8 END;
SQL