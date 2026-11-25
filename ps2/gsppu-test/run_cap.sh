#!/bin/bash
# usage: run_cap.sh <captured prefix, e.g. /home/claude/cap/c0_000400>  -> compares the GS render with the software frame
D=/home/claude/z3/zelda3-ps2/ps2/gsppu-test
cp $1.state $D/state.bin; cp $1.rgba $D/ref.rgba; rm -f $D/out.rgba $D/out.txt
timeout 70 xvfb-run -a -s "-screen 0 1280x720x24" bash /home/claude/run3.sh $D/gsppu_test.elf 12 >/dev/null 2>&1
cat $D/out.txt 2>/dev/null | tr '\n' ' ' || echo "no out.txt"
[ -f $D/out.rgba ] && python3 $D/compare.py $D/ref.rgba $D/out.rgba /home/claude/case.png $D/state.bin
