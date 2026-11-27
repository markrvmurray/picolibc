# Building, testing and benchmarking picolibc for the MC6809 / HD6309

This fork adds an MC6809 (and HD6309) port of picolibc, built with the
`llvm-mc6809` Clang/LLVM backend and exercised on the `usim` simulator,
6809 and HD6309 alike (MAME's `llvm6309` machine is an optional, slower
alternative for HD6309). The
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
                        only needed with BENCH_HD6309_BACKEND=mame
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
2. **Emulators** (the test runners): `usim09batch` runs everything,
   HD6309 included (`--hd6309`); the MAME-based `run-mc6809-mame` is an
   optional alternative for HD6309, selected with
   `BENCH_HD6309_BACKEND=mame`. With the default
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

`bench-parallel.sh` exports `MAMELLVM6309=$MAME_RUNNER` and
`MC6809_HD6309_SIM=$BENCH_HD6309_BACKEND` for the lit Execution gate, so
its HD6309 tests run on the same emulator as the `-hd6309` levels (usim
unless you ask for MAME).

## Meson cross files are generated, not committed

Meson cross files must contain **absolute** paths to the toolchain, so
they are machine-specific. They are generated from a template and are
git-ignored:

```
scripts/cross-clang-mc6809-unknown-elf.txt.in   ← committed template
scripts/gen-mc6809-cross.sh                      ← generator
scripts/cross-clang-mc6809-unknown-elf.txt       ← generated (usim, DP=$00)
scripts/cross-clang-mc6809-unknown-elf-hd6309.txt ← generated (HD6309: usim, or MAME with BENCH_HD6309_BACKEND=mame; DP=$00)
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
| `test-run-mc6809-os9` | Proves `run-mc6809-os9` finds the one firmware, loads `/J0` where an image needs it, and refuses an image that cannot reach the test disk, using stub tools. Takes seconds. |
| `test-bench-gates` | Proves the bench's exit status can fail and its one-at-a-time lock works, using a sandboxed copy of the bench and a stub lit. Takes seconds, touches nothing real. |
| `gen-mc6809-cross.sh` | Generate the meson cross files from the template for this machine's toolchain location. |
| `run-mc6809-tests` | The recording test harness (`--record --multi LEVEL:DIR,…`) that `bench-parallel.sh` calls. |
| `run-mc6809-tests-local` | Lightweight local variant that sets the tool env vars and runs the harness. |
| `run-mc6809` | exe_wrapper: run one ELF on `usim09batch` (reads `$USIM09BATCH`). |
| `run-mc6809-mame` | exe_wrapper: run one ELF on HD6309 under MAME (reads `$MAME_LLVM6309_DIR`). |

## Build layout

One meson builddir per configuration: `builddir-mc6809-<LEVEL>`. Suffixes
stack: `-fp` (libm + stdio-float), `-lto` and `-hd6309` (6309 ISA), e.g.
`builddir-mc6809-Os-lto-hd6309-fp`. The 6309 levels run on usim unless
`BENCH_HD6309_BACKEND=mame`, and the ledger records them as `-hd6309-usim`
or `-hd6309-mame` to match.

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
  --levels O0,O0-fp,O1,O1-fp,O2,O2-fp,O2-lto,O2-hd6309,O2-hd6309-fp,O3,O3-fp,O3-lto,Og,Og-fp,Og-hd6309,Og-lto-hd6309-fp,Os,Os-fp,Os-lto,Os-lto-fp,Os-hd6309,Os-hd6309-fp,Os-lto-hd6309,Os-lto-hd6309-fp,Oz,Oz-fp,Ofast,Ofast-fp,os9-Os,os9-Os-fp,os9-Os-hd6309 \
  --build-jobs 5 --test-jobs 16
```

- 20 6809 levels + 8 HD6309 levels, all on usim, + a lit gate (incl. the
  on-usim `test/MC/MC6809/Execution/` suite) ≈ 29 ledger rows.  With
  `BENCH_HD6309_BACKEND=mame` the 8 HD6309 levels run on MAME instead,
  about 11x slower.
- Three OS-9 levels are in the default set as well: `os9-Os` (NitrOS-9
  Level 1), `os9-Os-fp` (Level 2) and `os9-Os-hd6309` (the 6309 build on the
  6309 NitrOS-9 Level 2), each booted on usim09pt. They are tallied one at
  a time rather than in the shared pool -- a level is a NitrOS-9 level and
  CPU, which are environment variables, and one pool cannot hold two. They record no
  cycles: a boot is most of what a run costs, so a cycle count would be
  measuring the operating system. Their size column holds the OS-9 module's
  own size, the thing that has to fit in a task's blocks.
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

