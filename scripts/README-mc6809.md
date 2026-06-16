# Building, testing and benchmarking picolibc for the MC6809 / HD6309

This fork adds an MC6809 (and HD6309) port of picolibc, built with the
`llvm-mc6809` Clang/LLVM backend and exercised on the `usim` simulator
(and optionally MAME's `llvm6309` machine for HD6309 coverage). The
harness cross-builds picolibc, runs the test suite on the emulators, and
records per-test cycle counts and pass/fail status to a SQLite ledger so
codegen trends are visible across commits and optimisation levels.

The build and test scripts under `scripts/` originally hard-coded one
developer's home directory. They now take every external location from
the environment, with **sibling-directory defaults**: by default the
other repositories are expected to sit next to this picolibc checkout.

## Directory layout (default)

```
<parent>/
├── picolibc/        ← this checkout
├── llvm-mc6809/     ← the LLVM backend, with a build dir at
│                       llvm/cmake-build-debug-system/
├── usim/            ← the MC6809 simulator (usim09batch)
└── mame/            ← optional: MAME llvm6309 wrapper (run-mc6809-mame),
                        only needed for HD6309 (`-mame`) bench levels
```

If your checkouts live elsewhere, set the environment variables below —
nothing assumes a home-directory path.

## Prerequisites

1. **Build the compiler first** — the harness always uses your
   *just-built* clang from the LLVM tree:
   ```sh
   cd ../llvm-mc6809/llvm/cmake-build-debug-system
   cmake --build . --target all -j 8     # --target all: LTO needs a fresh ld.lld
   ```
2. **Emulators** (the test runners): `usim09batch` (6809) and, for
   HD6309 coverage, the MAME-based `run-mc6809-mame`. With the default
   sibling layout these are found automatically; otherwise point the
   harness at them with the environment variables below.

## Environment variables

| Variable            | Default                                         | Purpose |
|---------------------|-------------------------------------------------|---------|
| `PICO`              | this repo (auto-detected from the script path)  | picolibc checkout root |
| `LLVM_REPO`         | `$PICO/../llvm-mc6809`                           | llvm-mc6809 checkout |
| `LLVM_BUILD_SUBDIR` | `llvm/cmake-build-debug-system`                 | build dir within `LLVM_REPO` |
| `LLVM_BIN`          | `$LLVM_REPO/$LLVM_BUILD_SUBDIR/bin`             | directory holding `clang`, `llvm-ar`, … |
| `USIM`              | `$PICO/../usim`                                 | usim checkout |
| `USIM09BATCH`       | `$USIM/usim09batch`                             | the simulator binary |
| `MAME_RUNNER`       | `$PICO/../mame/run-mc6809-mame`                 | MAME llvm6309 wrapper (HD6309 only) |
| `MC6809_BENCH_DB`   | `~/Documents/mc6809-bench/results.sqlite`       | bench results ledger |

`bench-parallel.sh` exports `MAMELLVM6309=$MAME_RUNNER` for the lit
Execution gate, so you only ever set `MAME_RUNNER` (or rely on the
sibling default).

## Meson cross files are generated, not committed

Meson cross files must contain **absolute** paths to the toolchain, so
they are machine-specific. They are generated from a template and are
git-ignored:

```
scripts/cross-clang-mc6809-unknown-elf.txt.in   ← committed template
scripts/gen-mc6809-cross.sh                      ← generator
scripts/cross-clang-mc6809-unknown-elf.txt       ← generated (usim, DP=$00)
scripts/cross-clang-mc6809-unknown-elf-mame.txt  ← generated (MAME, DP=$00)
scripts/cross-clang-mc6809-unknown-elf-dp7F.txt  ← generated (usim, DP=$7F)
```

Generate them after cloning (or whenever the toolchain moves):

```sh
./scripts/gen-mc6809-cross.sh           # uses the env vars / defaults above
LLVM_BIN=/opt/llvm-mc6809/bin ./scripts/gen-mc6809-cross.sh   # explicit override
```

`bench-parallel.sh` runs the generator automatically, so a normal bench
run needs no manual step.

## Scripts

| Script | Role |
|---|---|
| `bench-parallel.sh` | **Top-level orchestrator.** Per level: `meson setup` → scrub stale `*.c.o` → `ninja -k 0` build → one shared parallel test pool → record to the ledger. |
| `gen-mc6809-cross.sh` | Generate the meson cross files from the template for this machine's toolchain location. |
| `run-mc6809-tests` | The recording test harness (`--record --multi LEVEL:DIR,…`) that `bench-parallel.sh` calls. |
| `run-mc6809-tests-local` | Lightweight local variant that sets the tool env vars and runs the harness. |
| `run-mc6809` | exe_wrapper: run one ELF on `usim09batch` (reads `$USIM09BATCH`). |
| `run-mc6809-mame` | exe_wrapper: run one ELF on HD6309 under MAME (reads `$MAME_LLVM6309_DIR`). |

## Build layout

One meson builddir per configuration: `builddir-mc6809-<LEVEL>`. Suffixes
stack: `-fp` (libm + stdio-float), `-lto`, `-hd6309` (6309 ISA), `-mame`
(runs under MAME), e.g. `builddir-mc6809-Os-lto-hd6309-mame-fp`.

## Quick start

```sh
# 1. Generate the cross files for your toolchain location.
./scripts/gen-mc6809-cross.sh

# 2. Configure and build one optimisation level by hand.
meson setup builddir-mc6809-Os \
    --cross-file scripts/cross-clang-mc6809-unknown-elf.txt -Doptimization=s ...
ninja -C builddir-mc6809-Os

# 3. Run the test suite for that build dir.
./scripts/run-mc6809-tests-local --jobs 6 builddir-mc6809-Os
```

## Full parallel bench

`bench-parallel.sh` builds every optimisation level in parallel, runs the
full on-simulator test suite, and records cycle counts to the ledger.
With the default sibling layout no environment setup is needed:

```sh
./scripts/bench-parallel.sh \
  --levels O0,O0-fp,O1,O1-fp,O2,O2-fp,O2-lto,O2-hd6309-mame,O2-hd6309-mame-fp,O3,O3-fp,O3-lto,Og,Og-fp,Og-hd6309-mame,Og-lto-hd6309-mame-fp,Os,Os-fp,Os-lto,Os-lto-fp,Os-hd6309-mame,Os-hd6309-mame-fp,Os-lto-hd6309-mame,Os-lto-hd6309-mame-fp,Oz,Oz-fp,Ofast,Ofast-fp \
  --build-jobs 5 --test-jobs 16
```

- 20 USim levels + 8 MAME-HD6309 levels + a lit gate (incl. the on-usim
  `test/MC/MC6809/Execution/` suite) ≈ 29 ledger rows. ~30–40 min wall,
  MAME-dominated.
- Slow tests are **on by default** — pass `--fast` to opt out for a
  quick subset.

Run `./scripts/bench-parallel.sh --help` for the full option list.

### Lighter modes

```sh
# one test, one level (~seconds) — reuses existing binaries
./scripts/bench-parallel.sh --skip-build --levels O1 --tests snprintf,asctime

# one level, full test set (~1–2 min)
PICOLIBC_BUILDDIR=$PWD/builddir-mc6809-O1 \
  ./scripts/run-mc6809-tests --record --jobs 8 --opt-level O1
```

## ⚠️ The one mandatory gotcha — scrub after a compiler rebuild

**ninja cannot see that clang changed** (the compiler-binary mtime is
invisible to its dependency tracker). After rebuilding the LLVM backend
you MUST wipe the builddirs, or you will silently test stale objects
built with the *old* compiler:

```sh
for d in builddir-mc6809-*; do
  [ -d "$d/meson-info" ] && meson setup --wipe "$d"
done
```

`meson setup --wipe` is the only fully reliable reset (deleting `*.c.o`
alone has been observed insufficient). `bench-parallel.sh` deletes stale
`*.c.o`/`*.cpp.o` per level during its build phase, which covers the
common case; a manual single-level build does no scrubbing at all. After
a backend rebuild, prefer the `--wipe` loop above.

## The ledger

`$MC6809_BENCH_DB` (default `~/Documents/mc6809-bench/results.sqlite`) —
two tables:

- `runs(run_id, opt_level, timestamp, llvm_commit, picolibc_commit, …)`
  — one row per (level, run).
- `results(run_id, test_name, suite, status, wall_seconds, cycles, rc,
  opt_level, text_bytes)`.

```sql
-- latest per-level tally
SELECT r.opt_level, res.status, COUNT(*)
  FROM results res JOIN runs r USING(run_id)
 WHERE r.run_id IN (SELECT MAX(run_id) FROM runs WHERE opt_level IS NOT NULL GROUP BY opt_level)
 GROUP BY r.opt_level, res.status ORDER BY r.opt_level, res.status;
```

Result statuses: `OK`, `EXPECTEDFAIL`, `SKIP`, `UNSUPPORTED`, `FAIL`,
`TIMEOUT`, `BUILDFAIL` (a test that fails to build, often the libm-size
class at `-fp`).
