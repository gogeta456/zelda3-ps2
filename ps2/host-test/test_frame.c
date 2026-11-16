#include "ps2/ps2_frame.h"
#include "ps2/iop/audsrv/src/ring_policy.h"
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
static void Clock(bool fps60,uint32_t start){
 Ps2FrameClock c;Ps2FrameReset(&c,start);uint32_t now=start;unsigned draws=0;
 for(unsigned n=0;n<60000;n++){
  now+=2;
  if(Ps2FrameAdvance(&c,fps60)){draws++;now+=6;now+=Ps2FrameWait(&c,now);}
 }
 assert(draws==(fps60?60000u:30000u));assert(now-start==1001000);
 // Both short and long stalls must preserve every scheduled presentation.
 Ps2FrameReset(&c,start);draws=0;
 for(unsigned i=0;i<1000;i++){
  bool draw=Ps2FrameAdvance(&c,fps60);
  assert(draw==(fps60 || (i&1u)));
  draws+=draw;
  uint32_t late=c.deadline+(i%2?35:251);
  assert(Ps2FrameWait(&c,late)==0);
  assert(c.steps==i+1);
  if(!(i&1u))assert(c.deadline==late);
 }
 assert(draws==(fps60?1000u:500u));
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
int main(void){Clock(false,0);Clock(true,0);Clock(false,UINT32_MAX-100);Clock(true,UINT32_MAX-100);Ring(1);Ring(2);puts("PASS: 30/60 presentation at identical game speed, tick wrap, NO automatic frameskip under delay; IOP ring data integrity under starvation/wrap");return 0;}
