#!/bin/sh
# Usage: ps2/make_iso.sh zelda3.elf zelda3_assets.dat [output.iso]
set -eu
exec python3 "$(dirname "$0")/make_iso.py" "$@"
