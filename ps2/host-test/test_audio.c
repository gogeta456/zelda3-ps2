#include "ps2/ps2_audio.h"
#include "ps2/ps2_frame.h"
#include "ps2/iop/audsrv/src/ring_policy.h"
#include "audsrv.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint32_t ticks;
static int read_head,write_head,ring,fragment,channels,playing,volume,stalled,fail_output;
static unsigned rendered,formats,spu_phase;
static unsigned long long samples_total;
uint32_t SDL_GetTicks(void){return ticks;}
int audsrv_queued(void){return z3_ring_queued(read_head,write_head,ring);}
void SDL_Delay(uint32_t ms){
 ticks+=ms;
 for(uint32_t i=0;i<ms;i++){
  spu_phase+=48000;
  if(spu_phase>=512000){spu_phase-=512000;
   if(playing&&!stalled&&z3_ring_can_read(read_head,write_head,ring,fragment))read_head=(read_head+fragment)%ring;
  }
 }
}
int init_audio_driver(void){return 0;}
void deinit_audio_driver(void){}
int audsrv_stop_audio(void){playing=0;return 0;}
int audsrv_set_volume(int v){volume=v;return 0;}
int audsrv_set_format(struct audsrv_fmt_t *f){
 assert(f->freq==32000&&f->bits==16);channels=f->channels;
 fragment=((512*32000)/48000)*channels*2;ring=fragment*10;
 read_head=write_head=0;playing=0;formats++;return 0;
}
int audsrv_available(void){return z3_ring_available(read_head,write_head,ring);}
int audsrv_play_audio(const char *data,int bytes){
 assert(data&&bytes>0&&bytes<=audsrv_available());if(fail_output)return -1;
 write_head=(write_head+bytes)%ring;playing=1;return bytes;
}
void ZeldaRenderAudio(int16_t *buf,int samples,int ch){
 assert(samples==533||samples==534);assert(ch==channels);
 memset(buf,0,samples*ch*2);rendered++;samples_total+=samples;
}
static void RunMode(bool fps60,int ch){
 Ps2AudioShutdown();assert(Ps2AudioInit(ch,80));Ps2FrameClock clock;Ps2FrameReset(&clock,ticks);
 unsigned first_formats=formats,first_rendered=rendered,draws=0;unsigned long long first_samples=samples_total;
 for(int i=0;i<6000;i++){
  SDL_Delay(2); // input, logic and synthesis cost
  assert(Ps2AudioSubmitFrame());assert(audsrv_queued()<=534*channels*2*3);
  if(Ps2FrameAdvance(&clock,fps60,ticks,Ps2AudioStarved(),true)){
   draws++;SDL_Delay(6);SDL_Delay(Ps2FrameWait(&clock,ticks));
  }
 }
 assert(rendered-first_rendered==6000&&samples_total-first_samples==3200071);
 assert(draws==(fps60?6000u:3000u)&&formats==first_formats);
 // A long frame starves the consumer. Recovery must not reset format or wait.
 SDL_Delay(250);unsigned before=Ps2AudioRecoveries();uint32_t start=ticks;
 assert(Ps2AudioSubmitFrame());assert(ticks==start&&formats==first_formats);
 assert(Ps2AudioRecoveries()==before+1);
 printf("PASS: %d FPS / %d channels, %u draws, starvation recovery 0 ms\n",fps60?60:30,ch,draws);
}
int main(void){
 assert(Ps2AudioInit(2,80));assert(!rendered&&volume==80&&ticks==0);
 RunMode(false,2);RunMode(true,2);RunMode(false,1);
 Ps2AudioPause(true);unsigned before=rendered;int tail=audsrv_queued();SDL_Delay(100000);
 assert(!Ps2AudioSubmitFrame()&&rendered==before&&!playing&&audsrv_queued()==tail);
 Ps2AudioPause(false);assert(playing);assert(Ps2AudioSubmitFrame());assert(Ps2AudioReady());
 Ps2AudioVolume(500);assert(volume==100);Ps2AudioVolume(-1);assert(volume==0);
 read_head=0;write_head=534*channels*2*2+100;stalled=1;uint32_t start=ticks;
 assert(!Ps2AudioSubmitFrame());assert(ticks-start<=41&&!Ps2AudioReady());
 before=rendered;assert(!Ps2AudioSubmitFrame()&&rendered==before+1);stalled=0;
 assert(Ps2AudioInit(1,50));fail_output=1;assert(!Ps2AudioSubmitFrame());assert(!Ps2AudioReady());
 Ps2AudioShutdown();puts("PASS: audio pause/resume, bounded output failure, no runtime re-prime");return 0;
}
