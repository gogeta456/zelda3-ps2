#include "ps2/ps2_menu.h"
#include "ps2/ps2_video.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
static uint16_t script[128];static unsigned count,pos,frames;static uint32_t ticks;static uint8_t pixels[448][640][3];
static int capture_page;
uint32_t SDL_GetTicks(void){return ticks;}
void SDL_Delay(uint32_t ms){ticks+=ms;}
const char *ps2_save_location(void){return "mc0:/ZELDA3-LTTP/";}
Ps2PadSample Ps2PadPoll(void){assert(pos<count);return(Ps2PadSample){script[pos++],128,128,true,true};}
void Ps2VideoMenuBegin(void){for(int y=0;y<448;y++)for(int x=0;x<640;x++){pixels[y][x][0]=10;pixels[y][x][1]=19;pixels[y][x][2]=28;}}
void Ps2VideoRect(int x,int y,int w,int h,uint32_t rgb){assert(x>=0&&y>=0&&x+w<=640&&y+h<=448);for(int yy=y;yy<y+h;yy++)for(int xx=x;xx<x+w;xx++){pixels[yy][xx][0]=rgb>>16;pixels[yy][xx][1]=rgb>>8;pixels[yy][xx][2]=rgb;}}
void Ps2VideoMenuEnd(void){ticks+=17;frames++;if(capture_page&&pos==count-2){char path[80];snprintf(path,sizeof path,"menu-page-%d.ppm",capture_page);FILE *f=fopen(path,"wb");assert(f);fprintf(f,"P6\n640 448\n255\n");assert(fwrite(pixels,1,sizeof pixels,f)==sizeof pixels);fclose(f);}}
static void Reset(void){count=pos=0;script[count++]=0;}
static void Key(uint16_t b){script[count++]=b;script[count++]=0;}
static bool Run(void){Ps2PadState p={0};bool result=Ps2MenuRun(&p);assert(p.blocked);return result;}
int main(void){assert(!mkdir("saves",0700));Ps2SettingsDefaults(&g_ps2_settings);
 Reset();Key(Z3P_DOWN);Key(Z3P_DOWN);Key(Z3P_CROSS);Key(Z3P_UP);Key(Z3P_CROSS);assert(Run());assert(g_ps2_settings.frame_mode==0);assert(access("saves/zelda3.ini",F_OK));
 Reset();Key(Z3P_CROSS);assert(Run());Ps2Settings saved;assert(Ps2SettingsLoad(&saved)&&saved.frame_mode==0);
 Reset();Key(Z3P_DOWN);Key(Z3P_DOWN);Key(Z3P_CROSS);Key(Z3P_CIRCLE);assert(!Run());assert(g_ps2_settings.frame_mode==0);
 for(int page=1;page<=3;page++){capture_page=page;Reset();for(int i=0;i<(page-1)*7;i++)Key(Z3P_DOWN);Key(Z3P_CIRCLE);assert(!Run());}
 assert(!rename("saves","missing-card"));capture_page=0;Reset();Key(Z3P_CROSS);Key(Z3P_DOWN);Key(Z3P_CROSS);assert(Run());
 printf("PASS: menu apply/save/cancel, failure fallback, 3 pages within screen (%u frames)\n",frames);return 0;
}
