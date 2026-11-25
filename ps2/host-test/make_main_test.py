#!/usr/bin/env python3
"""Generate main_test.c: src/main.c plus a few hooks for headless replay testing on a PC.
Not used by the PS2 build. Environment variables read by the generated program:
  Z3_REPLAY=n       play reference replay slot n (0..12 = chapters 1..13) from saves/ref
  Z3_FRAMES=n       stop after n frames
  Z3_DUMP=prefix    write <prefix>_<frame>.ppm screenshots ...
  Z3_DUMP_EVERY=n   ... every n frames (default 2000)
  Z3_HASH=file      write one hash of the finished picture per frame; two builds that print the same file draw identical frames
                    (this is how the PPU speed-ups were checked: 13 chapters x 2500 frames, bit-exact)
"""
import pathlib

import sys
here = pathlib.Path(__file__).resolve().parent
src = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else here / '../../src/main.c').read_text()   # CRLF files are fine


def replace_once(s, old, new):
    assert s.count(old) == 1, "anchor not found exactly once: " + old.strip()
    return s.replace(old, new, 1)


anchor = "static bool g_run_without_emu = 0;\n"
helper = anchor + r'''
// ---- headless test hooks (generated; not part of the PS2 build) ----
#ifdef Z3_GS_TEST
#include "ps2/ps2_indexed.h"
static unsigned long long g_indexed_total;
#endif
#ifdef Z3_GSPPU_CAPTURE
#include "ps2/ps2_gsppu.h"
void Ps2GsPpuCapture(const Ppu *ppu, const uint8_t *pix, int pitch, const char *prefix, int frame);
#endif
#ifdef Z3_FRAME_TEST
#include "ps2/ps2_frame.h"
static Ps2FrameClock g_test_clock;
#endif
static int g_test_frames;
static FILE *g_hash_file;
static uint32 ToArgb(uint32 p) {
#ifdef Z3_PIXEL_ABGR
  return (p & 0xff00) | ((p >> 16) & 0xff) | ((p & 0xff) << 16);
#else
  return p & 0xffffff;
#endif
}
static void TestDump(const uint8 *pix, int pitch, int w, int h) {
#ifdef Z3_GS_TEST
  Ps2IndexedResolve((uint8 *)pix, pitch);
  g_indexed_total += g_ps2_indexed.indexed_lines;
  if (g_test_frames % 2500 == 0) fprintf(stderr, "GS indexed scanlines: %llu\n", g_indexed_total);
#endif
#ifdef Z3_GSPPU_CAPTURE
  { const char *cp = getenv("Z3_CAPTURE"), *ce = getenv("Z3_CAPTURE_EVERY");
    int cev = ce ? atoi(ce) : 600;
    if (cp && cev > 0 && g_test_frames % cev == 0) Ps2GsPpuCapture(g_zenv.ppu, pix, pitch, cp, g_test_frames); }
#endif
  const char *hp = getenv("Z3_HASH");
  if (hp) {
    if (!g_hash_file) g_hash_file = fopen(hp, "w");
    uint64_t hsh = 1469598103934665603ULL;
    for (int y = 0; y < h; y++) {
      const uint32 *row = (const uint32 *)(pix + (size_t)y * pitch);
      for (int x = 0; x < w; x++) hsh = (hsh ^ ToArgb(row[x])) * 1099511628211ULL;
    }
    fprintf(g_hash_file, "%d %016llx\n", g_test_frames, (unsigned long long)hsh);
    if (g_test_frames % 50 == 0) fflush(g_hash_file);
  }
  const char *base = getenv("Z3_DUMP"); const char *iv = getenv("Z3_DUMP_EVERY");
  int every = iv ? atoi(iv) : 2000;
  if (!base || every <= 0 || g_test_frames % every != 0) return;
  char name[256]; snprintf(name, sizeof(name), "%s_%06d.ppm", base, g_test_frames);
  FILE *f = fopen(name, "wb"); if (!f) return;
  fprintf(f, "P6\n%d %d\n255\n", w, h);
  for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
    uint32 p = ToArgb(((const uint32 *)(pix + (size_t)y * pitch))[x]);
    fputc((p >> 16) & 255, f); fputc((p >> 8) & 255, f); fputc(p & 255, f);
  }
  fclose(f);
}
'''
src = replace_once(src, anchor, helper)
src = replace_once(src, "static void DrawPpuFrameWithPerf() {\n", "static void DrawPpuFrameWithPerf() {\n#ifdef Z3_GS_TEST\n  Ps2IndexedBegin(getenv(\"Z3_PALETTE\") != NULL);\n#endif\n#ifdef Z3_GSPPU_CAPTURE\n  Ps2GsPpuBegin(true);\n#endif\n")

a = "    RenderNumber(pixel_buffer + pitch * render_scale, pitch, g_curr_fps, render_scale == 4);\n"
src = replace_once(src, a, a + "  TestDump(pixel_buffer, pitch, g_snes_width * render_scale, g_snes_height * render_scale);\n")
a = "  ZeldaReadSram();\n"
src = replace_once(src, a, a + '  if (getenv("Z3_REPLAY")) SaveLoadSlot(kSaveLoad_Replay, 256 + atoi(getenv("Z3_REPLAY")));\n')
a = "    frameCtr++;\n"
src = replace_once(src, a, a + '    g_test_frames = frameCtr;\n    { static int lim = -1; if (lim < 0) lim = getenv("Z3_FRAMES") ? atoi(getenv("Z3_FRAMES")) : 0; if (lim > 0 && (int)frameCtr >= lim) running = false; }\n')
src = replace_once(src, "    DrawPpuFrameWithPerf();\n", "#ifdef Z3_FRAME_TEST\n    if (!Ps2FrameAdvance(&g_test_clock, getenv(\"Z3_PRESENT30\") == NULL, g_test_clock.deadline, true, true)) continue;\n#endif\n    DrawPpuFrameWithPerf();\n")
src = replace_once(src, "g_replay_turbo = true", "g_replay_turbo = false")   # draw every frame so the PPU code is exercised
pathlib.Path(sys.argv[2] if len(sys.argv) > 2 else here / 'main_test.c').write_text(src)
print("wrote main_test.c")
