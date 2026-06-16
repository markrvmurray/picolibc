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
# Override any of them by exporting before invoking.

set -eu

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
PICO=${PICO:-$(cd "$SCRIPT_DIR/.." && pwd)}
PARENT=$(dirname "$PICO")
LLVM_REPO=${LLVM_REPO:-$PARENT/llvm-mc6809}
LLVM_BUILD_SUBDIR=${LLVM_BUILD_SUBDIR:-llvm/cmake-build-debug-system}
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
	    -e "s|%%DP_BASE%%|$dp|g" \
	    "$TEMPLATE" > "$out"
	echo "  wrote $out"
}

echo "gen-mc6809-cross: LLVM_BIN=$LLVM_BIN"
emit "$SCRIPT_DIR/cross-clang-mc6809-unknown-elf.txt"      "$PICO/scripts/run-mc6809"       0
emit "$SCRIPT_DIR/cross-clang-mc6809-unknown-elf-mame.txt" "$PICO/scripts/run-mc6809-mame"  0
emit "$SCRIPT_DIR/cross-clang-mc6809-unknown-elf-dp7F.txt" "$PICO/scripts/run-mc6809"     127
