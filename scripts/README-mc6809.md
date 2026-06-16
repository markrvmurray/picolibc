# Building and testing picolibc for the MC6809 / HD6309

This fork adds an MC6809 (and HD6309) port of picolibc, built with the
`llvm-mc6809` Clang/LLVM backend and exercised on the `usim` simulator
(and optionally MAME's `llvm6309` machine for HD6309 coverage).

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
full on-simulator test suite, and records cycle counts to the ledger:

```sh
./scripts/bench-parallel.sh --levels O0,O1,O2,Os --test-jobs 8
```

Run `./scripts/bench-parallel.sh --help` for the full option list.
