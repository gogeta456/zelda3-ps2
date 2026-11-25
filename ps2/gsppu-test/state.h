// Binary snapshot shared by the x86 tools and the PS2 test ELF (little endian, natural alignment is identical on both).
#pragma once
#include <stdint.h>
typedef struct GsppuRow {            // same layout as LineState in ps2_gsppu.c
  uint16_t h[3], v[3], map[3], tile[3];
  uint8_t flags[3];                  // bit0 wider, bit1 higher
  uint8_t layers, objSize, pad;
  uint16_t obj1, obj2;
  uint8_t mathMask, subLayer, mflags, fr, fg, fb;   // color math: mask of layers, BG on the subscreen (0xff none), bit0 half bit1 subtract bit2 addSubscreen, fixed color (5 bit)
} GsppuRow;
typedef struct GsppuState {
  uint32_t magic;                    // 'GSPP'
  uint16_t oam[0x110];
  uint32_t palRgb[256];
  uint16_t vram[0x8000];
  uint8_t has[224];                  // rows the GS path takes
  GsppuRow rows[224];
  uint8_t bmult[64];                 // ppu->brightnessMult
} GsppuState;
#define GSPPU_MAGIC 0x50505347u
