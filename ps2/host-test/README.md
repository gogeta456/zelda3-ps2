Revisao PS2: consulte ../../PS2.pt-BR.md e ../VALIDACAO.md para a nova suite.

# Headless host test

`make` builds the PS2-patched sources for x86-64 with `-fsanitize=alignment`, plus a small test driver generated from
`src/main.c` (`make_main_test.py`). `./run_replays.sh zelda3_assets.dat` then plays the 13 reference replays in `saves/ref`
(the whole game, chapter by chapter) with SDL's dummy video/audio drivers and reports any misaligned memory access.

The PS2's R5900 raises an exception on misaligned loads/stores (x86 and ARM do not), so any new `*(uint16 *)ptr` style access
that can hit an odd address must use the unaligned types from `src/types.h` (`uint16u`, `int16u`, `uint32u`, `uint64u`).
This test is how the port was checked: chapters 1-13, 2500 frames each, no misaligned access reported.

The test driver can also write one hash per finished frame (`Z3_HASH=file`, see `make_main_test.py`). Build twice (before and after
a change to the renderer, or with `EXTRA=-DZ3_PIXEL_ABGR=1`), run the same replays and `cmp` the files: identical files mean
identical pictures. The PPU speed-ups and the PS2's native pixel order were verified like this over 32,500 frames.

Requirements: gcc, python3, `libsdl2-dev`, and `zelda3_assets.dat` extracted from your own ROM (see ../../PS2.md).
