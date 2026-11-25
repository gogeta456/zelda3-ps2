// x86: builds a synthetic PPU state, renders it with the software PPU (reference) and dumps state.bin + ref.rgba.
// usage: gen_ref <seed> <layers-mask> <sprite-mode> <hdma-mode> [outdir]
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "snes/ppu.h"
#include "state.h"
static uint32_t rs;
static uint32_t rnd(void) { rs = rs * 1664525u + 1013904223u; return rs >> 8; }
int main(int argc, char **argv) {
  if (argc < 5) return 1;
  rs = atoi(argv[1]) * 7919u + 1;
  int layers = strtol(argv[2], 0, 0), sprmode = atoi(argv[3]), hdma = atoi(argv[4]);
  const char *dir = argc > 5 ? argv[5] : ".";
  static GsppuState st; memset(&st, 0, sizeof(st));
  Ppu *p = ppu_init(); ppu_reset(p); p->extraLeftRight = 0; PpuSetExtraSideSpace(p, 0, 0, 0); p->extraLeftCur = p->extraRightCur = p->extraBottomCur = 0;
  p->mode = 1; p->forcedBlank = false; p->brightness = 15; p->screenEnabled[0] = layers; p->screenEnabled[1] = 0;
  p->objSize = (rnd() & 1) ? 0 : 3;
  p->objTileAdr1 = (rnd() & 3) << 13; p->objTileAdr2 = p->objTileAdr1 + (((rnd() & 3) + 1) << 12);
  // VRAM: sparse random words
  for (int i = 0; i < 0x8000; i++) { uint32_t r = rnd(); p->vram[i] = (r % 5 == 0) ? 0 : (r >> 3) & 0xffff; }
  { const char *m = getenv("GEN_CLEARMASK"); if (m) { unsigned mask = strtoul(m, 0, 0); for (int i = 0; i < 0x8000; i++) p->vram[i] &= ~mask; } }
  // keep tile data realistic: some fully blank tiles
  for (int t = 0; t < 2048; t++) if (rnd() % 4 == 0) memset(&p->vram[t * 16], 0, 32);
  for (int i = 0; i < 256; i++) p->cgram[i] = rnd() & 0x7fff;
  for (int i = 0; i < 3; i++) {
    BgLayer *b = &p->bgLayer[i];
    b->tilemapAdr = ((rnd() % 16) << 10) & 0x7c00; b->tileAdr = (rnd() & 7) << 12;
    b->tilemapWider = rnd() & 1; b->tilemapHigher = rnd() & 1;
    b->hScroll = rnd() % 1024; b->vScroll = rnd() % 1024;
  }
  if (getenv("GEN_PATTERN")) {
    memset(p->vram, 0, sizeof(p->vram));
    for (int i = 0; i < 256; i++) p->cgram[i] = (i * 3 + 1) & 0x7fff | ((i * 5) & 31) << 5;
    for (int i = 0; i < 3; i++) { BgLayer *b = &p->bgLayer[i]; b->tilemapAdr = 0; b->tileAdr = 0x1000; b->tilemapWider = b->tilemapHigher = 0; b->hScroll = b->vScroll = 0; }
    // tile 1: pixel(x,y) = (x + 2*y) % 15 + 1
    for (int tile = 1; tile <= 2; tile++)
      for (int y = 0; y < 8; y++) {
        int w0 = 0, w1 = 0;
        for (int x = 0; x < 8; x++) {
          int px = (tile == 1 ? x : y) + 1, bit = 7 - x;
          w0 |= (px & 1) << bit | ((px >> 1) & 1) << (8 + bit);
          w1 |= ((px >> 2) & 1) << bit | ((px >> 3) & 1) << (8 + bit);
        }
        p->vram[0x1000 + tile * 16 + y] = w0; p->vram[0x1000 + tile * 16 + 8 + y] = w1;
      }
    for (int ty = 0; ty < 28; ty++) for (int tx = 0; tx < 32; tx++) {
      int e = 1 + (tx & 1);
      if (tx >= 8 && tx < 12) e |= 0x4000;
      if (tx >= 12 && tx < 16) e |= 0x8000;
      if (tx >= 16 && tx < 20) e |= 3 << 10;
      p->vram[ty * 32 + tx] = (tx < 24 && ty < 20) ? e : 0;
    }
  }
  // OAM
  for (int s = 0; s < 128; s++) {
    uint32_t r = rnd();
    int y = (r % 40 == 0) ? 0xf0 : rnd() % 224;
    if (sprmode == 0) y = 0xf0;
    uint16_t o1 = rnd() & 0xffff;
    if (sprmode == 2) o1 = (o1 & ~0x3000) | (2 << 12);   // single priority
    p->oam[s * 2] = (rnd() & 0xff) | y << 8; p->oam[s * 2 + 1] = o1;
  }
  for (int i = 0; i < 8; i++) p->oam[0x100 + i * 1] = 0;
  for (int i = 0; i < 0x10; i++) p->oam[0x100 + i] = rnd() & 0xffff;
  // snapshot of what the PS2 test needs (before rendering mutates scrolls through hdma)
  st.magic = GSPPU_MAGIC;
  for (int y = 0; y < 224; y++) {
    GsppuRow *r = &st.rows[y]; st.has[y] = 1;
    r->layers = layers & 0x17; r->objSize = p->objSize; r->obj1 = p->objTileAdr1; r->obj2 = p->objTileAdr2;
    for (int i = 0; i < 3; i++) {
      r->map[i] = p->bgLayer[i].tilemapAdr; r->tile[i] = p->bgLayer[i].tileAdr;
      r->flags[i] = p->bgLayer[i].tilemapWider | p->bgLayer[i].tilemapHigher << 1;
    }
  }
  // optional color math: GEN_MATH=mask,add,half,subtract,fr,fg,fb,sublayer(-1 none)
  int mmask = 0, madd = 0, mhalf = 0, msub = 0, mfr = 0, mfg = 0, mfb = 0, msl = -1;
  if (getenv("GEN_MATH")) {
    sscanf(getenv("GEN_MATH"), "%i,%d,%d,%d,%d,%d,%d,%d", &mmask, &madd, &mhalf, &msub, &mfr, &mfg, &mfb, &msl);
    p->mathEnabled = mmask; p->addSubscreen = madd; p->halfColor = mhalf; p->subtractColor = msub;
    p->fixedColorR = mfr; p->fixedColorG = mfg; p->fixedColorB = mfb;
    p->screenEnabled[1] = msl < 0 ? 0 : (1 << msl);
  }
  static uint8_t buf[256 * 224 * 4 + 256 * 4 * 32];
  PpuBeginDrawing(p, buf, 1024, kPpuRenderFlags_NewRenderer);
  int baseh[3], basev[3]; for (int i = 0; i < 3; i++) baseh[i] = p->bgLayer[i].hScroll, basev[i] = p->bgLayer[i].vScroll;
  for (int y = 0; y < 224; y++) {
    GsppuRow *r = &st.rows[y];
    r->mathMask = mmask & 0x37; r->subLayer = (mmask & 0x37) && madd && msl >= 0 ? msl : 0xff; r->mflags = mhalf | msub << 1 | madd << 2;
    r->fr = p->fixedViewR; r->fg = p->fixedViewG; r->fb = p->fixedViewB;
    if (!(mmask & 0x37)) { r->mflags = 0; r->fr = r->fg = r->fb = 0; }
  }
  memcpy(st.bmult, p->brightnessMult, sizeof(st.bmult) < sizeof(p->brightnessMult) ? sizeof(st.bmult) : sizeof(p->brightnessMult));
  for (int line = 0; line <= 224; line++) {
    int y = line - 1;
    if (y >= 0 && y < 224) {
      for (int i = 0; i < 3; i++) {
        int hs = baseh[i], vs = basev[i];
        if (hdma == 1 && i == 1) hs += (y / 16) % 2 ? 3 : 0;     // alternating bands
        if (hdma == 2) hs += y / 8;                              // many strips
        if (hdma == 3 && y >= 100) vs += 40;                     // one split
        st.rows[y].h[i] = hs; st.rows[y].v[i] = vs;
      }
    }
    if (line > 0) {
      for (int i = 0; i < 3; i++) { /* apply per-row scroll for the row rendered by this call */
        p->bgLayer[i].hScroll = st.rows[line - 1].h[i]; p->bgLayer[i].vScroll = st.rows[line - 1].v[i];
      }
    }
    ppu_runLine(p, line);
  }
  memcpy(st.oam, p->oam, sizeof(st.oam)); memcpy(st.vram, p->vram, sizeof(st.vram)); memcpy(st.palRgb, p->palRgb, sizeof(st.palRgb));
  char path[512];
  snprintf(path, sizeof(path), "%s/state.bin", dir); FILE *f = fopen(path, "wb"); fwrite(&st, sizeof(st), 1, f); fclose(f);
  snprintf(path, sizeof(path), "%s/ref.rgba", dir); f = fopen(path, "wb");
  for (int y = 0; y < 224; y++) fwrite(buf + y * 1024, 1, 1024, f);
  fclose(f);
  return 0;
}
