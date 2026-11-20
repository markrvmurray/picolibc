#!/bin/sh
# gen-mc6809-cross.sh — generate the MC6809 meson cross files from the
# template scripts/cross-clang-mc6809-unknown-elf.txt.in.
#
# The cross files bake in absolute paths to the llvm-mc6809 toolchain and
# to this checkout's exe_wrapper scripts, so they are machine-specific and
# are NOT committed (see .gitignore). bench-parallel.sh runs this
# automatically; run it by hand after cloning, or whenever LLVM_BIN moves.
#
# Locations are taken from the environment with sibling-directory
# defaults (llvm-mc6809 a sibling of this picolibc checkout):
#   PICO               picolibc checkout root      (default: this repo)
#   LLVM_REPO          llvm-mc6809 checkout        (default: ../llvm-mc6809)
#   LLVM_BUILD_SUBDIR  build dir within LLVM_REPO  (default: llvm/cmake-build-debug-system)
#   LLVM_BIN           toolchain bin directory     (default: $LLVM_REPO/$LLVM_BUILD_SUBDIR/bin)
#   MC6809_TOOLCHAIN   an installed toolchain prefix; its bin/ becomes
#                      LLVM_BIN, and the rest is derived from that compiler
# Override any of them by exporting before invoking.

set -eu

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
PICO=${PICO:-$(cd "$SCRIPT_DIR/.." && pwd)}
PARENT=$(dirname "$PICO")
LLVM_REPO=${LLVM_REPO:-$PARENT/llvm-mc6809}
LLVM_BUILD_SUBDIR=${LLVM_BUILD_SUBDIR:-llvm/cmake-build-debug-system}
# An installed toolchain instead of a build tree: one variable points the
# generated cross files at it, and everything else follows -- the resource
# directory, the OS-9 linker script and the floating-point module are all
# derived by asking that compiler where they are.
MC6809_TOOLCHAIN=${MC6809_TOOLCHAIN:-}
if [ -n "$MC6809_TOOLCHAIN" ]; then
	LLVM_BIN=${LLVM_BIN:-$MC6809_TOOLCHAIN/bin}
fi
LLVM_BIN=${LLVM_BIN:-$LLVM_REPO/$LLVM_BUILD_SUBDIR/bin}

TEMPLATE=$SCRIPT_DIR/cross-clang-mc6809-unknown-elf.txt.in
[ -f "$TEMPLATE" ] || { echo "gen-mc6809-cross: missing template $TEMPLATE" >&2; exit 1; }

# emit OUTFILE EXE_WRAPPER DP_BASE
emit() {
	out=$1; wrapper=$2; dp=$3
	# Substitute the %%...%% generator placeholders only; meson's @...@
	# tokens in directpage_block/_memory are left intact. Paths are fed
	# via the shell environment to sed's `r`-free `s` with a control-char
	# delimiter so slashes in paths need no escaping.
	sed -e "s|%%LLVM_BIN%%|$LLVM_BIN|g" \
	    -e "s|%%EXE_WRAPPER%%|$wrapper|g" \
	    -e "s|%%TOOLCHAIN_REF%%|$LLVM_BIN/clang|g" \
	    -e "s|%%DP_BASE%%|$dp|g" \
	    "$TEMPLATE" > "$out"
	echo "  wrote $out"
}

echo "gen-mc6809-cross: LLVM_BIN=$LLVM_BIN"

# The HD6309 cross's exe_wrapper selects the execution backend. Default is now
# USim (run-mc6809-usim6309, usim09batch --hd6309) — ~11x faster than MAME and
# result-identical across the suite. BENCH_HD6309_BACKEND=mame restores the MAME
# backend (run-mc6809-mame), the ground-truth cross-check. Same -mcpu=hd6309
# codegen either way; only the executor (and its cycle model) differs, so the
# bench records the two under distinct -hd6309-usim / -hd6309-mame opt_levels.
hd6309_wrapper="$PICO/scripts/run-mc6809-usim6309"
if [ "${BENCH_HD6309_BACKEND:-usim}" = "mame" ]; then
	hd6309_wrapper="$PICO/scripts/run-mc6809-mame"
fi
echo "gen-mc6809-cross: HD6309 backend = ${BENCH_HD6309_BACKEND:-usim} ($hd6309_wrapper)"

emit "$SCRIPT_DIR/cross-clang-mc6809-unknown-elf.txt"      "$PICO/scripts/run-mc6809"       0
emit "$SCRIPT_DIR/cross-clang-mc6809-unknown-elf-hd6309.txt" "$hd6309_wrapper"              0
emit "$SCRIPT_DIR/cross-clang-mc6809-unknown-elf-dp7F.txt" "$PICO/scripts/run-mc6809"     127

# OS-9 / NitrOS-9: its own template (no flash/RAM/direct-page model); the
# DECB: the driver supplies the start-up code and the linker script, so the
# cross file names neither.  No exe_wrapper: nothing here can run one.
DECB_TEMPLATE=$SCRIPT_DIR/cross-clang-mc6809-unknown-decb.txt.in
if [ -f "$DECB_TEMPLATE" ]; then
	sed -e "s|%%LLVM_BIN%%|$LLVM_BIN|g" \
	    "$DECB_TEMPLATE" > "$SCRIPT_DIR/cross-clang-mc6809-unknown-decb.txt"
	echo "  wrote $SCRIPT_DIR/cross-clang-mc6809-unknown-decb.txt"
fi

# linker script lives in the compiler's resource directory.
OS9_TEMPLATE=$SCRIPT_DIR/cross-clang-mc6809-unknown-os9.txt.in
OS9_RESDIR=$("$LLVM_BIN/clang" --target=mc6809-unknown-os9 -print-resource-dir 2>/dev/null)/lib/mc6809-unknown-os9
OS9_LDS=$OS9_RESDIR/mc6809-os9.lds
OS9_FP=$OS9_RESDIR/FPO9
if [ -f "$OS9_TEMPLATE" ]; then
	if [ ! -f "$OS9_LDS" ]; then
		echo "gen-mc6809-cross: warning: $OS9_LDS not found (build the OS-9 runtime: runtimes/builtins-mc6809-unknown-os9-build)" >&2
	fi
	sed -e "s|%%LLVM_BIN%%|$LLVM_BIN|g" \
	    -e "s|%%EXE_WRAPPER%%|$PICO/scripts/run-mc6809-os9|g" \
	    -e "s|%%OS9_FP%%|$OS9_FP|g" \
	    -e "s|%%TOOLCHAIN_REF%%|$LLVM_BIN/clang|g" \
	    -e "s|%%OS9_LDS%%|$OS9_LDS|g" \
	    "$OS9_TEMPLATE" > "$SCRIPT_DIR/cross-clang-mc6809-unknown-os9.txt"
	echo "  wrote $SCRIPT_DIR/cross-clang-mc6809-unknown-os9.txt"
fi
