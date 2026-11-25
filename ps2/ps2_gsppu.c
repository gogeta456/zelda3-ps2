#include "ps2_gsppu.h"
#include <string.h>
#include <stdlib.h>

// ---------------------------------------------------------------------------------------------------------------------
// Per-line recording (portable; the EE side only needs this part while the PPU runs)
// ---------------------------------------------------------------------------------------------------------------------
typedef struct LineState {
  uint16_t h[3], v[3], map[3], tile[3];
  uint8_t flags[3];  // bit0: tilemap wider, bit1: tilemap higher
  uint8_t layers;    // screenEnabled[0] & 0x17 (BG1..BG3, OBJ)
  uint8_t objSize, pad;
  uint16_t obj1, obj2;
  uint8_t mathMask, subLayer, mflags, fr, fg, fb;  // see GsppuRow in gsppu-test/state.h
} LineState;

static struct {
  bool enabled;
  const Ppu *ppu;
  unsigned count, strips;
  bool have_last;
  LineState last;
  uint8_t has[Z3_GSPPU_LINES];
  LineState st[Z3_GSPPU_LINES];
} F;

static int g_max_strips = Z3_GSPPU_MAX_STRIPS;
static bool g_use_scissor = true, g_has_alpha = true;
void Ps2GsPpuConfigure(int max_strips, bool use_scissor, bool has_alpha) {
  g_max_strips = max_strips < 1 ? 1 : max_strips > Z3_GSPPU_MAX_STRIPS ? Z3_GSPPU_MAX_STRIPS : max_strips;
  g_use_scissor = use_scissor;
  g_has_alpha = has_alpha;
}

void Ps2GsPpuBegin(bool enabled) {
  F.enabled = enabled;
  F.count = F.strips = 0;
  F.have_last = false;
  memset(F.has, 0, sizeof(F.has));
}

bool Ps2GsPpuHasLine(unsigned y) { return y < Z3_GSPPU_LINES && F.has[y]; }
unsigned Ps2GsPpuLineCount(void) { return F.count; }