## One bench at a time

`bench-parallel.sh` and `run-mc6809-tests --record` share a kernel lock,
`$MC6809_BENCH_LOCK` (default `/tmp/mc6809-bench.lock`), held from start to
exit, and refuse to start if it is taken. The refusal lists the processes
holding it. It cannot go stale: there is nothing to clean up after a crash, and
the file itself is never removed. A killed bench's workers keep the lock until
they exit, because they can still write. Running `usim09batch` or an unrecorded
`run-mc6809-tests` by hand needs no lock and blocks nothing.

## The ledger

`$MC6809_BENCH_DB` (default `~/Documents/mc6809-bench/results.sqlite`) —
two tables:

- `runs(run_id, opt_level, timestamp, llvm_commit, picolibc_commit, …)`
  — one row per (level, run).
- `results(run_id, test_name, suite, status, wall_seconds, cycles, rc,
  opt_level, text_bytes)`.

Lit rows (`opt_level='lit'`) name a test by lit's identifier, such as
`LLVM :: CodeGen/MC6809/add.ll`. Rows from before that change hold the bare
file name, and `Runtime` for the one OS-9 case that was kept.

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

## OS-9 / NitrOS-9 (`mc6809-unknown-os9`)

`gen-mc6809-cross.sh` also writes `cross-clang-mc6809-unknown-os9.txt`, for
building picolibc as OS-9 program modules.  Configure with:

```sh
meson setup builddir-mc6809-os9-Os \
    --cross-file scripts/cross-clang-mc6809-unknown-os9.txt \
    -Dpicocrt=false -Dsemihost=false -Dos-os9=true -Dposix-console=true \
    -Dtests=true -Doptimization=s \
    -Dformat-default=integer -Dprintf-aliases=false -Dstdio-float=false \
    -Dposix-extensions=false -Dsearch-extensions=true -Dwant-libm=false \
    -Dmb-capable=false -Dio-long-long=false -Dmalloc-clear-allocated=false \
    -Dtests-enable-stack-protector=false
```

For floating point, add `-Dstdio-float=true -Dwant-libm=true` and call the
directory `builddir-mc6809-os9-Os-fp`, exactly as the bare-metal `-fp` levels
do:

```sh
meson setup builddir-mc6809-os9-Os-fp \
    --cross-file scripts/cross-clang-mc6809-unknown-os9.txt \
    -Dpicocrt=false -Dsemihost=false -Dos-os9=true -Dposix-console=true \
    -Dtests=true -Doptimization=s \
    -Dformat-default=integer -Dprintf-aliases=false -Dstdio-float=true \
    -Dposix-extensions=false -Dsearch-extensions=true -Dwant-libm=true \
    -Dmb-capable=false -Dio-long-long=false -Dmalloc-clear-allocated=false \
    -Dtests-enable-stack-protector=false
```

A program that does floating point does not carry the MC6839 ROM: it links
the FPO9 module, which the compiler stages beside `mc6809-os9.lds` and which
the cross file passes to the test runner (`--fp-module`) to put on each test
disk.  Remember that the module shares the process's 64K address space with
the program and its data area, so a large floating-point program can run out
of room where the same program fits on bare metal -- the kernel says so
(`Error #207`, memory full) at start-up rather than half way through.

`-Dprintf-aliases=false` is not incidental.  Turning aliases on is a global
switch: it emits `-D_PICOLIBC_PRINTF` for every test built with
`printf_compile_args_d`, which pulls the double `vfprintf` into all of them,
and on a 64K target most of them then do not fit -- measured at Os-fp, build
failures go from 69 to 187, every one a section overflow.  A test that really
needs a particular printf variant opts in on its own rule, as
`libc-testsuite/sscanf` does with `-D_PICOLIBC_SCANF='d'` and a `--defsym`.
The consequence to remember is that any `printf_compile_args_*` a test asks
for here is **silently ignored**; if a test's diagnostics need floating point,
they have to be written another way (the math tests print bit patterns).

The start-up code, the `<os9.h>` system-call shims and `mc6809-os9.lds` come
from the compiler's OS-9 runtime (`libclang_rt.os9`, built by the LLVM tree's
`runtimes/builtins-mc6809-unknown-os9` target); the cross file's `os9_lds`
property points at the script and must exist before configuring.

