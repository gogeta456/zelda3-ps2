# Zelda3 for PlayStation 2 — revision 2: fixed 60 FPS

Native libpad input, direct AUDSRV PCM output, gsKit presentation and a Portuguese settings menu. The game starts straight away; open the menu during play with **Start + Select**. X confirms, Circle cancels. Preferences and game SRAM are stored in alternating, checksummed files under `mc0:/ZELDA3-LTTP/` or `mc1:/ZELDA3-LTTP/`. The legacy `sram.dat` is retained and imported when no valid new SRAM bank exists.

Presentation is fixed at **60 FPS**: one drawing pass per game/audio update, with **frameskip permanently off** (no drawing pass is dropped to catch up; if the console cannot keep up, the game runs slower). Neither has a menu entry. Preferences saved by earlier builds (for example 30 FPS or frameskip on) are forced to these values on load, without resetting controls, audio preferences or SRAM.

The GS performs scaling, filtering, menu drawing and palette expansion on eligible scanlines. SNES composition, complex effects and SPC/DSP synthesis still use the EE. This is partial hardware acceleration. Audio generation follows game frames. The bundled AUDSRV server outputs silence on starvation without consuming unwritten data; the EE no longer performs the blocking runtime re-prime. The PCM budget matches the actual 341-input/512-output resampler ratio. Texture transfers omit unused rows, and full-screen clearing is skipped when the image covers the target.

The standalone ELF requires the user's `zelda3_assets.dat` beside it. The ISO authoring script embeds that file and checks every resulting disc entry. Sources do not include the assets.

See [Portuguese documentation](PS2.pt-BR.md), [usage instructions](ps2/LEIA-ME.txt) and [validation](ps2/VALIDACAO.md). This revision has compiled for PS2 and passed host regression and mocked-driver tests; it has not yet run on a PS2 or PCSX2. Real timing, perceived Triforce audio sync and GS presentation remain to be verified there.

Build with a full ps2dev environment: `make -f Makefile.ps2`. Set `PS2DEV`, `PS2SDK`, `GSKIT` and the compiler paths. Install `pycdlib`, then use `python3 ps2/make_iso.py zelda3.elf zelda3_assets.dat output.iso`.

**Video mode** (Options > Video > MODO DE VIDEO): 480i (default), 240p or 480p. 480p is 704x480 progressive, set up like Metroid Zero Mission DX;
a confirmation screen appears (press X within 10 s to keep it, otherwise it reverts) and the memory card only stores a confirmed 480p.