#ifdef Z3_GSPPU_CAPTURE
#include <stdio.h>
static unsigned long long g_rej[12], g_cnt[64]; static char g_keys[64][80]; static int g_nk;
static void PrintRej(void) { for (int j = 0; j < g_nk; j++) fprintf(stderr, "MATH %llu %s\n", g_cnt[j], g_keys[j]); fprintf(stderr, "GSPPU rejects: mode=%llu blank=%llu extra=%llu clip=%llu window=%llu mosaic=%llu math=%llu limit=%llu ok=%llu\n", g_rej[6], g_rej[7], g_rej[8], g_rej[9], g_rej[1], g_rej[2], g_rej[3], g_rej[4], g_rej[5]); }
#define REJECT(n) do { if (!g_rej[11]) { atexit(PrintRej); g_rej[11] = 1; } g_rej[n]++; return false; } while (0)
#else
#define REJECT(n) return false
#endif
bool Ps2GsPpuLine(const Ppu *ppu, unsigned y) {
  if (!F.enabled || y >= Z3_GSPPU_LINES)
    return false;
  // Same conditions the software path uses before handing a line to the GS palette lookup: no color window, and no
  // color math that changes anything. Windows, mosaic and mode 7 are not drawn by the GS path at all.
  if (ppu->mode != 1) REJECT(6);
  if (ppu->forcedBlank) REJECT(7);
  if (ppu->extraLeftRight) REJECT(8);
  unsigned en = ppu->screenEnabled[0];
  if (ppu->screenWindowed[0] & en & 0x1f)
    REJECT(1);
  if (ppu->mosaicEnabled & en & 7)
    REJECT(2);
  // Color window (layer 5): when both windows are off or empty its state is the same across the whole line.
  unsigned wf = ppu->windowsel >> 20;
  bool w1 = (wf & 2) && ppu->window1left <= ppu->window1right, w2 = (wf & 8) && ppu->window2left <= ppu->window2right;
  bool uniform = !w1 && !w2, cw = ((wf & 3) == 3) || ((wf & 12) == 12);  // cw: inside-state when uniform
  if (ppu->clipMode == 3)
    REJECT(9);
  if (ppu->clipMode) {  // clip to black inside (2) or outside (1) the color window
    if (!uniform || (ppu->clipMode == 2 ? cw : !cw))
      REJECT(9);
  }
  uint8_t mm = 0, sub = 0xff, mf = 0, fix[3] = {0, 0, 0};
  if (ppu->mathEnabled && ppu->preventMathMode != 3) {
    bool noop = !(ppu->addSubscreen && ppu->screenEnabled[1]) && !ppu->halfColor &&
                !(ppu->fixedViewR | ppu->fixedViewG | ppu->fixedViewB);
    bool prevented = uniform && ((ppu->preventMathMode == 2 && cw) || (ppu->preventMathMode == 1 && !cw));
    if (!noop && !prevented) {
      if (!g_has_alpha || (ppu->preventMathMode && !uniform))
        REJECT(3);  // no alpha channel for the math mask, or math applies on part of the line only
      unsigned en1 = ppu->screenEnabled[1];
      if (ppu->addSubscreen && en1) {
        // The subscreen is added on top of the main screen: only a single BG on it can be drawn straight onto the main screen.
        unsigned sl = en1 & 0x17;
        if ((sl != 1 && sl != 2 && sl != 4) || (sl & en) || (ppu->screenWindowed[1] & en1 & 0x1f) || (ppu->mosaicEnabled & en1 & 7))
          REJECT(3);
        sub = sl == 1 ? 0 : sl == 2 ? 1 : 2;
      }
      mm = ppu->mathEnabled & 0x37;
      if (mm) {
        mf = (ppu->halfColor ? 1 : 0) | (ppu->subtractColor ? 2 : 0) | (ppu->addSubscreen ? 4 : 0);
        fix[0] = ppu->fixedViewR, fix[1] = ppu->fixedViewG, fix[2] = ppu->fixedViewB;
      } else {
        sub = 0xff;
      }
    }
  }
  LineState s;
  memset(&s, 0, sizeof(s));
  unsigned subbit = sub == 0xff ? 0 : 1u << sub;
  for (int i = 0; i < 3; i++) {
    if (!((en | subbit) & (1 << i)))
      continue;  // hidden layers keep zeros so they do not split strips
    const BgLayer *b = &ppu->bgLayer[i];
    s.h[i] = b->hScroll, s.v[i] = b->vScroll, s.map[i] = b->tilemapAdr, s.tile[i] = b->tileAdr;
    s.flags[i] = (b->tilemapWider ? 1 : 0) | (b->tilemapHigher ? 2 : 0);
  }
  s.layers = en & 0x17;
  s.mathMask = mm, s.subLayer = sub, s.mflags = mf, s.fr = fix[0], s.fg = fix[1], s.fb = fix[2];
  if (en & 0x10)
    s.objSize = ppu->objSize, s.obj1 = ppu->objTileAdr1, s.obj2 = ppu->objTileAdr2;
  if (!(F.have_last && !memcmp(&s, &F.last, sizeof(s)))) {
    if (F.strips >= (unsigned)g_max_strips)
      REJECT(4);  // too many HDMA changes: the rest of the frame goes through the software path
    F.strips++;
    F.last = s;
    F.have_last = true;
  }
#ifdef Z3_GSPPU_CAPTURE
  g_rej[5]++;
#endif
  F.ppu = ppu;
  F.has[y] = 1;
  F.st[y] = s;
  F.count++;
  return true;
}