Ninja has no edge from a test binary, or from `libc.a`'s objects, to the
compiler that built them -- not to `clang`, not to `libclang_rt.os9.a`, not to
`mc6809-os9.lds`.  So after rebuilding LLVM, `ninja -C builddir-…` relinks
nothing and rebuilds nothing, and the tests exercise the *previous* backend
while looking perfectly green.  One command puts a level right:

```sh
scripts/mc6809-refresh builddir-mc6809-os9-Os        # or any level
```

`run-mc6809-tests` and `bench-parallel.sh` do this themselves before every
measured run (it is `--refresh-only` of the same preflight), so the numbers
they report are always against the compiler on disk.  For anything run by
hand, the two runners compare the program against the toolchain and refuse
to run one that predates it -- exit 99, naming the command above.
`MC6809_ALLOW_STALE=1` runs it anyway.

Each test runs on a real NitrOS-9 boot (~2 s), driven by
`scripts/run-mc6809-os9` as meson's `exe_wrapper`:

```sh
export NITROS9_BOOT_DSK=…/NOS9_6809_L2_DEV_picothing_mega.dsk # has /J0
export NITROS9_FIRMWARE=…/rel_picothing.srec
export PATH=…/usim:$PATH                                       # usim09pt
PICOLIBC_BUILDDIR=$PWD/builddir-mc6809-os9-Os \
    python3 scripts/run-mc6809-tests --jobs 8 --include-fake-fs
```

Both levels run the same binaries -- a level is a boot image, not a build,
since `U` is the data base wherever the kernel puts it:

```sh
NITROS9_LEVEL=1 PICOLIBC_BUILDDIR=$PWD/builddir-mc6809-os9-Os-fp \
    python3 scripts/run-mc6809-tests --jobs 8 --include-fake-fs \
            --opt-level os9-l1-Os-fp
```

`NITROS9_LEVEL` (default 2) picks the mega boot image out of
`NITROS9_RECIPES`, whose `/J0` descriptor is how the test disk is reached. The
firmware is the newest `rel_picothing.srec` there, since one REL boots every
level. An explicit `NITROS9_BOOT_DSK`/`NITROS9_FIRMWARE` still wins.  Give the run its own `--opt-level` label so the two levels get
separate ledger rows.

Expect a very different shape at Level 1: it has no DAT, so a program
shares the one 64K space with the system and the size limit is far lower
(measured on this boot image: 31804 bytes runs, 35568 is refused).  Level 1
refuses cleanly at every size, which is why the module-size gate is applied
only at Level 2, where the loader wedges instead.

The runner sets the guest's clock from the host at every boot (the kernel
starts at its 1986 epoch); `NITROS9_SETIME=off` leaves it alone, and
`NITROS9_SETIME='2026/08/19 12:00:00'` pins it.

### OS-9 text output

Descriptor-backed text output uses `I$WritLn`, so SCF sends CR with the
line-feed behavior selected by its path options (`PD.ALF`). RBF preserves
every byte; short writes at CR are handled by the existing flush loop.
Binary streams, raw `write`, memory streams
and cookie/custom callbacks retain their bytes. `fopen`, `fdopen` and
`freopen` honor `b` when choosing console output behavior.

Stdio does not translate CR or LF. Native newline character values are the
compiler's responsibility: the planned OS-9 clang change makes `\n` CR,
with `\l` for a physical LF. Released RC2 still defines `\n` as LF.

`test-os9-text` checks byte-preserving disk, binary, raw and cookie I/O,
seeks, mode changes and the compiler's newline delimiter. The companion
`test-os9-text-console` uses explicit CR, so it can test native console
behavior even with RC2; physical LF probes use octal `\012`. The runner
checks its raw emulator log for exact CR/LF text output and unchanged LF
binary/raw output before normalizing anything. Run both tests with
`-Dfast-bufio=false` and `true`.

### Shared OS-9 calls

The OS-9 build requires an LLVM runtime built with the canonical
[nitros9-c-libs](https://github.com/nitros9project/nitros9-c-libs) OS-call
provider (`NITROS9_OS_CALLS_ABI` in its installed `os9.h`). An older runtime
now produces an explicit compile-time error. Rebuild/install the LLVM OS-9
runtime and regenerate the cross file before configuring picolibc.

Picolibc keeps POSIX semantics and errno translation in `libos/os9`, and
calls the shared native assembly through the installed OS-9 header. It does
not vendor syscall implementations or add calling-convention wrappers.
The LLVM runtime pins the library revision and installs a source-hash
manifest alongside its runtime archive.
