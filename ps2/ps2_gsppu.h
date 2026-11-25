#ifndef Z3_PS2_GSPPU_H
#define Z3_PS2_GSPPU_H
// PPU rendering on the GS: BG tiles and sprites are drawn as textured quads straight from an atlas of the SNES VRAM,
// so the EE only records per-line register state and builds a quad list.  Lines the GS path cannot reproduce
// (windows, mosaic, color math, mode 7, ...) are left to the software PPU (see Ps2GsPpuLine).
#include <stdbool.h>
#include <stdint.h>
#include "snes/ppu.h"
#ifdef _EE
#include <gsKit.h>
#endif

enum { Z3_GSPPU_LINES = 224, Z3_GSPPU_MAX_STRIPS = 32 };

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _EE
// Allocates GS memory (atlases and palettes). Returns false when it does not fit; the GS path then stays off.
bool Ps2GsPpuInit(GSGLOBAL *gs);
#endif

// Starts a frame. enabled=false makes Ps2GsPpuLine refuse every line.
void Ps2GsPpuBegin(bool enabled);
// Called by ppu_runLine for screen row y (0-based) before any software drawing. Returns true when the GS will draw this
// row (the caller must then skip the software path for it).
bool Ps2GsPpuLine(const Ppu *ppu, unsigned y);
bool Ps2GsPpuHasLine(unsigned y);
unsigned Ps2GsPpuLineCount(void);

#ifdef _EE
// Draws every recorded row. x0/sx/sy: screen position of the 256x224 picture and its scale (2.5 x 2 or 2 x 2).
void Ps2GsPpuDraw(GSGLOBAL *gs, float x0, float y0, float sx, float sy);
// Per video mode: how many strips of rows may differ (without scissor only one strip is possible), whether the frame
// buffer has an alpha channel (needed for the color math mask) and whether the scissor register may be used.
void Ps2GsPpuConfigure(int max_strips, bool use_scissor, bool has_alpha);
// Same rows at 1x into a 256x224 CT32 buffer in GS memory (vram_bytes must be 8 KB aligned), then back to the screen.
// Used with the smooth filter: the buffer is then drawn to the screen as an ordinary texture.
void Ps2GsPpuDrawOffscreen(GSGLOBAL *gs, uint32_t vram_bytes);
#endif

#ifdef __cplusplus
}
#endif
#endif
