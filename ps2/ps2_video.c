#ifdef _EE
#include "ps2_video.h"
#include "ps2_indexed.h"
#include "ps2_settings.h"
#include "src/util.h"
#include <gsKit.h>
#include <dmaKit.h>
#include <kernel.h>
#include <SDL_timer.h>
#include <malloc.h>
static GSGLOBAL *gs;
static GSTEXTURE rgba,indexed;
static u32 cluts[Z3_GS_PALETTES][256] __attribute__((aligned(128)));
static uint32_t clut_vram[Z3_GS_PALETTES];
static int aspect,fps;static bool filter,show_fps;static uint32_t fps_time,fps_frames;
// Optional L3 overlay; no timer reads or per-stage work while it is hidden.
static bool stats;
static uint32_t stats_at,stats_window,stats_frames,stats_ms[5],stats_average[5];
void Ps2VideoToggleStats(void){
 stats=!stats;stats_frames=0;stats_window=SDL_GetTicks();
 memset(stats_ms,0,sizeof(stats_ms));memset(stats_average,0,sizeof(stats_average));
}
void Ps2VideoStatsStart(void){if(stats)stats_at=SDL_GetTicks();}
void Ps2VideoStatsStep(unsigned stage){
 if(!stats||stage>=5)return;
 uint32_t now=SDL_GetTicks();stats_ms[stage]+=now-stats_at;stats_at=now;
 if(stage==4){
  stats_frames++;
  if(now-stats_window>=1000){
   for(unsigned i=0;i<5;i++){stats_average[i]=(stats_ms[i]*10+stats_frames/2)/stats_frames;stats_ms[i]=0;}
   stats_frames=0;stats_window=now;
  }
 }
}
static bool Init(SDL_Window *unused){
 (void)unused;gs=gsKit_init_global();if(!gs)return false;
 gs->Mode=GS_MODE_NTSC;gs->Interlace=GS_INTERLACED;gs->Field=GS_FIELD;
 gs->Width=640;gs->Height=448;gs->PSM=GS_PSM_CT32;gs->ZBuffering=GS_SETTING_OFF;
 gs->DoubleBuffering=GS_SETTING_ON;gs->PrimAlphaEnable=GS_SETTING_OFF;
 dmaKit_init(D_CTRL_RELE_OFF,D_CTRL_MFD_OFF,D_CTRL_STS_UNSPEC,D_CTRL_STD_OFF,D_CTRL_RCYC_8,1<<DMA_CHANNEL_GIF);
 dmaKit_chan_init(DMA_CHANNEL_GIF);gsKit_init_screen(gs);gsKit_mode_switch(gs,GS_ONESHOT);
 memset(&rgba,0,sizeof(rgba));rgba.Width=256;rgba.Height=256;rgba.PSM=GS_PSM_CT32;rgba.TBW=4;
 rgba.Mem=memalign(128,256*256*4);if(!rgba.Mem)return false;memset(rgba.Mem,0,256*256*4);
 rgba.Vram=gsKit_vram_alloc(gs,gsKit_texture_size(256,256,rgba.PSM),GSKIT_ALLOC_USERBUFFER);
 indexed=rgba;indexed.Mem=(u32 *)g_ps2_indexed.pixels;indexed.PSM=GS_PSM_T8;indexed.ClutPSM=GS_PSM_CT32;indexed.ClutStorageMode=0;
 indexed.Vram=gsKit_vram_alloc(gs,gsKit_texture_size(256,256,indexed.PSM),GSKIT_ALLOC_USERBUFFER);
 for(int i=0;i<Z3_GS_PALETTES;i++){
  clut_vram[i]=gsKit_vram_alloc(gs,gsKit_texture_size(16,16,GS_PSM_CT32),GSKIT_ALLOC_USERBUFFER);
  if(clut_vram[i]==GSKIT_ALLOC_ERROR)return false;
 }
 fps_time=SDL_GetTicks();return rgba.Vram!=GSKIT_ALLOC_ERROR&&indexed.Vram!=GSKIT_ALLOC_ERROR;
}
static void Destroy(void){dmaKit_wait_fast();free(rgba.Mem);rgba.Mem=NULL;if(gs)gsKit_deinit_global(gs);gs=NULL;}
static void Begin(int width,int height,uint8 **pixels,int *pitch){
 if(width!=256||height!=224)Die("PS2 renderer expects 256x224");
 Ps2IndexedBegin(g_ps2_settings.gs_palette&&!filter);*pixels=(uint8 *)rgba.Mem;*pitch=1024;
}
static void Present(bool count){
 gsKit_queue_exec(gs);gsKit_finish();Ps2VideoStatsStep(3);gsKit_sync_flip(gs);Ps2VideoStatsStep(4);
 if(count){uint32_t now=SDL_GetTicks(),dt=now-fps_time;fps_frames++;
  if(dt>=1000){fps=(fps_frames*1000+dt/2)/dt;fps_frames=0;fps_time=now;}
 }else{fps_time=SDL_GetTicks();fps_frames=0;}
}
static void End(void){
 Ps2VideoStatsStep(2);
 const Ps2IndexedFrame *f=&g_ps2_indexed;dmaKit_wait_fast();
 rgba.Filter=filter?GS_FILTER_LINEAR:GS_FILTER_NEAREST;indexed.Filter=GS_FILTER_NEAREST;
 if(f->indexed_lines<224)gsKit_texture_send_inline(gs,rgba.Mem,256,224,rgba.Vram,rgba.PSM,rgba.TBW,GS_CLUT_NONE);
 if(f->indexed_lines){
  gsKit_texture_send_inline(gs,indexed.Mem,256,224,indexed.Vram,indexed.PSM,indexed.TBW,GS_CLUT_TEXTURE);
  for(unsigned p=0;p<f->palette_count;p++){
   for(unsigned i=0;i<256;i++){unsigned j=(i&~24u)|((i&8)<<1)|((i&16)>>1);cluts[p][j]=f->palettes[p][i];}
   gsKit_texture_send_inline(gs,cluts[p],16,16,clut_vram[p],GS_PSM_CT32,1,GS_CLUT_PALLETE);
  }
 }
 if(aspect)gsKit_clear(gs,GS_SETREG_RGBAQ(0,0,0,0x80,0));
 float x=aspect?64.0f:0.0f,width=aspect?512.0f:640.0f;
 for(int y=0;y<224;){
  int p=f->line_palette[y],end=y+1;while(end<224&&f->line_palette[end]==p)end++;
  GSTEXTURE *tex=p<0?&rgba:&indexed;if(p>=0)indexed.VramClut=clut_vram[p];
  gsKit_prim_sprite_texture(gs,tex,x,y*2.0f,0.0f,(float)y,x+width,end*2.0f,256.0f,(float)end,1,GS_SETREG_RGBAQ(0x80,0x80,0x80,0x80,0));y=end;
 }
 if(show_fps||stats){char t[80];snprintf(t,sizeof(t),"%d FPS",fps);
  Ps2VideoRect(8,8,stats?480:100,stats?70:22,0x101820);Ps2VideoText(14,12,t,2,0xe8e8d0);
  if(stats){
   snprintf(t,sizeof(t),"J %u.%u  A %u.%u  P %u.%u",(unsigned)stats_average[0]/10,(unsigned)stats_average[0]%10,(unsigned)stats_average[1]/10,(unsigned)stats_average[1]%10,(unsigned)stats_average[2]/10,(unsigned)stats_average[2]%10);
   Ps2VideoText(14,34,t,2,0xe8e8d0);
   snprintf(t,sizeof(t),"GS %u.%u  V %u.%u ms",(unsigned)stats_average[3]/10,(unsigned)stats_average[3]%10,(unsigned)stats_average[4]/10,(unsigned)stats_average[4]%10);
   Ps2VideoText(14,56,t,2,0xe8e8d0);
  }
 }
 Present(true);
}
void Ps2VideoCreate(struct RendererFuncs *f){*f=(struct RendererFuncs){Init,Destroy,Begin,End};}
void Ps2VideoOptions(bool filtering,int screen_aspect,bool counter){filter=filtering;aspect=screen_aspect;show_fps=counter;}
int Ps2VideoFps(void){return fps;}
void Ps2VideoMenuBegin(void){gsKit_clear(gs,GS_SETREG_RGBAQ(10,19,28,0x80,0));}
void Ps2VideoMenuEnd(void){Present(false);}
void Ps2VideoRect(int x,int y,int w,int h,uint32_t rgb){gsKit_prim_sprite(gs,x,y,x+w,y+h,2,GS_SETREG_RGBAQ(rgb>>16,(rgb>>8)&255,rgb&255,0x80,0));}
#endif
