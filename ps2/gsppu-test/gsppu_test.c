// PS2 test: renders a GsppuState through the GS PPU path at 1x and writes the 256x224 result to host:out.rgba
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <kernel.h>
#include <gsKit.h>
#include <dmaKit.h>
#include <dma.h>
#include "ps2/ps2_gsppu.h"
#include "state.h"
#include "ps2/ps2_video.h"
Ps2Prof g_ps2_prof;
#undef fopen

static GSGLOBAL *gs;
static GsppuState st;
static Ppu ppu;
static u64 pkt[10] __attribute__((aligned(64)));

static void ReadBack(u32 vram_bytes, int fbw64, int w, int h, void *dst) {
  pkt[0] = (u64)4 | (1ull << 15) | (0ull << 58) | (1ull << 60);  // NLOOP=4 EOP=1 FLG=PACKED NREG=1 (REGS=A+D)
  pkt[1] = 0xe;
  pkt[2] = GS_SETREG_BITBLTBUF(vram_bytes / 256, fbw64, GS_PSM_CT32, 0, 0, 0); pkt[3] = 0x50;
  pkt[4] = 0; pkt[5] = 0x51;                                  // TRXPOS
  pkt[6] = (u64)w | ((u64)h << 32); pkt[7] = 0x52;            // TRXREG
  pkt[8] = 1; pkt[9] = 0x53;                                  // TRXDIR: local -> host
  dma_channel_send_normal(DMA_CHANNEL_GIF, pkt, 5, 0, 0);
  dma_channel_wait(DMA_CHANNEL_GIF, 0);
  *(volatile u64 *)0x12001040 = 1;                            // BUSDIR: GS -> EE
  *(volatile u32 *)0x10003c00 = 1 << 23;                      // VIF1_STAT.FDR
  dma_channel_receive_normal(DMA_CHANNEL_VIF1, dst, w * h * 4, 0, 0);
  dma_channel_wait(DMA_CHANNEL_VIF1, 0);
  *(volatile u32 *)0x10003c00 = 0;
  *(volatile u64 *)0x12001040 = 0;
}

int main(void) {
  FILE *f = fopen("host:state.bin", "rb");
  if (!f || fread(&st, sizeof(st), 1, f) != 1 || st.magic != GSPPU_MAGIC) { FILE *e = fopen("host:out.txt", "wb"); if (e) { fputs("bad state\n", e); fclose(e); } SleepThread(); }
  fclose(f);
  gs = gsKit_init_global();
  gs->Mode = GS_MODE_NTSC; gs->Interlace = GS_INTERLACED; gs->Field = GS_FIELD;
  gs->Width = 640; gs->Height = 448; gs->PSM = GS_PSM_CT32; gs->ZBuffering = GS_SETTING_OFF;
  gs->DoubleBuffering = GS_SETTING_ON; gs->PrimAlphaEnable = GS_SETTING_OFF;
  dmaKit_init(D_CTRL_RELE_OFF, D_CTRL_MFD_OFF, D_CTRL_STS_UNSPEC, D_CTRL_STD_OFF, D_CTRL_RCYC_8, 1 << DMA_CHANNEL_GIF);
  dmaKit_chan_init(DMA_CHANNEL_GIF);
  dma_channel_initialize(DMA_CHANNEL_VIF1, NULL, 0);
  gsKit_init_screen(gs); gsKit_mode_switch(gs, GS_ONESHOT);
  int ok = Ps2GsPpuInit(gs);
  // fill the PPU from the snapshot
  ppu.mode = 1;
  memcpy(ppu.oam, st.oam, sizeof(ppu.oam)); memcpy(ppu.vram, st.vram, sizeof(ppu.vram)); memcpy(ppu.palRgb, st.palRgb, sizeof(ppu.palRgb));
  memcpy(ppu.brightnessMult, st.bmult, sizeof(ppu.brightnessMult) < 64 ? sizeof(ppu.brightnessMult) : 64);
  Ps2GsPpuBegin(true);
  for (int y = 0; y < 224; y++) {
    if (!st.has[y]) continue;
    const GsppuRow *r = &st.rows[y];
    ppu.mathEnabled = r->mathMask; ppu.addSubscreen = (r->mflags >> 2) & 1; ppu.halfColor = r->mflags & 1; ppu.subtractColor = (r->mflags >> 1) & 1;
    ppu.fixedViewR = r->fr; ppu.fixedViewG = r->fg; ppu.fixedViewB = r->fb; ppu.preventMathMode = 0; ppu.clipMode = 0;
    ppu.screenEnabled[1] = (r->subLayer == 0xff) ? 0 : (1 << r->subLayer);
    ppu.screenEnabled[0] = r->layers; ppu.objSize = r->objSize; ppu.objTileAdr1 = r->obj1; ppu.objTileAdr2 = r->obj2;
    for (int i = 0; i < 3; i++) {
      ppu.bgLayer[i].hScroll = r->h[i]; ppu.bgLayer[i].vScroll = r->v[i];
      ppu.bgLayer[i].tilemapAdr = r->map[i]; ppu.bgLayer[i].tileAdr = r->tile[i];
      ppu.bgLayer[i].tilemapWider = r->flags[i] & 1; ppu.bgLayer[i].tilemapHigher = (r->flags[i] >> 1) & 1;
    }
    Ps2GsPpuLine(&ppu, y);
  }
  gsKit_clear(gs, GS_SETREG_RGBAQ(0, 0, 0, 0x80, 0));
#ifdef TEST_OFFSCREEN
  // GS rows into an offscreen buffer, then that buffer drawn to the screen as a texture (nearest, 1:1)
  u32 off = gsKit_vram_alloc(gs, gsKit_texture_size(256, 224, GS_PSM_CT32), GSKIT_ALLOC_USERBUFFER);
  GSTEXTURE otex; memset(&otex, 0, sizeof(otex)); otex.Width = 256; otex.Height = 224; otex.PSM = GS_PSM_CT32; otex.Vram = off; otex.TBW = 4; otex.Filter = GS_FILTER_NEAREST; otex.VramClut = 0;
  { FILE *e = fopen("host:align.txt", "wb"); if (e) { fprintf(e, "off=%u mod8192=%u\n", off, off % 8192); fclose(e); } }
  Ps2GsPpuDrawOffscreen(gs, off);
  gsKit_prim_sprite_texture(gs, &otex, 0.0f, 0.0f, 0.0f, 0.0f, 256.0f, 224.0f, 256.0f, 224.0f, 1, GS_SETREG_RGBAQ(0x80, 0x80, 0x80, 0x80, 0));
#else
  Ps2GsPpuDraw(gs, 0.0f, 0.0f, 1.0f, 1.0f);
#endif
  gsKit_queue_exec(gs); gsKit_finish(); for (int i = 0; i < 6; i++) gsKit_vsync_wait();
  u32 *img = memalign(64, 256 * 224 * 4);
  memset(img, 0xAA, 256 * 224 * 4); SyncDCache(img, (u8 *)img + 256 * 224 * 4);
  ReadBack(gs->ScreenBuffer[gs->ActiveBuffer & 1], 10, 256, 224, img);
  SyncDCache(img, (u8 *)img + 256 * 224 * 4);
  FILE *o = fopen("host:out.rgba", "wb"); if (o) { fwrite(img, 1, 256 * 224 * 4, o); fclose(o); }
  o = fopen("host:out.txt", "wb"); if (o) { fprintf(o, "init=%d lines=%u\n", ok, Ps2GsPpuLineCount()); fclose(o); }
  SleepThread();
  return 0;
}
