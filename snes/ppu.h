
#ifndef ZELDA3_SNES_PPU_H_
#define ZELDA3_SNES_PPU_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "snes/saveload.h"
typedef struct Ppu Ppu;

#include "src/types.h"

// Output pixel order. The PS2's GS reads 32-bit textures as bytes R,G,B,A (= SDL_PIXELFORMAT_ABGR8888), so building the frame
// in that order lets SDL hand it to the GS as is, without a slow per-frame format conversion.
#if defined(_EE) && !defined(Z3_PIXEL_ABGR)
#define Z3_PIXEL_ABGR 1
#endif
#ifdef Z3_PIXEL_ABGR
#define PPU_RGB(r, g, b) ((uint32_t)(r) | (uint32_t)(g) << 8 | (uint32_t)(b) << 16)
#else
#define PPU_RGB(r, g, b) ((uint32_t)(r) << 16 | (uint32_t)(g) << 8 | (uint32_t)(b))
#endif

typedef struct BgLayer {
  uint16_t hScroll;
  uint16_t vScroll;
  // -- snapshot starts here
  bool tilemapWider;
  bool tilemapHigher;
  uint16_t tilemapAdr;
  // -- snapshot ends here
  uint16_t tileAdr;
} BgLayer;

enum {
  kPpuXPixels = 256 + kPpuExtraLeftRight * 2,
};

typedef uint16_t PpuZbufType;

typedef struct PpuPixelPrioBufs {
  // This holds the prio in the upper 8 bits and the color in the lower 8 bits.
  PpuZbufType data[kPpuXPixels];
} PpuPixelPrioBufs;

enum {
  kPpuRenderFlags_NewRenderer = 1,
  // Render mode7 upsampled by 4x4
  kPpuRenderFlags_4x4Mode7 = 2,
  // Use 240 height instead of 224
  kPpuRenderFlags_Height240 = 4,
  // Disable sprite render limits
  kPpuRenderFlags_NoSpriteLimits = 8,
};


struct Ppu {
  bool lineHasSprites;
  uint8_t lastBrightnessMult;
  uint8_t lastMosaicModulo;
  uint8_t renderFlags;
  uint32_t renderPitch;
  uint8_t *renderBuffer;
  uint8_t extraLeftCur, extraRightCur, extraLeftRight, extraBottomCur;
  float mode7PerspectiveLow, mode7PerspectiveHigh;

  // TMW / TSW etc
  uint8 screenEnabled[2];
  uint8 screenWindowed[2];
  uint8 mosaicEnabled;
  uint8 mosaicSize;
  // object/sprites
  uint16_t objTileAdr1;
  uint16_t objTileAdr2;
  uint8_t objSize;
  // Window
  uint8_t window1left;
  uint8_t window1right;
  uint8_t window2left;
  uint8_t window2right;
  uint32_t windowsel;

  // color math
  uint8_t clipMode;
  uint8_t preventMathMode;
  bool addSubscreen;
  bool subtractColor;
  bool halfColor;
  uint8 mathEnabled;
  uint8_t fixedColorR, fixedColorG, fixedColorB;
  // settings
  bool forcedBlank;
  uint8_t brightness;
  uint8_t mode;

  // vram access
  uint16_t vramPointer;
  uint16_t vramIncrement;
  bool vramIncrementOnHigh;
  // cgram access
  uint8_t cgramPointer;
  bool cgramSecondWrite;
  uint8_t cgramBuffer;
  // oam access
  uint16_t oamAdr;
  bool oamSecondWrite;
  uint8_t oamBuffer;

  // background layers
  BgLayer bgLayer[4];
  uint8_t scrollPrev;
  uint8_t scrollPrev2;
  
  // mode 7
  int16_t m7matrix[8]; // a, b, c, d, x, y, h, v
  uint8_t m7prev;
  bool m7largeField;
  bool m7charFill;
  bool m7xFlip;
  bool m7yFlip;
  bool m7extBg_always_zero;
  // mode 7 internal
  int32_t m7startX;
  int32_t m7startY;

  uint16_t oam[0x110];
  
  // store 31 extra entries to remove the need for clamp
  uint8_t brightnessMult[32 + 31];
  uint8_t brightnessMultHalf[32 * 2];
  uint16_t cgram[0x100];
  uint8_t mosaicModulo[kPpuXPixels];
  uint32_t colorMapRgb[256];
  PpuPixelPrioBufs bgBuffers[2];
  PpuPixelPrioBufs objBuffer;
  uint16_t vram[0x8000];
  // Saturation setting: renderers read cgramView/fixedView* (cgram/fixedColor* with the saturation applied)
  uint16_t saturation;  // 256 = unchanged
  uint8_t fixedViewR, fixedViewG, fixedViewB;
  uint16_t cgramView[0x100];
  uint32_t paletteRevision; // GS palette cache invalidation, not serialized
  uint32_t palRgb[0x100];  // cgramView as final output pixels (brightness applied)
  // Derived palettes only; these fields are not part of a saved game.
  uint32_t fixedPalette[2][256]; // unclipped / clipped-to-black main screen
  uint32_t fixedPaletteKey, fixedPaletteRevision;
  bool fixedPaletteValid;
  uint32_t gsMathPalette[256], gsMathKey, gsMathSourceRevision, gsMathRevision;
  bool gsMathValid;
  // Visible OAM entries, in original OAM order. Derived, never serialized.
  uint8_t spriteRowCount[256], spriteRows[256][128];
  bool spriteRowsValid;
  // Subscreen color math: rows are built only for secondary colors in use.
  // Derived state, excluded from SRAM/snapshots; no large per-frame memset.
  uint32_t mixedPalette[256][256], mixedRowGeneration[256];
  uint32_t mixedZero[256], mixedBackdrop[256];
  uint32_t mixedGeneration, mixedSourceRevision, mixedKey;
  bool mixedValid;
  uint16_t paletteSource[256];
  uint8_t paletteSourceBrightness;
  bool paletteSourceValid;
};

Ppu* ppu_init();
void PpuSetSaturation(Ppu *ppu, int percent);  // 100 = original colors
void ppu_free(Ppu* ppu);
void ppu_reset(Ppu* ppu);
void ppu_handleVblank(Ppu* ppu);
void ppu_runLine(Ppu* ppu, int line);
uint8_t ppu_read(Ppu* ppu, uint8_t adr);
void ppu_write(Ppu* ppu, uint8_t adr, uint8_t val);
void ppu_saveload(Ppu *ppu, SaveLoadFunc *func, void *ctx);
void PpuBeginDrawing(Ppu *ppu, uint8_t *buffer, size_t pitch, uint32_t render_flags);

// Returns the current render scale, 1x = 256px, 2x=512px, 4x=1024px
int PpuGetCurrentRenderScale(Ppu *ppu, uint32_t render_flags);

void PpuSetMode7PerspectiveCorrection(Ppu *ppu, int low, int high);
void PpuSetExtraSideSpace(Ppu *ppu, int left, int right, int bottom);

#endif  // ZELDA3_SNES_PPU_H_
