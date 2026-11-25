#include "ps2/ps2_frame.h"
#include "ps2/iop/audsrv/src/ring_policy.h"
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
static void Clock(bool fps60,uint32_t start){
 Ps2FrameClock c;Ps2FrameReset(&c,start);uint32_t now=start;unsigned draws=0;
 for(unsigned n=0;n<60000;n++){
  now+=2;
  if(Ps2FrameAdvance(&c,fps60,now,true,true)){draws++;now+=6;now+=Ps2FrameWait(&c,now);}
 }
 assert(draws==(fps60?60000u:30000u));assert(now-start==1001000);
 Ps2FrameReset(&c,0);unsigned skips=0;
 for(int i=0;i<20;i++){if(!Ps2FrameAdvance(&c,fps60,200,true,true))skips++;}
 assert(skips>0&&skips<20);assert(Ps2FrameWait(&c,c.deadline+251)==0);assert(!c.steps);
}

/* Regression for the "FPS collapses to 10 and recovers slowly" report. The loop below models the
 * real pacing: gameplay is throttled by an audio queue of two frames and by the GS vsync, so no step
 * can finish faster than real time. Four short load spikes (door hits) follow one second of light load. */
static bool LegacyAdvance(Ps2FrameClock *c,bool fps60,uint32_t now){
 c->deadline+=Ps2FrameMilliseconds(&c->phase);++c->steps;
 if(!fps60&&(c->steps&1u))return false;
 if((int32_t)(now-c->deadline)>34&&c->skipped<2){++c->skipped;return false;}
 c->skipped=0;return true;
}
static double CeilTo(double t,double p){long n=(long)(t/p);if(n*p<t)n++;return n*p;}
static void Model(bool fixed,bool fps60,double light_logic,double light_draw,unsigned fps[16]){
 const double P=1001.0/60.0;static const double spikes[4][2]={{2000,2250},{2600,2850},{3200,3450},{3800,4050}};
 Ps2FrameClock c;Ps2FrameReset(&c,0);double t=0,qt=0,last=0;unsigned seed=7;memset(fps,0,16*sizeof(unsigned));
 while(t<16000){
  bool heavy=false;for(int i=0;i<4;i++)if(t>=spikes[i][0]&&t<spikes[i][1])heavy=true;
  seed=seed*1664525u+1013904223u;double j=1.0+((int)((seed>>16)%301)-150)/1000.0; /* +-15% jitter */
  double a=(heavy?12.0:light_logic)*j,d=(heavy?34.0:light_draw)*j;
  t+=a;qt-=t-last;if(qt<0)qt=0;last=t;bool starved=qt<P;
  while(qt>2*P){t+=1;qt-=1;if(qt<0)qt=0;last=t;}
  qt+=P;
  bool draw=fixed?Ps2FrameAdvance(&c,fps60,(uint32_t)t,starved,true):LegacyAdvance(&c,fps60,(uint32_t)t);
  if(!draw)continue;
  t+=d;t=CeilTo(t,P);qt-=t-last;if(qt<0)qt=0;last=t;
  int s=(int)(t/1000);if(s<16)fps[s]++;
  unsigned w=Ps2FrameWait(&c,(uint32_t)t);if(w){t+=w;qt-=w;if(qt<0)qt=0;last=t;}
 }
}
static void LockIn(void){
 unsigned fps[16];
 for(int mode=0;mode<2;mode++){
  bool fps60=mode;double al=fps60?5:7,dl=fps60?9:12;unsigned target=fps60?60:30;
  Model(false,fps60,al,dl,fps);for(int s=6;s<16;s++)assert(fps[s]<=target/2+2); /* old logic stays locked at 1/3 (60) or 1/3 (30) */
  Model(true,fps60,al,dl,fps);for(int s=6;s<16;s++)assert(fps[s]+3>=target);   /* fixed logic is back at full rate */
 }
 Ps2FrameClock c;Ps2FrameReset(&c,0); /* late but audio healthy: forgive, draw, no skip */
 assert(Ps2FrameAdvance(&c,true,200,false,true)&&c.deadline==200&&!c.late_skips);
 Ps2FrameReset(&c,0); /* late and audio starving: skip twice, then draw and forgive the rest */
 assert(!Ps2FrameAdvance(&c,true,200,true,true)&&!Ps2FrameAdvance(&c,true,200,true,true)&&c.late_skips==2);
 assert(Ps2FrameAdvance(&c,true,200,true,true)&&c.deadline==200);
 Ps2FrameReset(&c,0); /* frameskip off: late and starving still draws every pass and never counts a skip */
 for(int i=0;i<10;i++)assert(Ps2FrameAdvance(&c,true,200+i*16,true,false));
 assert(!c.late_skips);
}
static void Ring(int channels){
 unsigned char data[16384];int fragment=341*channels*2,size=fragment*10,rd=0,wr=0;
 unsigned produced=0,consumed=0,random=19;
 assert(z3_ring_queued(rd,wr,size)==0&&z3_ring_available(rd,wr,size)==size-4);
 for(int i=0;i<200000;i++){
  random=random*1664525u+1013904223u;
  if(random&2){
   int bytes=(random>>8)%2140;int room=z3_ring_available(rd,wr,size);if(bytes>room)bytes=room;
   for(int k=0;k<bytes;k++){data[wr]=(unsigned char)produced++;wr=(wr+1)%size;}
  }
  if(random&4){
   if(z3_ring_can_read(rd,wr,size,fragment)){
    for(int k=0;k<fragment;k++){assert(data[rd]==(unsigned char)consumed++);rd=(rd+1)%size;}
   }else{assert(produced-consumed<(unsigned)fragment);}
  }
  assert(z3_ring_queued(rd,wr,size)==(int)(produced-consumed));
  assert(z3_ring_available(rd,wr,size)>=0&&produced-consumed<=(unsigned)(size-4));
 }
}
int main(void){Clock(false,0);Clock(true,0);Clock(false,UINT32_MAX-100);Clock(true,UINT32_MAX-100);LockIn();Ring(1);Ring(2);puts("PASS: 30/60 presentation at identical game speed, tick wrap, late frames, no lock-in after load spikes; IOP ring data integrity under starvation/wrap");return 0;}