#ifdef Z3_GSPPU_CAPTURE
// Host test hook: writes the rows recorded this frame plus VRAM/OAM/palette (state) and the software picture (rgba).
#include <stdio.h>
#include <stdlib.h>
#include "ps2/gsppu-test/state.h"
void Ps2GsPpuCapture(const Ppu *ppu, const uint8_t *pix, int pitch, const char *prefix, int frame) {
  if (getenv("Z3_STATONLY")) {
    static uint16_t prev[0x8000]; static int first = 1; int ch = 0, lo = 0x8000, hi = -1;
    for (int t = 0; t < 2048; t++) if (memcmp(&ppu->vram[t * 16], &prev[t * 16], 32)) { ch++; if (t < lo) lo = t; if (t > hi) hi = t; }
    int rowsgs = 0; for (int y = 0; y < 224; y++) rowsgs += F.has[y];
    if (!first) fprintf(stderr, "VRAMSTAT %d %d %d %d %d\n", frame, ch, lo, hi, rowsgs);
    first = 0; memcpy(prev, ppu->vram, sizeof(prev)); return;
  }
  static GsppuState st;
  _Static_assert(sizeof(GsppuRow) == sizeof(LineState), "layout");
  memset(&st, 0, sizeof(st));
  st.magic = GSPPU_MAGIC;
  memcpy(st.oam, ppu->oam, sizeof(st.oam)); memcpy(st.vram, ppu->vram, sizeof(st.vram));
  for (int i = 0; i < 256; i++) st.palRgb[i] = ppu->palRgb[i];
  memcpy(st.bmult, ppu->brightnessMult, sizeof(ppu->brightnessMult) < sizeof(st.bmult) ? sizeof(ppu->brightnessMult) : sizeof(st.bmult));
  for (int y = 0; y < 224; y++) { st.has[y] = F.has[y]; memcpy(&st.rows[y], &F.st[y], sizeof(LineState)); }
  char path[512];
  snprintf(path, sizeof(path), "%s_%06d.state", prefix, frame);
  FILE *f = fopen(path, "wb"); if (f) { fwrite(&st, sizeof(st), 1, f); fclose(f); }
  snprintf(path, sizeof(path), "%s_%06d.rgba", prefix, frame);
  f = fopen(path, "wb"); if (f) { for (int y = 0; y < 224; y++) fwrite(pix + (size_t)y * pitch, 1, 1024, f); fclose(f); }
}
#endif

#ifdef _EE
// ---------------------------------------------------------------------------------------------------------------------
// GS drawing
// ---------------------------------------------------------------------------------------------------------------------
#include <dmaKit.h>
#include <gsInline.h>
#include "ps2/ps2_video.h"
#include <malloc.h>

enum {
  kAtlas4W = 256, kAtlas4H = 512,    // 4bpp tiles, 32 per row, tile t at ((t & 31) * 8, (t >> 5) * 8)
  kAtlas2W = 512, kAtlas2H = 512,    // 2bpp tiles, 64 per row
  kClutCount = 32,  // 4 sets of 8 palettes: BG1, BG2, BG3 (2bpp), OBJ
};

#ifndef Z3_GSPPU_ATE
#define Z3_GSPPU_ATE 1
#endif
static GSGLOBAL *gs;
static uint8_t *atlas4, *atlas2;
static uint32_t atlas4_vram, atlas2_vram, clut_vram[kClutCount];
static uint32_t clut_mem[kClutCount][16] __attribute__((aligned(128)));
static int clut_code[4] = {-1, -1, -1, -1};  // alpha variant currently uploaded per set
static uint32_t pal_copy[256];
static bool pal_ok, vram_ok;
static uint16_t shadow[0x8000];
static uint8_t blank4[2048], blank2[4096];

bool Ps2GsPpuInit(GSGLOBAL *g) {
  gs = g;
  if (!atlas4)
    atlas4 = memalign(128, kAtlas4W * kAtlas4H / 2);
  if (!atlas2)
    atlas2 = memalign(128, kAtlas2W * kAtlas2H / 2);
  if (!atlas4 || !atlas2)
    return false;
  memset(atlas4, 0, kAtlas4W * kAtlas4H / 2);
  memset(atlas2, 0, kAtlas2W * kAtlas2H / 2);
  for (int i = 0; i < 4; i++)
    clut_code[i] = -1;
  atlas4_vram = gsKit_vram_alloc(gs, gsKit_texture_size(kAtlas4W, kAtlas4H, GS_PSM_T4), GSKIT_ALLOC_USERBUFFER);
  atlas2_vram = gsKit_vram_alloc(gs, gsKit_texture_size(kAtlas2W, kAtlas2H, GS_PSM_T4), GSKIT_ALLOC_USERBUFFER);
  if (atlas4_vram == GSKIT_ALLOC_ERROR || atlas2_vram == GSKIT_ALLOC_ERROR)
    return false;
  for (int i = 0; i < kClutCount; i++) {
    clut_vram[i] = gsKit_vram_alloc(gs, gsKit_texture_size(8, 2, GS_PSM_CT32), GSKIT_ALLOC_USERBUFFER);
    if (clut_vram[i] == GSKIT_ALLOC_ERROR)
      return false;
  }
  vram_ok = pal_ok = false;
  return true;
}

