#include "ps2/iop/audsrv/src/upsamplers.h"
#include "ps2/ps2_timing.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
int main(void){
 for(int channels=1;channels<=2;channels++){
  short src[341*2],left[512],right[512];
  for(int i=0;i<341;i++){src[i*channels]=i;if(channels==2)src[i*2+1]=-i;}
  upsample_t u={left,right,(const unsigned char *)src};
  upsampler_t fn=find_upsampler(32000,16,channels);assert(fn);
  assert(fn(&u)==341*channels*2);
  for(int i=0;i<512;i++){assert(left[i]==i*2/3);assert(right[i]==(channels==2?-(i*2/3):i*2/3));}
 }
 unsigned phase=0;uint64_t count=0;
 for(int i=0;i<2560;i++)count+=Ps2AudioFrameSamples(&phase);
 assert(count==1365364&&phase==0);
 puts("PASS: real IOP resampler consumes 341 samples/block; PCM clock matches its effective rate");return 0;
}
