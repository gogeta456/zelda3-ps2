# Bundled AUDSRV server for Zelda3 PS2 revision 2

Derived from the PS2SDK AUDSRV sources at https://github.com/ps2dev/ps2sdk/tree/master/iop/sound/audsrv. The unmodified `audsrv.c` used for this local snapshot has Git blob SHA-1 `394b3e6d3a43965980e969724f9d3675d42163a7`. Original copyright notices remain in every source file; the applicable AFL 2.0 and LGPL 2.0 texts are included.

Local changes (8 November 2026):

- Initialize the PCM ring empty and zero its storage on a format change.
- Reserve one word so full and empty are distinguishable; reject impossible wait sizes.
- Emit silence and retain the read head when fewer than one source fragment is queued.
- Keep the upstream 16-bit 32 kHz upsamplers; the EE clock matches their effective rate of 341 input samples for 512 SPU2 samples.
- Identify this build as `0.93-z3r2` and build it with the installed PS2SDK sample Makefiles.

`ps2/audsrv_embed.S` provides `audsrv_irx` and `size_audsrv_irx` before `libps2_drivers` is linked, so the regular `init_audio_driver()` loads this server. No extra file is needed beside the ELF or inside the ISO. Rebuild with `make -f Makefile.ps2`; a complete ps2dev toolchain with IOP compiler and `iopfixup` is required. This patch targets the game's 16-bit mono/stereo 32 kHz PCM stream; other AUDSRV formats are not part of its validation.
