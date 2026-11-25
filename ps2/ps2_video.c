#ifdef _EE
#include "ps2_video.h"
#include "ps2_indexed.h"
#include "ps2_gsppu.h"
#include "ps2_settings.h"
#include "ps2_pad.h"
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
static bool gsppu_ok,off_ok;
Ps2Prof g_ps2_prof;static int prof_ms10[7],prof_up;
static int aspect,fps;static bool filter,show_fps;static uint32_t fps_time,fps_frames;
static int vmode=-1,scr_w=640,scr_h=448;static bool hires,dma_ready;
static bool Setup(int mode){
 if(mode<0||mode>2)mode=0;
 hires=(mode==2);vmode=mode;
 if(!dma_ready){dmaKit_init(D_CTRL_RELE_OFF,D_CTRL_MFD_OFF,D_CTRL_STS_UNSPEC,D_CTRL_STD_OFF,D_CTRL_RCYC_8,1<<DMA_CHANNEL_GIF);dmaKit_chan_init(DMA_CHANNEL_GIF);dma_ready=true;}
 gs=hires?gsKit_hires_init_global():gsKit_init_global();if(!gs)return false;
 gs->PSMZ=GS_PSMZ_16S;gs->ZBuffering=GS_SETTING_OFF;gs->DoubleBuffering=GS_SETTING_ON;gs->PrimAlphaEnable=GS_SETTING_OFF;gs->Dithering=GS_SETTING_OFF;
 if(mode==2){
  /* same as OPL "DTV480P 704x480 full screen" (as in Metroid Zero Mission DX): progressive, 24 bit, 2 passes */
  gs->Mode=GS_MODE_DTV_480P;gs->Width=704;gs->Height=480;gs->Interlace=GS_NONINTERLACED;gs->Field=GS_FRAME;gs->PSM=GS_PSM_CT24;gs->Dithering=GS_SETTING_ON;
  gs->OffsetX=((4096-gs->Width)/2)*16;gs->OffsetY=((4096-gs->Height)/2)*16;
  gsKit_hires_init_screen(gs,2);gsKit_set_test(gs,GS_ZTEST_OFF);
 }else{
  gs->Mode=GS_MODE_NTSC;gs->PSM=GS_PSM_CT32;gs->Width=640;
  if(mode==0){gs->Height=448;gs->Interlace=GS_INTERLACED;gs->Field=GS_FIELD;}   /* 480i */
  else{gs->Height=224;gs->Interlace=GS_NONINTERLACED;gs->Field=GS_FRAME;}       /* 240p: one GS line per SNES line */
  gsKit_init_screen(gs);gsKit_mode_switch(gs,GS_ONESHOT);
 }
 scr_w=gs->Width;scr_h=gs->Height;
 memset(&rgba,0,sizeof(rgba));rgba.Width=256;rgba.Height=256;rgba.PSM=GS_PSM_CT32;rgba.TBW=4;
 if(!rgba.Mem)rgba.Mem=memalign(128,256*256*4);
 if(!rgba.Mem)return false;memset(rgba.Mem,0,256*256*4);
 rgba.Vram=gsKit_vram_alloc(gs,gsKit_texture_size(256,256,rgba.PSM),GSKIT_ALLOC_USERBUFFER);
 indexed=rgba;indexed.Mem=(u32 *)g_ps2_indexed.pixels;indexed.PSM=GS_PSM_T8;indexed.ClutPSM=GS_PSM_CT32;indexed.ClutStorageMode=0;
 indexed.Vram=gsKit_vram_alloc(gs,gsKit_texture_size(256,256,indexed.PSM),GSKIT_ALLOC_USERBUFFER);
 for(int i=0;i<Z3_GS_PALETTES;i++){
  clut_vram[i]=gsKit_vram_alloc(gs,gsKit_texture_size(16,16,GS_PSM_CT32),GSKIT_ALLOC_USERBUFFER);
  if(clut_vram[i]==GSKIT_ALLOC_ERROR)return false;
 }
 /* 480p (hires) runs the draw queue once per pass: no scissor, one strip, and 24 bit has no alpha for the color math mask */
 Ps2GsPpuConfigure(hires?1:Z3_GSPPU_MAX_STRIPS,!hires,!hires);
 gsppu_ok=Ps2GsPpuInit(gs);off_ok=!hires&&rgba.Vram!=GSKIT_ALLOC_ERROR&&(rgba.Vram&8191)==0; /* GS rows can be drawn into the rgba texture, which is then filtered to the screen */ /* false: not enough GS memory, tiles stay on the EE */
 fps_time=SDL_GetTicks();return rgba.Vram!=GSKIT_ALLOC_ERROR&&indexed.Vram!=GSKIT_ALLOC_ERROR;
}
static void Teardown(void){dmaKit_wait_fast();if(gs){if(hires)gsKit_hires_deinit_global(gs);else gsKit_deinit_global(gs);}gs=NULL;}
static bool Init(SDL_Window *unused){(void)unused;return Setup(g_ps2_settings.video_mode);}
static void Destroy(void){Teardown();free(rgba.Mem);rgba.Mem=NULL;}
int Ps2VideoMode(void){return vmode;}
/* Switches the video mode at run time. Falls back to 480i (which always works) when the new mode cannot be set up. */
bool Ps2VideoSetMode(int mode){
 if(mode==vmode&&gs)return true;
 Teardown();
 if(Setup(mode))return true;
 Teardown();Setup(0);return false;
}
/* Where the 256x224 picture goes on the screen of the current mode */
static void Layout(float *x,float *y,float *w,float *h){
 if(vmode==2){if(aspect){*x=96;*y=16;*w=512;*h=448;}else{*x=0;*y=0;*w=704;*h=480;}}
 else if(vmode==1){if(aspect){*x=64;*y=0;*w=512;*h=224;}else{*x=0;*y=0;*w=640;*h=224;}}
 else{if(aspect){*x=64;*y=0;*w=512;*h=448;}else{*x=0;*y=0;*w=640;*h=448;}}
}
static void ClearScreen(u64 color){if(hires)gsKit_prim_sprite(gs,0,0,(float)scr_w,(float)scr_h,1,color);else gsKit_clear(gs,color);}
static void Begin(int width,int height,uint8 **pixels,int *pitch){
 if(width!=256||height!=224)Die("PS2 renderer expects 256x224");
 Ps2IndexedBegin(false);Ps2GsPpuBegin(gsppu_ok&&(!filter||off_ok)); /* the GS draws tiles and sprites whenever it can */*pixels=(uint8 *)rgba.Mem;*pitch=1024;
}
static void Present(bool count){
 uint32_t c0=Ps2Cycles(),c1,c2;
 if(hires){gsKit_hires_sync(gs);c1=Ps2Cycles();gsKit_hires_flip(gs);c2=Ps2Cycles();if(count){g_ps2_prof.vsync+=c1-c0;g_ps2_prof.gsrun+=c2-c1;g_ps2_prof.frames++;}}
 else{gsKit_queue_exec(gs);gsKit_finish();c1=Ps2Cycles();gsKit_sync_flip(gs);c2=Ps2Cycles();
 if(count){g_ps2_prof.gsrun+=c1-c0;g_ps2_prof.vsync+=c2-c1;g_ps2_prof.frames++;}}
 if(count){uint32_t now=SDL_GetTicks(),dt=now-fps_time;fps_frames++;
  if(dt>=1000){fps=(fps_frames*1000+dt/2)/dt;fps_frames=0;fps_time=now;
   if(g_ps2_prof.frames){uint64_t v[7]={g_ps2_prof.logic,g_ps2_prof.ppu,g_ps2_prof.gsbuild,g_ps2_prof.gsrun,g_ps2_prof.vsync,g_ps2_prof.sync,g_ps2_prof.strips};for(int i=0;i<7;i++)prof_ms10[i]=(int)(v[i]*10/294912/g_ps2_prof.frames);prof_up=(int)g_ps2_prof.uploads;}
   memset(&g_ps2_prof,0,sizeof(g_ps2_prof));}
 }else{fps_time=SDL_GetTicks();fps_frames=0;}
}
static void End(void){
 uint32_t pc0=Ps2Cycles();
 const Ps2IndexedFrame *f=&g_ps2_indexed;dmaKit_wait_fast();
 rgba.Filter=filter?GS_FILTER_LINEAR:GS_FILTER_NEAREST;indexed.Filter=GS_FILTER_NEAREST;
 if(f->indexed_lines+Ps2GsPpuLineCount()<224)gsKit_texture_send_inline(gs,rgba.Mem,256,224,rgba.Vram,rgba.PSM,rgba.TBW,GS_CLUT_NONE);
 if(f->indexed_lines){
  gsKit_texture_send_inline(gs,indexed.Mem,256,224,indexed.Vram,indexed.PSM,indexed.TBW,GS_CLUT_TEXTURE);
  for(unsigned p=0;p<f->palette_count;p++){
   for(unsigned i=0;i<256;i++){unsigned j=(i&~24u)|((i&8)<<1)|((i&16)>>1);cluts[p][j]=f->palettes[p][i];}
   gsKit_texture_send_inline(gs,cluts[p],16,16,clut_vram[p],GS_PSM_CT32,1,GS_CLUT_PALLETE);
  }
 }
 if(aspect)ClearScreen(GS_SETREG_RGBAQ(0,0,0,0x80,0));
 float x,oy,width,height;Layout(&x,&oy,&width,&height);float sy=height/224.0f;
 if(Ps2GsPpuLineCount()){ /* rows the GS draws from VRAM tiles: straight to the screen, or into the rgba texture when filtering */
  if(filter&&off_ok)Ps2GsPpuDrawOffscreen(gs,rgba.Vram);else Ps2GsPpuDraw(gs,x,oy,width/256.0f,sy);
  if(hires&&aspect){u64 k=GS_SETREG_RGBAQ(0,0,0,0x80,0); /* no scissor in 480p: cover the tile quads that spill past the picture */
   gsKit_prim_sprite(gs,0,0,x,(float)scr_h,1,k);gsKit_prim_sprite(gs,x+width,0,(float)scr_w,(float)scr_h,1,k);
   gsKit_prim_sprite(gs,x,0,x+width,oy,1,k);gsKit_prim_sprite(gs,x,oy+height,x+width,(float)scr_h,1,k);}
 }
 for(int y=0;y<224;){
  if(!filter&&Ps2GsPpuHasLine(y)){y++;continue;}
  int p=f->line_palette[y],end=y+1;while(end<224&&f->line_palette[end]==p&&(filter||!Ps2GsPpuHasLine(end)))end++;
  GSTEXTURE *tex=p<0?&rgba:&indexed;if(p>=0)indexed.VramClut=clut_vram[p];
  gsKit_prim_sprite_texture(gs,tex,x,oy+y*sy,0.0f,(float)y,x+width,oy+end*sy,256.0f,(float)end,1,GS_SETREG_RGBAQ(0x80,0x80,0x80,0x80,0));y=end;
 }
 g_ps2_prof.gsbuild+=Ps2Cycles()-pc0;
 if(show_fps){char t[24];snprintf(t,sizeof(t),"%d FPS",fps);Ps2VideoRect(8,8,100,22,0x101820);Ps2VideoText(14,12,t,2,0xe8e8d0);
  /* ms per frame, last second: L game logic, P PPU on the EE, B building GS work, G GS drawing, V waiting for vblank */
  snprintf(t,sizeof(t),"L%d.%d P%d.%d B%d.%d",prof_ms10[0]/10,prof_ms10[0]%10,prof_ms10[1]/10,prof_ms10[1]%10,prof_ms10[2]/10,prof_ms10[2]%10);Ps2VideoRect(8,34,250,22,0x101820);Ps2VideoText(14,38,t,2,0xe8e8d0);
  snprintf(t,sizeof(t),"G%d.%d V%d.%d",prof_ms10[3]/10,prof_ms10[3]%10,prof_ms10[4]/10,prof_ms10[4]%10);Ps2VideoRect(8,60,170,22,0x101820);Ps2VideoText(14,64,t,2,0xe8e8d0);
  /* inside B: Y = VRAM sync (compare, convert, upload), D = building the tile/sprite passes, U = VRAM uploads in the last second */
  snprintf(t,sizeof(t),"Y%d.%d D%d.%d U%d",prof_ms10[5]/10,prof_ms10[5]%10,prof_ms10[6]/10,prof_ms10[6]%10,prof_up);Ps2VideoRect(8,86,250,22,0x101820);Ps2VideoText(14,90,t,2,0xe8e8d0);}
 Present(true);
}
void Ps2VideoCreate(struct RendererFuncs *f){*f=(struct RendererFuncs){Init,Destroy,Begin,End};}
void Ps2VideoOptions(bool filtering,int screen_aspect,bool counter){filter=filtering;aspect=screen_aspect;show_fps=counter;}
int Ps2VideoFps(void){return fps;}
void Ps2VideoMenuBegin(void){ClearScreen(GS_SETREG_RGBAQ(0x3c,0x2c,0x2c,0x80,0));}
void Ps2VideoMenuEnd(void){Present(false);}
/* The menu and overlays are laid out for 640x448: 240p halves the vertical coordinates (its lines are twice as tall), 480p centers them */
void Ps2VideoRect(int x,int y,int w,int h,uint32_t rgb){
 int ox=vmode==2?32:0,oy=vmode==2?16:0,y0=y,y1=y+h;
 if(vmode==1){y0>>=1;y1>>=1;if(y1==y0)y1=y0+1;}
 gsKit_prim_sprite(gs,x+ox,y0+oy,x+ox+w,y1+oy,2,GS_SETREG_RGBAQ(rgb>>16,(rgb>>8)&255,rgb&255,0x80,0));
}
/* After switching to 480p: wait up to 10 s for X (keep). O or the timeout go back, so a TV without 480p never traps the player. */
bool Ps2VideoConfirm(const char *mode){
 Ps2PadSample prev=Ps2PadPoll();uint32_t start=SDL_GetTicks();char t[40];
 for(;;){
  int left=10-(int)((SDL_GetTicks()-start)/1000);if(left<=0)return false;
  Ps2VideoMenuBegin();Ps2VideoRect(40,60,560,300,0x202030);
  snprintf(t,sizeof(t),"MODO %s ATIVO",mode);Ps2VideoText(64,90,t,3,0xe8e8d0);
  Ps2VideoText(64,150,"SE VOCE ESTA VENDO ESTA TELA:",2,0xe8e8d0);
  Ps2VideoText(64,190,"X: MANTER ESTE MODO",2,0x60e060);
  Ps2VideoText(64,220,"O: VOLTAR AO MODO ANTERIOR",2,0xe8e8d0);
  snprintf(t,sizeof(t),"VOLTA SOZINHO EM %d S",left);Ps2VideoText(64,290,t,2,0xe0c060);
  Ps2VideoMenuEnd();
  Ps2PadSample cur=Ps2PadPoll();uint16_t pressed=cur.buttons&~prev.buttons;prev=cur;
  if(pressed&Z3P_CROSS)return true;if(pressed&Z3P_CIRCLE)return false;
 }
}
#endif