// ---- VRAM -> atlas ----
static void ConvTile4(const uint16_t *vram, int t) {
  const uint16_t *w = &vram[t * 16];
  uint8_t *dst = atlas4 + ((t >> 5) * 8) * (kAtlas4W / 2) + (t & 31) * 4;
  uint32_t any = 0;
  for (int r = 0; r < 8; r++, dst += kAtlas4W / 2) {
    uint32_t a = w[r], b = w[r + 8];
    any |= a | b;
    for (int px = 0; px < 8; px += 2) {
      int s0 = 7 - px, s1 = 6 - px;
      uint32_t p0 = ((a >> s0) & 1) | ((a >> (8 + s0)) & 1) << 1 | ((b >> s0) & 1) << 2 | ((b >> (8 + s0)) & 1) << 3;
      uint32_t p1 = ((a >> s1) & 1) | ((a >> (8 + s1)) & 1) << 1 | ((b >> s1) & 1) << 2 | ((b >> (8 + s1)) & 1) << 3;
      dst[px >> 1] = (uint8_t)(p0 | p1 << 4);
    }
  }
  blank4[t] = any == 0;
}

static void ConvTile2(const uint16_t *vram, int t) {
  const uint16_t *w = &vram[t * 8];
  uint8_t *dst = atlas2 + ((t >> 6) * 8) * (kAtlas2W / 2) + (t & 63) * 4;
  uint32_t any = 0;
  for (int r = 0; r < 8; r++, dst += kAtlas2W / 2) {
    uint32_t a = w[r];
    any |= a;
    for (int px = 0; px < 8; px += 2) {
      int s0 = 7 - px, s1 = 6 - px;
      uint32_t p0 = ((a >> s0) & 1) | ((a >> (8 + s0)) & 1) << 1;
      uint32_t p1 = ((a >> s1) & 1) | ((a >> (8 + s1)) & 1) << 1;
      dst[px >> 1] = (uint8_t)(p0 | p1 << 4);
    }
  }
  blank2[t] = any == 0;
}

static void SyncVram(const Ppu *ppu) {
  const uint16_t *vram = ppu->vram;
  bool dirty = false;
  for (int t = 0; t < 2048; t++) {
    if (vram_ok && !memcmp(&vram[t * 16], &shadow[t * 16], 32))
      continue;
    ConvTile4(vram, t);
    ConvTile2(vram, t * 2);
    ConvTile2(vram, t * 2 + 1);
    dirty = true;
  }
  if (dirty) {
    g_ps2_prof.uploads++;
    memcpy(shadow, vram, sizeof(shadow));
    vram_ok = true;
    gsKit_texture_send_inline(gs, (u32 *)atlas4, kAtlas4W, kAtlas4H, atlas4_vram, GS_PSM_T4, kAtlas4W / 64, GS_CLUT_NONE);
    gsKit_texture_send_inline(gs, (u32 *)atlas2, kAtlas2W, kAtlas2H, atlas2_vram, GS_PSM_T4, kAtlas2W / 64, GS_CLUT_NONE);
  }
}

// ---- CGRAM -> 32 small CLUTs. Entry 0 is transparent (alpha 0). Opaque entries carry alpha 0x60, or 0xC0 when color
// math applies to that layer: the MSB of the alpha written to the frame buffer is the "math applies here" mask. ----
static void SyncPalette(const Ppu *ppu) {
  if (pal_ok && !memcmp(pal_copy, ppu->palRgb, sizeof(pal_copy)))
    return;
  memcpy(pal_copy, ppu->palRgb, sizeof(pal_copy));
  pal_ok = true;
  for (int i = 0; i < 4; i++)
    clut_code[i] = -1;
}

