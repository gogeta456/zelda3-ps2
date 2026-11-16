#include "ps2/ps2_audio.h"
#include "ps2/ps2_audio_transport.h"
#include "ps2/iop/audsrv/src/ring_policy.h"
#include "ps2/iop/audsrv/src/resample32.h"
#include "audsrv.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint32_t ticks;
static int rd,wr,ring,channels,playing,volume,stalled,fail_output,rpc_stalled;
static unsigned rendered,formats,spu_phase,pcm_phase;
static unsigned long long samples_total;
static int rpc_pending,rpc_bytes;
static unsigned char rpc_copy[Z3_AUDIO_PACKET_BYTES];
static const unsigned char *rpc_source;
uint32_t SDL_GetTicks(void){return ticks;}
int audsrv_queued(void){return z3_ring_queued(rd,wr,ring);}
void SDL_Delay(uint32_t ms){
 ticks+=ms;
 for(uint32_t i=0;i<ms;i++){
  spu_phase+=48000;
  if(spu_phase>=512000){spu_phase-=512000;
   if(playing&&!stalled&&z3_ring_can_read(rd,wr,ring,342*channels*2)){
    unsigned frames=(pcm_phase+1024)/3;pcm_phase=(pcm_phase+1024)%3;
    rd=(rd+frames*channels*2)%ring;
   }
  }
 }
}
int init_audio_driver(void){return 0;}
void deinit_audio_driver(void){}
int audsrv_stop_audio(void){playing=0;return 0;}
int audsrv_set_volume(int v){volume=v;return 0;}
int audsrv_set_format(struct audsrv_fmt_t *f){
 assert(f->freq==32000&&f->bits==16);channels=f->channels;
 ring=341*channels*2*10;rd=wr=0;playing=0;pcm_phase=0;formats++;return 0;
}
int audsrv_available(void){return z3_ring_available(rd,wr,ring);}
int audsrv_play_audio(const char *data,int bytes){
 assert(data&&bytes>0&&bytes<=audsrv_available());if(fail_output)return -1;
 wr=(wr+bytes)%ring;playing=1;return bytes;
}
bool Ps2AudioTransportInit(void){assert(!rpc_pending);return true;}
bool Ps2AudioTransportStart(const void *pcm,int bytes){
 assert(!rpc_pending&&bytes>0&&bytes<=Z3_AUDIO_PACKET_BYTES);
 rpc_bytes=bytes;rpc_source=pcm;memcpy(rpc_copy,pcm,bytes);rpc_pending=1;return true;
}
int Ps2AudioTransportPoll(int *accepted,int *queued){
 assert(rpc_pending);
 // The source must not be reused/overwritten during a delayed RPC.
 assert(!memcmp(rpc_copy,rpc_source,rpc_bytes));
 if(rpc_stalled)return 0;
 *queued=audsrv_queued();
 *accepted=*queued>rpc_bytes*2?0:audsrv_play_audio((const char *)rpc_copy,rpc_bytes);
 rpc_pending=0;return 1;
}
void ZeldaRenderAudio(int16_t *buf,int samples,int ch){
 assert(samples==533||samples==534);assert(ch==channels);
 for(int i=0;i<samples*ch;i++)buf[i]=(int16_t)(rendered+i);
 rendered++;samples_total+=samples;
}
static void RunMode(int ch){
 Ps2AudioShutdown();assert(Ps2AudioInit(ch,80));
 unsigned first_formats=formats,first_rendered=rendered,phase=0;
 unsigned long long first_samples=samples_total;
 for(int i=0;i<60000;i++){
  uint32_t start=ticks;assert(Ps2AudioSubmitFrame());assert(ticks==start);
  assert(audsrv_queued()<=534*channels*2*3);
  SDL_Delay(Ps2FrameMilliseconds(&phase)); // one NTSC VBlank per game frame
 }
 assert(rendered-first_rendered==60000&&samples_total-first_samples==32032000);
 assert(formats==first_formats&&Ps2AudioDropped()==0);
 SDL_Delay(250);unsigned before=Ps2AudioRecoveries();uint32_t start=ticks;
 assert(Ps2AudioSubmitFrame());assert(ticks==start&&formats==first_formats);
 assert(Ps2AudioRecoveries()==before+1);
 printf("PASS: 60000 frames / %d channels; no waits, drops or runtime format resets\n",ch);
}
static void Resampler(void){
 static int16_t data[342*2],left[512],right[512];
 for(int ch=1;ch<=2;ch++){
  unsigned phase=0,consumed=0;
  for(unsigned block=0;block<3000;block++){
   for(unsigned i=0;i<342;i++){
    data[i*ch]=(int16_t)(consumed+i);
    if(ch==2)data[i*ch+1]=(int16_t)~(consumed+i);
   }
   unsigned used=z3_resample32(data,ch,left,right,&phase);
   for(unsigned i=0;i<512;i++){
    unsigned sample=((block*512+i)*2)/3;
    assert(left[i]==(int16_t)sample);
    assert(right[i]==(int16_t)(ch==1?sample:~sample));
   }
   consumed+=used;
  }
  assert(consumed==1024000&&phase==0);
 }
 puts("PASS: exact 32 kHz consumption, stereo order and fractional phase");
}
int main(void){
 Resampler();assert(Ps2AudioInit(2,80));assert(!rendered&&volume==80&&ticks==0);
 RunMode(2);RunMode(1);
 Ps2AudioPause(true);unsigned before=rendered;SDL_Delay(1000);
 assert(!Ps2AudioSubmitFrame()&&rendered==before&&!playing);
 Ps2AudioPause(false);assert(playing);
 Ps2AudioVolume(500);assert(volume==100);Ps2AudioVolume(-1);assert(volume==0);
 // A frozen IOP never freezes video: bounded queue, immutable DMA storage.
 rpc_stalled=1;assert(Ps2AudioSubmitFrame());uint32_t start=ticks;
 for(int i=0;i<100;i++)Ps2AudioSubmitFrame();
 assert(ticks==start&&Ps2AudioDropped()>0&&Ps2AudioReady());
 rpc_stalled=0;SDL_Delay(50);for(int i=0;i<20;i++){Ps2AudioSubmitFrame();SDL_Delay(17);}
 assert(Ps2AudioReady());Ps2AudioShutdown();assert(Ps2AudioInit(1,50));
 fail_output=1;assert(Ps2AudioSubmitFrame());assert(!Ps2AudioSubmitFrame());assert(!Ps2AudioReady());
 Ps2AudioShutdown();puts("PASS: delayed RPC ownership, overflow recovery, pause/resume, output failure");return 0;
}
