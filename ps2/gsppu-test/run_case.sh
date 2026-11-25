#!/bin/bash
# usage: run_case.sh seed layers sprmode hdma  -> prints comparison, writes /home/claude/case.png
D=/home/claude/z3/zelda3-ps2/ps2/gsppu-test
/tmp/gen_ref $1 $2 $3 $4 $D || exit 1
rm -f $D/out.rgba $D/out.txt
timeout 70 xvfb-run -a -s "-screen 0 1280x720x24" bash /home/claude/run3.sh $D/gsppu_test.elf 12 >/dev/null 2>&1
cat $D/out.txt 2>/dev/null || echo "no out.txt"
[ -f $D/out.rgba ] && python3 $D/compare.py $D/ref.rgba $D/out.rgba /home/claude/case.png
