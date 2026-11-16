#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
TEST_DIR=$(mktemp -d)
trap 'rm -rf "$TEST_DIR"' EXIT
FLAGS='-std=gnu11 -O2 -g -Wall -Wextra -Werror -fsanitize=address,undefined -I.'
gcc $FLAGS -DZ3_PS2_NO_MENU=1 ps2/host-test/test_port.c ps2/ps2_settings.c ps2/ps2_pad.c ps2/ps2_indexed.c -o "$TEST_DIR/port"
"$TEST_DIR/port"
gcc $FLAGS ps2/host-test/test_frame.c -o "$TEST_DIR/frame"
"$TEST_DIR/frame"
gcc $FLAGS -D_EE -Ips2/host-test/mocks ps2/host-test/test_audio.c ps2/ps2_audio.c -o "$TEST_DIR/audio"
"$TEST_DIR/audio"
gcc $FLAGS -D_EE -Ips2/host-test/mocks ps2/host-test/test_audio_transport.c ps2/ps2_audio_transport.c -o "$TEST_DIR/transport"
"$TEST_DIR/transport"
gcc $FLAGS ps2/host-test/test_dsp_samples.c snes/dsp.c -o "$TEST_DIR/dsp"
"$TEST_DIR/dsp"
gcc $FLAGS -D_EE ps2/host-test/test_files.c ps2/ps2_compat.c ps2/save_icon.S -o "$TEST_DIR/files"
"$TEST_DIR/files" 0
"$TEST_DIR/files" 1
"$TEST_DIR/files" 2
gcc $FLAGS -D_EE -Ips2/host-test/mocks -c ps2/ps2_menu.c -o "$TEST_DIR/menu.o"
gcc $FLAGS -Ips2/host-test/mocks ps2/host-test/test_menu.c ps2/ps2_pad.c ps2/ps2_settings.c ps2/ps2_font.c "$TEST_DIR/menu.o" -o "$TEST_DIR/menu"
mkdir "$TEST_DIR/menu-run"
(cd "$TEST_DIR/menu-run" && "$TEST_DIR/menu")
if [ -n "${Z3_MENU_CAPTURE_DIR:-}" ]; then
  mkdir -p "$Z3_MENU_CAPTURE_DIR"
  cp "$TEST_DIR/menu-run/"*.ppm "$Z3_MENU_CAPTURE_DIR/"
fi
