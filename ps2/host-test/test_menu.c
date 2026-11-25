#include "ps2/ps2_menu.h"
#include "ps2/ps2_video.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
static uint16_t script[128];static unsigned count,pos,frames;static uint32_t ticks;static uint8_t pixels[448][640][3];
static const char *capture_name;
uint32_t SDL_GetTicks(void){return ticks;}
void SDL_Delay(uint32_t ms){ticks+=ms;}
const char *ps2_save_location(void){return "mc0:/ZELDA3-LTTP/";}
Ps2PadSample Ps2PadPoll(void){assert(pos<count);return(Ps2PadSample){script[pos++],128,128,true,true};}
void Ps2VideoMenuBegin(void){for(int y=0;y<448;y++)for(int x=0;x<640;x++){pixels[y][x][0]=10;pixels[y][x][1]=19;pixels[y][x][2]=28;}}
void Ps2VideoRect(int x,int y,int w,int h,uint32_t rgb){assert(x>=0&&y>=0&&x+w<=640&&y+h<=448);for(int yy=y;yy<y+h;yy++)for(int xx=x;xx<x+w;xx++){pixels[yy][xx][0]=rgb>>16;pixels[yy][xx][1]=rgb>>8;pixels[yy][xx][2]=rgb;}}
void Ps2VideoMenuEnd(void){ticks+=17;frames++;if(capture_name&&pos==count-2){char path[80];snprintf(path,sizeof path,"menu-%s.ppm",capture_name);FILE *f=fopen(path,"wb");assert(f);fprintf(f,"P6\n640 448\n255\n");assert(fwrite(pixels,1,sizeof pixels,f)==sizeof pixels);fclose(f);}}
static void Reset(void){count=pos=0;script[count++]=0;}
static void Key(uint16_t b){script[count++]=b;script[count++]=0;}
static bool Run(void){Ps2PadState p={0};bool result=Ps2MenuRun(&p);assert(p.blocked);return result;}
static void Down(int n){while(n--)Key(Z3P_DOWN);}
static void Capture(const char *name,int downs,int enter_page_at){ /* navigate, screenshot, then START resumes */
 capture_name=name;Reset();if(enter_page_at>=0){Down(enter_page_at);Key(Z3P_CROSS);}Down(downs);Key(Z3P_START);assert(Run());capture_name=NULL;}
int main(void){assert(!mkdir("saves",0700));Ps2SettingsDefaults(&g_ps2_settings);
 /* CONTINUAR JOGO applies and saves. */
 Reset();Key(Z3P_CROSS);assert(Run());assert(!Ps2MenuExitRequested());
 /* VIDEO > MOSTRAR FPS toggles the counter, O goes back, START resumes with the change saved. 60 FPS, frameskip off and the PPU on the GS are fixed: no menu entry. */
 Reset();Down(1);Key(Z3P_CROSS);Down(1);Key(Z3P_CROSS);Key(Z3P_CIRCLE);Key(Z3P_START);assert(Run());assert(g_ps2_settings.show_fps==1);
 Ps2Settings saved;assert(Ps2SettingsLoad(&saved)&&saved.show_fps==1);
 assert(saved.frame_mode==1&&!saved.frameskip&&saved.gs_palette);
 /* X and right on every VIDEO entry cannot change the fixed options. */
 Reset();Down(1);Key(Z3P_CROSS);for(int i=0;i<6;i++){Key(Z3P_CROSS);Key(Z3P_RIGHT);Key(Z3P_DOWN);}Key(Z3P_START);assert(Run());
 assert(g_ps2_settings.frame_mode==1&&!g_ps2_settings.frameskip&&g_ps2_settings.gs_palette);
 int vol0=g_ps2_settings.volume;
 /* AUDIO > VOLUME moves in steps of 5 with left/right and clamps. */
 Reset();Down(2);Key(Z3P_CROSS);Key(Z3P_LEFT);Key(Z3P_LEFT);Key(Z3P_START);assert(Run());assert(g_ps2_settings.volume==vol0-10);
 /* SISTEMA > DESCARTAR drops pending edits. */
 int before=g_ps2_settings.show_fps;
 Reset();Down(1);Key(Z3P_CROSS);Down(1);Key(Z3P_CROSS);Key(Z3P_CIRCLE);Down(3);Key(Z3P_CROSS);Down(2);Key(Z3P_CROSS);assert(!Run());assert(g_ps2_settings.show_fps==before);
 /* SISTEMA > RESTAURAR PADRAO then START. */
 Reset();Down(4);Key(Z3P_CROSS);Down(1);Key(Z3P_CROSS);Key(Z3P_START);assert(Run());assert(g_ps2_settings.show_fps==0&&g_ps2_settings.volume==100&&g_ps2_settings.frame_mode==1&&!g_ps2_settings.frameskip&&g_ps2_settings.gs_palette);
 /* SAIR DO JOGO asks for confirmation: VOLTAR keeps playing, SAIR requests exit. */
 Reset();Down(5);Key(Z3P_CROSS);Key(Z3P_CROSS);Key(Z3P_START);assert(Run());assert(!Ps2MenuExitRequested());
 Reset();Down(5);Key(Z3P_CROSS);Down(1);Key(Z3P_CROSS);assert(!Run());assert(Ps2MenuExitRequested());
 Reset();Key(Z3P_CIRCLE);assert(Run());assert(!Ps2MenuExitRequested());
 /* Screenshots of every page (the harness asserts every rectangle stays inside 640x448). */
 Capture("main",0,-1);Capture("video",0,1);Capture("video-last",5,1);Capture("audio",0,2);Capture("controls",0,3);Capture("system",0,4);Capture("exit",1,5);
 assert(!rename("saves","missing-card"));Reset();Key(Z3P_CROSS);assert(Run()); /* no card: resume still works */
 printf("PASS: menu resume/sub-menus/discard/defaults/exit confirm, no-card resume, all pages inside the screen (%u frames)\n",frames);return 0;
}