// code 0: plain; code 1: math applies (for the OBJ set only palettes 4-7)
static void EnsureClut(int set, int code) {
  if (clut_code[set] == code)
    return;
  clut_code[set] = code;
  for (int p = 0; p < 8; p++) {
    for (int i = 0; i < 16; i++) {
      uint32_t col = set < 2 ? pal_copy[p * 16 + i] : set == 2 ? (i < 4 ? pal_copy[p * 4 + i] : 0) : pal_copy[128 + p * 16 + i];
      bool valid = i && (set != 2 || i < 4);
      bool math = code && (set != 3 || p >= 4);
      clut_mem[set * 8 + p][i] = (col & 0xffffff) | (valid ? (math ? 0xC0000000u : 0x60000000u) : 0);
    }
    gsKit_texture_send_inline(gs, (u32 *)clut_mem[set * 8 + p], 8, 2, clut_vram[set * 8 + p], GS_PSM_CT32, 1, GS_CLUT_PALLETE);
  }
}

// ---- packet building: everything goes out as A+D entries through a scratch buffer ----
static u64 sc[2 * 16384];
static unsigned scn;
static int cur_tex = -1;

static void Flush(void) {
  if (!scn)
    return;
  u64 *p = gsKit_heap_alloc(gs, scn, scn * 16, GIF_AD);
  p[0] = GIF_TAG_AD(scn);
  p[1] = GIF_AD;
  memcpy(p + 2, sc, scn * 16);
  scn = 0;
}
#define PUSH(reg, val) do { sc[2 * scn] = (u64)(val); sc[2 * scn + 1] = (reg); scn++; } while (0)

static inline u64 XYZ(float x, float y) {
  return (u64)(gsKit_float_to_int_x(gs, x) & 0xffff) | (u64)(gsKit_float_to_int_y(gs, y) & 0xffff) << 16;
}

static inline int ClampUV(int v) { return v < 0 ? 0 : v > 16383 ? 16383 : v; }

// atlas: 0 = 4bpp tiles, 1 = 2bpp tiles; clut: index into the 24 CLUTs
static void SetTex(int atlas, int clut) {
  int key = atlas << 8 | clut;
  if (key == cur_tex)
    return;
  cur_tex = key;
  if (atlas == 0)
    PUSH(GS_TEX0_1, GS_SETREG_TEX0(atlas4_vram / 256, kAtlas4W / 64, GS_PSM_T4, 8, 9, 1, 0, clut_vram[clut] / 256, GS_PSM_CT32, 0, 0, 1));
  else
    PUSH(GS_TEX0_1, GS_SETREG_TEX0(atlas2_vram / 256, kAtlas2W / 64, GS_PSM_T4, 9, 9, 1, 0, clut_vram[clut] / 256, GS_PSM_CT32, 0, 0, 1));
}

// One 8x8 tile as a screen quad. (tu, tv) = texel position of the tile in its atlas.
static void TileQuad(float x, float y, float w, float h, int tu, int tv, int hf, int vf) {
  if (scn > 16000)
    Flush();
  // The GS samples at the pixel position, not its center, so the far edge must be exact (no -1/16 texel).
  int u0 = tu * 16, u1 = (tu + 8) * 16, v0 = tv * 16, v1 = (tv + 8) * 16;
  // Flipped: the UV runs backwards, so start 1/16 texel inside the tile or the first pixel would sample the next texel.
  if (hf) { u0 = (tu + 8) * 16 - 1; u1 = tu * 16 - 1; }
  if (vf) { v0 = (tv + 8) * 16 - 1; v1 = tv * 16 - 1; }
  PUSH(GS_UV, GS_SETREG_UV(ClampUV(u0), ClampUV(v0)));
  PUSH(GS_XYZ2, XYZ(x, y));
  PUSH(GS_UV, GS_SETREG_UV(ClampUV(u1), ClampUV(v1)));
  PUSH(GS_XYZ2, XYZ(x + w, y + h));
}

typedef struct Tile { int16_t x, y; uint16_t id; uint8_t pal, prio, hf, vf; } Tile;
static Tile tiles[3][36 * 36];
static int ntiles[3];

