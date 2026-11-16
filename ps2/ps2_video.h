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
void Ps2VideoToggleStats(void);
void Ps2VideoStatsStart(void);
void Ps2VideoStatsStep(unsigned stage);
#endif
