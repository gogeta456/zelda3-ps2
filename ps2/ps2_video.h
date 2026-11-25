#ifndef Z3_PS2_VIDEO_H
#define Z3_PS2_VIDEO_H
#include <stdbool.h>
#include <stdint.h>
struct RendererFuncs;
void Ps2VideoCreate(struct RendererFuncs *f);
void Ps2VideoOptions(bool filter,int aspect,bool show_fps);
void Ps2VideoMenuBegin(void);
void Ps2VideoRect(int x,int y,int w,int h,uint32_t rgb);
void Ps2VideoText(int x,int y,const char *text,int scale,uint32_t rgb);
void Ps2VideoMenuEnd(void);
int Ps2VideoFps(void);
int Ps2VideoMode(void);                /* 0: 480i, 1: 240p, 2: 480p */
bool Ps2VideoSetMode(int mode);        /* run-time switch; false: kept/fell back to 480i */
bool Ps2VideoConfirm(const char *mode);/* 10 s prompt: X keeps, O or timeout reverts */
// Per-stage timing (EE cycle counter, 294912 cycles per ms), shown under the FPS counter.
typedef struct Ps2Prof { uint64_t logic, ppu, gsbuild, gsrun, vsync, sync, strips; uint32_t frames, uploads; } Ps2Prof;
extern Ps2Prof g_ps2_prof;
static inline uint32_t Ps2Cycles(void){uint32_t c;__asm__ volatile("mfc0 %0, $9":"=r"(c));return c;}
#endif