static void EnumBg(const Ppu *ppu, const LineState *s, int L, int y0, int y1) {
  const uint16_t *vram = ppu->vram;
  // The software PPU adds the 1-based line number to vScroll, so every BG row sits one line higher than the raw scroll.
  int hs = s->h[L], vs = s->v[L] + 1;
  bool wider = s->flags[L] & 1, higher = s->flags[L] & 2;
  int r0 = (y0 + vs) >> 3, r1 = (y1 - 1 + vs) >> 3;
  int c0 = hs >> 3, c1 = (hs + 255) >> 3, n = 0;
  Tile *out = tiles[L];
  for (int r = r0; r <= r1; r++) {
    int sy = (r << 3) - vs;
    int off = s->map[L] + ((r & 31) << 5);
    if ((r & 32) && higher)
      off += wider ? 0x800 : 0x400;
    int base0 = off & 0x7fff, base1 = (off + (wider ? 0x400 : 0)) & 0x7fff;
    for (int c = c0; c <= c1; c++) {
      unsigned t = vram[(((c & 32) ? base1 : base0) + (c & 31)) & 0x7fff];
      unsigned id = t & 0x3ff;
      unsigned abs = L < 2 ? ((s->tile[L] >> 4) + id) & 0x7ff : ((s->tile[L] >> 3) + id) & 0xfff;
      if (L < 2 ? blank4[abs] : blank2[abs])
        continue;
      out[n++] = (Tile){(int16_t)((c << 3) - hs), (int16_t)sy, (uint16_t)abs, (uint8_t)((t >> 10) & 7),
                        (uint8_t)((t >> 13) & 1), (uint8_t)((t >> 14) & 1), (uint8_t)((t >> 15) & 1)};
    }
  }
  ntiles[L] = n;
}

static float g_x0, g_y0, g_sx, g_sy;
#define SY(v) (g_y0 + (float)(v) * g_sy)

static void BgPass(int L, int prio) {
  const Tile *t = tiles[L];
  for (int pal = 0; pal < 8; pal++) {
    bool first = true;
    for (int i = 0; i < ntiles[L]; i++) {
      if (t[i].prio != prio || t[i].pal != pal)
        continue;
      if (first) {
        SetTex(L < 2 ? 0 : 1, L * 8 + pal);
        first = false;
      }
      int tu = L < 2 ? (t[i].id & 31) * 8 : (t[i].id & 63) * 8;
      int tv = L < 2 ? (t[i].id >> 5) * 8 : (t[i].id >> 6) * 8;
      TileQuad(g_x0 + t[i].x * g_sx, SY(t[i].y), 8 * g_sx, 8 * g_sy, tu, tv, t[i].hf, t[i].vf);
    }
  }
}

static const uint8_t kSpriteSizes[8][2] = {{8, 16}, {8, 32}, {8, 64}, {16, 32}, {16, 64}, {32, 64}, {16, 32}, {16, 32}};

// Sprites of one priority, from the highest OAM index to the lowest so that sprite 0 ends up on top.
static void SpritePass(const Ppu *ppu, const LineState *s, int prio, int y0, int y1) {
  const uint8_t *sizes = kSpriteSizes[s->objSize];
  for (int i = 127; i >= 0; i--) {
    int index = i * 2;
    int yy = ppu->oam[index] >> 8;
    if (yy == 0xf0)
      continue;
    int oam1 = ppu->oam[index + 1];
    if (((oam1 >> 12) & 3) != prio)
      continue;
    int high = ppu->oam[0x100 + (index >> 4)] >> (index & 15);
    int size = sizes[(high >> 1) & 1];
    int x = (ppu->oam[index] & 0xff) + (high & 1) * 256;
    x -= (x >= 256) * 512;
    if (x <= -size)
      continue;
    int adr = (oam1 & 0x100) ? s->obj2 : s->obj1;
    int hf = (oam1 & 0x4000) != 0, vf = (oam1 & 0x8000) != 0, n = size >> 3;
    int clut = 24 + ((oam1 & 0xe00) >> 9);
    for (int copy = 0; copy < 2; copy++) {
      int top = copy ? yy - 256 : yy;
      if (copy && yy + size <= 256)
        break;
      if (top + size <= y0 || top >= y1)
        continue;
      for (int tr = 0; tr < n; tr++) {
        int str = vf ? n - 1 - tr : tr;
        for (int tc = 0; tc < n; tc++) {
          int dx = x + tc * 8;
          if (dx <= -8 || dx >= 256)
            continue;
          int stc = hf ? n - 1 - tc : tc;
          int used = ((((oam1 & 0xff) >> 4) + str) << 4) | (((oam1 & 0xf) + stc) & 0xf);
          int abs = ((adr >> 4) + used) & 0x7ff;
          if (blank4[abs])
            continue;
          SetTex(0, clut);
          TileQuad(g_x0 + dx * g_sx, SY(top + tr * 8), 8 * g_sx, 8 * g_sy, (abs & 31) * 8, (abs >> 5) * 8, hf, vf);
        }
      }
    }
  }
}

