#include "ps2/ps2_audio_transport.h"
#include "audsrv.h"
#include "sifrpc.h"
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
static int busy,fail;
static void *dma_source,*dma_result;
static int dma_size;
int sceSifBindRpc(SifRpcClientData_t *c,int id,int mode){
 assert(id==AUDSRV_IRX&&!mode);c->server=c;return 0;
}
int sceSifCheckStatRpc(SifRpcClientData_t *c){assert(c->server);return busy;}
int sceSifCallRpc(SifRpcClientData_t *c,int cmd,int mode,void *src,int size,void *dst,int rsize,void *cb,void *arg){
 assert(c->server&&cmd==Z3_AUDIO_RPC_PUSH&&mode==SIF_RPC_M_NOWAIT);
 assert(!cb&&!arg&&rsize==8&&size>=4&&size<=Z3_AUDIO_PACKET_BYTES+4);
 assert(!((uintptr_t)src&63)&&!((uintptr_t)dst&63));
 if(fail)return -1;
 dma_source=src;dma_result=dst;dma_size=size;busy=1;return 0;
}
int main(void){
 unsigned char source[Z3_AUDIO_PACKET_BYTES],expected[Z3_AUDIO_PACKET_BYTES];
 for(unsigned i=0;i<sizeof source;i++)source[i]=expected[i]=(i*97+11)&255;
 assert(Ps2AudioTransportInit());
 assert(!Ps2AudioTransportStart(source,0));
 assert(!Ps2AudioTransportStart(source,sizeof source+1));
 assert(Ps2AudioTransportStart(source,sizeof source));
 int accepted=-9,queued=-9;
 assert(!Ps2AudioTransportPoll(&accepted,&queued)&&accepted==-9);
 assert(!Ps2AudioTransportStart(source,42));
 memset(source,0,sizeof source); // caller buffer may be reused: packet must survive.
 assert(*(int *)dma_source==sizeof source&&dma_size==sizeof source+4);
 assert(!memcmp((char *)dma_source+4,expected,sizeof expected));
 ((int *)dma_result)[0]=sizeof source;((int *)dma_result)[1]=1234;busy=0;
 assert(Ps2AudioTransportPoll(&accepted,&queued)&&accepted==sizeof source&&queued==1234);
 fail=1;assert(!Ps2AudioTransportStart(source,42));
 puts("PASS: native SIF transport NOWAIT, DMA alignment, packet lifetime and reply");
}
