#!/bin/sh
set -eu
REFERENCE=$(realpath "$1")
cd "$(dirname "$0")/../.."
TEST_DIR=$(mktemp -d)
trap 'rm -rf "$TEST_DIR"' EXIT
FLAGS='-std=gnu11 -O2 -g -fsanitize=address,alignment -I.'
RENAMES=''
for name in dsp_init dsp_free dsp_reset dsp_cycle dsp_read dsp_write dsp_getSamples dsp_saveload; do
  RENAMES="$RENAMES -D$name=Reference_$name"
done
gcc $FLAGS $RENAMES -c "$REFERENCE/snes/dsp.c" -o "$TEST_DIR/reference.o"
gcc $FLAGS ps2/host-test/test_dsp_compare.c snes/dsp.c "$TEST_DIR/reference.o" -o "$TEST_DIR/compare"
"$TEST_DIR/compare"
