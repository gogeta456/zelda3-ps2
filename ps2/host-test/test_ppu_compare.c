#include "snes/ppu.h"
#include "ps2/ps2_indexed.h"
#include <assert.h>

Ppu *Reference_ppu_init(void);
void Reference_PpuBeginDrawing(Ppu *, uint8_t *, size_t, uint32_t);
void Reference_ppu_runLine(Ppu *, int);
void Reference_ppu_write(Ppu *, uint8_t, uint8_t);
static uint32_t state = 0x5132;
static uint32_t Random(void) { return state = state * 1664525u + 1013904223u; }
static void WriteBoth(Ppu *a, Ppu *b, uint8_t reg, uint8_t value) {
  ppu_write(a, reg, value); Reference_ppu_write(b, reg, value);
}
int main(void) {
  Ppu *a = ppu_init(), *b = Reference_ppu_init();
  b = realloc(b, sizeof(*b)); assert(a && b);
  static uint32_t expected[224][256], actual[224][256];
  for (int test = 0; test < 1024; test++) {
    ppu_reset(a);
    a->extraLeftRight = a->extraLeftCur = a->extraRightCur = a->extraBottomCur = 0;
    a->forcedBlank = false; a->brightness = test & 15;
    a->mode = test % 11 ? 1 : 7;
    a->clipMode = test >> 2 & 3; a->preventMathMode = test >> 4 & 3;
    a->mathEnabled = (test * 13) & 63;
    a->addSubscreen = (test & 1) != 0; a->halfColor = (test & 2) != 0;
    a->subtractColor = (test & 256) != 0;
    a->screenEnabled[0] = 31; a->screenEnabled[1] = test % 3 ? 0 : 18;
    a->screenWindowed[0] = Random() & 31; a->screenWindowed[1] = Random() & 31;
    a->window1left = 29; a->window1right = 91;
    a->window2left = 75; a->window2right = 237; a->windowsel = Random() & 0xffffff;
    a->mosaicEnabled = test % 7 == 0 ? 7 : 0; a->mosaicSize = 1 + (test & 15);
    a->fixedColorR = Random() & 31; a->fixedColorG = Random() & 31; a->fixedColorB = Random() & 31;
    a->objSize = test & 7; a->objTileAdr1 = 0x4000; a->objTileAdr2 = 0x6000;
    for (int i = 0; i < 8; i++) a->m7matrix[i] = (int16_t)(Random() >> 20) - 2048;
    for (unsigned i = 0; i < 0x8000; i++) a->vram[i] = Random() >> 12;
    for (unsigned i = 0; i < 256; i++) a->cgram[i] = Random() >> 17;
    for (unsigned i = 0; i < 0x110; i++) a->oam[i] = Random() >> 12;
    for (unsigned i = 0; i < 3; i++) {
      a->bgLayer[i] = (BgLayer){.hScroll = Random() & 511, .vScroll = Random() & 511,
        .tilemapAdr = (Random() & 15) << 11, .tileAdr = (Random() & 7) << 12,
        .tilemapWider = (test & 1) != 0, .tilemapHigher = (test & 2) != 0};
    }
    PpuSetSaturation(a, test % 5 == 0 ? 30 : 100);
    for (int phase = 0; phase < 3; phase++) {
      if (phase == 2) {
        // Changes outside ppu_write must also invalidate derived palettes.
        a->cgram[(test * 7) & 255] ^= 31;
        a->brightness ^= 1;
        a->fixedColorR ^= 7;
        a->halfColor = !a->halfColor;
        if (test == 511) { a->mixedGeneration = UINT32_MAX; a->mixedValid = false; }
      }
    memcpy(b, a, sizeof(*a));
    uint32_t flags = kPpuRenderFlags_NewRenderer | ((test & 1) ? kPpuRenderFlags_NoSpriteLimits : 0);
    PpuBeginDrawing(a, (uint8_t *)actual, 1024, flags);
    Reference_PpuBeginDrawing(b, (uint8_t *)expected, 1024, flags);
    Ps2IndexedBegin(test % 3 != 0);
    for (int y = 1; y <= 224; y++) {
      if (y == 73 || y == 149) {
        WriteBoth(a, b, 0x21, 0); // Change CGRAM and fixed color mid-frame.
        WriteBoth(a, b, 0x22, Random()); WriteBoth(a, b, 0x22, Random() & 127);
        WriteBoth(a, b, 0x32, 0xe0 | (Random() & 31));
        WriteBoth(a, b, 0x02, 0); WriteBoth(a, b, 0x03, 0);
        WriteBoth(a, b, 0x04, Random()); WriteBoth(a, b, 0x04, y);
      }
      ppu_runLine(a, y); Reference_ppu_runLine(b, y);
    }
    Ps2IndexedResolve((uint8_t *)actual, 1024);
    if (memcmp(expected, actual, sizeof(expected))) {
      fprintf(stderr, "PPU mismatch in synthetic frame %d, phase %d\n", test, phase); return 1;
    }
    }
  }
  ppu_free(a); ppu_free(b);
  puts("PASS: 3072 synthetic PPU frames; warm caches, direct palette/brightness changes, generation wrap; windows, math, sprites, mosaic, Mode 7, mid-frame palette changes");
  return 0;
}