#define TEST_BASE GS_SETREG_TEST(Z3_GSPPU_ATE, 6, 0x40, 0, 0, 0, 1, 1)
#define PRIM_TEX (6 | 1 << 4 | 1 << 8)  // sprite, textured, UV coordinates
// ALPHA register for the math blends: add Cs+Cd, half (Cs+Cd)/2, subtract Cd-Cs, half-subtract (Cd-Cs)/2
static u64 MathAlpha(bool subtract, bool half) {
  u64 fix = half ? 0x40 : 0x80;
  return subtract ? (1 | 0 << 2 | 2 << 4 | 2 << 6 | fix << 32) : half ? (0 | 1 << 2 | 2 << 4 | 1 << 6 | fix << 32) : (0 | 2 << 2 | 2 << 4 | 1 << 6 | fix << 32);
}

static void DrawStrip(const Ppu *ppu, const LineState *s, int y0, int y1) {
  int sub = s->subLayer, mm = s->mathMask;
  Flush();
  for (int L = 0; L < 3; L++)
    EnsureClut(L, (sub != L && (mm >> L & 1)) ? 1 : 0);
  EnsureClut(3, (mm & 0x10) ? 1 : 0);
  PUSH(GS_TEXFLUSH, 0);
  // scissor to this strip
  int sx0 = (int)g_x0, sx1 = (int)(g_x0 + 256 * g_sx) - 1, sy0 = (int)SY(y0), sy1 = (int)SY(y1) - 1;
  if (g_use_scissor)
    PUSH(GS_SCISSOR_1, GS_SETREG_SCISSOR(sx0, sx1, sy0, sy1));
  // backdrop (its alpha carries the math mask as well)
  uint32_t bd = ppu->palRgb[0];
  PUSH(GS_PRIM, 6);  // sprite, no texture
  PUSH(GS_RGBAQ, GS_SETREG_RGBAQ(bd & 255, (bd >> 8) & 255, (bd >> 16) & 255, (mm & 0x20) ? 0xC0 : 0x60, 0));
  PUSH(GS_XYZ2, XYZ(g_x0, SY(y0)));
  PUSH(GS_XYZ2, XYZ(g_x0 + 256 * g_sx, SY(y1)));
  PUSH(GS_PRIM, PRIM_TEX);
  PUSH(GS_RGBAQ, GS_SETREG_RGBAQ(0x80, 0x80, 0x80, 0x80, 0));
  cur_tex = -1;
  unsigned subbit = sub == 0xff ? 0 : 1u << sub;
  for (int L = 0; L < 3; L++)
    if ((s->layers | subbit) & (1 << L))
      EnumBg(ppu, s, L, y0, y1);
    else
      ntiles[L] = 0;
  bool obj = (s->layers & 0x10) != 0;
  // back to front (mode 1, BG3 priority bit set)
  if (s->layers & 4) BgPass(2, 0);
  if (obj) SpritePass(ppu, s, 0, y0, y1);
  if (obj) SpritePass(ppu, s, 1, y0, y1);
  if (s->layers & 2) BgPass(1, 0);
  if (s->layers & 1) BgPass(0, 0);
  if (obj) SpritePass(ppu, s, 2, y0, y1);
  if (s->layers & 2) BgPass(1, 1);
  if (s->layers & 1) BgPass(0, 1);
  if (obj) SpritePass(ppu, s, 3, y0, y1);
  if (s->layers & 4) BgPass(2, 1);
  if (mm) {
    bool half = s->mflags & 1, subtract = s->mflags & 2, addsub = s->mflags & 4;
    if (sub != 0xff) {
      // The single subscreen BG goes straight onto the main pixels whose alpha MSB says "math applies". Drawing it
      // overwrites that alpha with its own (MSB 0), which marks the pixel as done for the fixed color pass.
      PUSH(GS_TEST_1, GS_SETREG_TEST(Z3_GSPPU_ATE, 6, 0x40, 0, 1, 1, 1, 1));
      PUSH(GS_ALPHA_1, MathAlpha(subtract, half));
      PUSH(GS_PRIM, PRIM_TEX | 1 << 6);
      BgPass(sub, 0);
      BgPass(sub, 1);
    }
    // Fixed color on whatever the subscreen did not cover (halved only when there is no subscreen to add).
    PUSH(GS_TEST_1, GS_SETREG_TEST(0, 1, 0, 0, 1, 1, 1, 1));
    PUSH(GS_ALPHA_1, MathAlpha(subtract, half && !addsub));
    PUSH(GS_PRIM, 6 | 1 << 6);
    PUSH(GS_RGBAQ, GS_SETREG_RGBAQ(ppu->brightnessMult[s->fr], ppu->brightnessMult[s->fg], ppu->brightnessMult[s->fb], 0x80, 0));
    PUSH(GS_XYZ2, XYZ(g_x0, SY(y0)));
    PUSH(GS_XYZ2, XYZ(g_x0 + 256 * g_sx, SY(y1)));
    PUSH(GS_TEST_1, TEST_BASE);
    PUSH(GS_PRIM, PRIM_TEX);
    PUSH(GS_RGBAQ, GS_SETREG_RGBAQ(0x80, 0x80, 0x80, 0x80, 0));
  }
  Flush();
}

