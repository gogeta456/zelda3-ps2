# Zelda3 for PlayStation 2 — R5

Direct boot, 60 FPS target, no automatic frameskip and no configuration menu.
Old memory-card settings cannot select 30 FPS in this build. Existing game
saves, button mappings and sound settings remain readable.

R5 skips subscreen work when no visible main pixel can use it, caches exact
SNES color math and preserves unchanged palettes across frames. L3 toggles
a real presentation-FPS and per-stage millisecond overlay, without a menu.

The GS waits for one VBlank per picture. The second timer-based wait is gone.
PCM submission is asynchronous over SIF with retained buffers and bounded
queues. The bundled IOP server preserves the exact 32-to-48 kHz fractional
sample phase. Visible OAM entries are classified once per frame; OAM order
and scanline limits are preserved.

Reference: [z2442/zelda3-PSP](https://github.com/z2442/zelda3-PSP).
The PSP GU renderer and Media Engine DSP have not been ported wholesale.
PS2 composition and DSP still partly use the EE. This build has passed host
regression tests and cross-compilation, but has not run on PS2 or PCSX2 here.
Neither 60 FPS everywhere nor the reported scene-dependent 30 FPS drops is hardware-verified.

The standalone ELF needs `zelda3_assets.dat` beside it. The ISO/Portuguese
bundle includes the user's supplied PT-BR assets. Source archives omit assets.

See [Portuguese documentation](PS2.pt-BR.md), [usage](ps2/LEIA-ME.txt) and
[validation](ps2/VALIDACAO.md). Build: `make -f Makefile.ps2` with full ps2dev,
ps2sdk, gsKit, SDL2 and ps2_drivers. Create the disc with
`python3 ps2/make_iso.py game.elf zelda3_assets.dat game.iso` (requires pycdlib).
