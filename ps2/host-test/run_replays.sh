#!/bin/bash
# usage: ./run_replays.sh /path/to/zelda3_assets.dat [frames per chapter, default 2500]
# Plays the reference replays (saves/ref, chapters 1-13) headless and prints every distinct misaligned access that UBSan finds.
# Nothing listed under "misaligned accesses" means clean. A screenshot (.ppm) of the last frame of each chapter is kept in the run dir.
set -e
HERE=$(cd "$(dirname "$0")" && pwd); ROOT=$(cd "$HERE/../.." && pwd)
ASSETS=$(realpath "$1"); FRAMES=${2:-2500}
RUN=$(mktemp -d); mkdir -p "$RUN/saves"
ln -s "$ASSETS" "$RUN/zelda3_assets.dat"; ln -s "$ROOT/saves/ref" "$RUN/saves/ref"
sed -e 's/^OutputMethod *=.*/OutputMethod = SDL-Software/' -e 's/^DisableFrameDelay *=.*/DisableFrameDelay = 1/' "$ROOT/ps2/zelda3.ini" > "$RUN/zelda3.ini"
cd "$RUN"
for n in $(seq 0 12); do
  SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy Z3_REPLAY=$n Z3_FRAMES=$FRAMES Z3_DUMP="$RUN/chapter$((n+1))" Z3_DUMP_EVERY=$FRAMES \
    UBSAN_OPTIONS=print_stacktrace=0 timeout 900 "$HERE/z3test" > "run$n.log" 2>&1 || echo "chapter $((n+1)): exit code $?"
done
echo "== misaligned accesses =="
grep -h "runtime error" run*.log | sed -E 's/0x[0-9a-f]+/ADDR/' | sort | uniq -c || true
echo "run dir: $RUN"