void Ps2GsPpuDraw(GSGLOBAL *g, float x0, float y0, float sx, float sy) {
  gs = g;
  if (!F.count || !F.ppu)
    return;
  const Ppu *ppu = F.ppu;
  g_x0 = x0, g_y0 = y0, g_sx = sx, g_sy = sy;
  uint32_t pc0 = Ps2Cycles();
  SyncVram(ppu);
  SyncPalette(ppu);
  uint32_t pc1 = Ps2Cycles();
  g_ps2_prof.sync += pc1 - pc0;
  scn = 0;
  PUSH(GS_TEX1_1, GS_SETREG_TEX1(1, 0, 0, 0, 0, 0, 0));
  PUSH(GS_CLAMP_1, 0);
  PUSH(GS_TEST_1, TEST_BASE);
  PUSH(GS_COLCLAMP, 1);
  for (int y = 0; y < Z3_GSPPU_LINES;) {
    if (!F.has[y]) { y++; continue; }
    int y1 = y + 1;
    while (y1 < Z3_GSPPU_LINES && F.has[y1] && !memcmp(&F.st[y1], &F.st[y], sizeof(LineState)))
      y1++;
    DrawStrip(ppu, &F.st[y], y, y1);
    y = y1;
  }
  Flush();
  g_ps2_prof.strips += Ps2Cycles() - pc1;
  if (g_use_scissor)
    gsKit_set_scissor(gs, GS_SCISSOR_RESET);
  gsKit_set_test(gs, GS_ATEST_OFF);
}
#endif  // _EE

#ifdef _EE
static void SetFrame(u32 vram_bytes, int width_px, int psm) {
  u64 *p = gsKit_heap_alloc(gs, 2, 32, GIF_AD);
  p[0] = GIF_TAG_AD(2);
  p[1] = GIF_AD;
  p[2] = (u64)(vram_bytes / 8192) | (u64)(width_px / 64) << 16 | (u64)psm << 24;
  p[3] = GS_FRAME_1 + gs->PrimContext;
  p[4] = 0;
  p[5] = GS_TEXFLUSH;
}

void Ps2GsPpuDrawOffscreen(GSGLOBAL *g, uint32_t vram_bytes) {
  gs = g;
  if (!F.count || !F.ppu)
    return;
  SetFrame(vram_bytes, 256, GS_PSM_CT32);
  Ps2GsPpuDraw(g, 0.0f, 0.0f, 1.0f, 1.0f);
  SetFrame(g->ScreenBuffer[g->ActiveBuffer & 1], g->Width, g->PSM);
}
#endif
