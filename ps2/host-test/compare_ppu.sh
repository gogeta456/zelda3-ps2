#!/bin/sh
# Compare with the previous source tree, which is passed as the first argument.
set -eu
REFERENCE=$(realpath "$1")
cd "$(dirname "$0")/../.."
TEST_DIR=$(mktemp -d)
trap 'rm -rf "$TEST_DIR"' EXIT
FLAGS='-std=gnu11 -O2 -g -fsanitize=address,alignment -I.'
RENAMES=''
for name in ppu_init ppu_free ppu_reset ppu_read ppu_write ppu_runLine ppu_handleVblank ppu_saveload PpuSetSaturation PpuBeginDrawing PpuGetCurrentRenderScale PpuSetMode7PerspectiveCorrection PpuSetExtraSideSpace; do
  RENAMES="$RENAMES -D$name=Reference_$name"
done
gcc $FLAGS $RENAMES -c "$REFERENCE/snes/ppu.c" -o "$TEST_DIR/reference.o"
gcc $FLAGS -DZ3_GS_TEST=1 ps2/host-test/test_ppu_compare.c snes/ppu.c ps2/ps2_indexed.c "$TEST_DIR/reference.o" -o "$TEST_DIR/compare"
"$TEST_DIR/compare"
