#include "ps2_pad.h"
#include <string.h>
uint8_t Ps2PadDirections(uint8_t rx,uint8_t ry,int deadzone,uint8_t previous) {
 int x=(int)rx-128,y=(int)ry-128,ax=x<0?-x:x,ay=y<0?-y:y;
 int threshold=deadzone-(previous?4:0);
 if(ax*ax+ay*ay<threshold*threshold) return 0;
 uint8_t d=0;
 if(ax*256>=ay*106) d|=x<0?64:128;
 if(ay*256>=ax*106) d|=y<0?16:32;
 return d;
}
bool Ps2PadUpdate(Ps2PadState *s,const Ps2PadSample *p,const Ps2Settings *cfg) {
 if(!p->connected) { memset(s,0,sizeof(*s));return false; }
 uint16_t b=p->buttons; s->pressed=b&~s->held;s->held=b;
#ifdef Z3_PS2_NO_MENU
 bool chord=false,open=false; // L2/R2 no longer intercept START.
#else
 bool chord=(b&Z3P_MENU)==Z3P_MENU,open=chord&&!s->menu_latch;s->menu_latch=chord;
#endif
 s->axes=p->analog&&cfg->analog?Ps2PadDirections(p->x,p->y,cfg->deadzone,s->axes):0;
 if(s->blocked||chord) { s->snes=0;
 if(!b&&!s->axes)s->blocked=false;
 return open; }
 unsigned d=0;
 if(b&Z3P_UP)d|=16;
 if(b&Z3P_DOWN)d|=32;
 if(b&Z3P_LEFT)d|=64;
 if(b&Z3P_RIGHT)d|=128;
 if(!d)d=s->axes; // preserve the stick while D-pad temporarily overrides it
 if((d&48)==48)d&=~48u;
 if((d&192)==192)d&=~192u;
 unsigned a=cfg->swap_ab?Z3P_CROSS:Z3P_CIRCLE,bb=cfg->swap_ab?Z3P_CIRCLE:Z3P_CROSS;
 unsigned x=cfg->swap_xy?Z3P_SQUARE:Z3P_TRIANGLE,y=cfg->swap_xy?Z3P_TRIANGLE:Z3P_SQUARE;
 s->snes=d|((b&bb)?1:0)|((b&y)?2:0)|((b&Z3P_SELECT)?4:0)|((b&Z3P_START)?8:0)|
         ((b&a)?256:0)|((b&x)?512:0)|((b&Z3P_L1)?1024:0)|((b&Z3P_R1)?2048:0);
 return open;
}
#ifdef _EE
#include <libpad.h>
#include <ps2_joystick_driver.h>
#include <SDL_timer.h>
static unsigned char buffers[2][256] __attribute__((aligned(64)));
static struct { bool opened,configured;unsigned retries;uint32_t retry_at;Ps2PadSample last; } ports[2];
static int active=-1;
bool Ps2PadInit(void) {
 if(init_joystick_driver(true)<0)return false;
 for(int i=0;i<2;i++)ports[i].opened=padPortOpen(i,0,buffers[i])>0;
 return ports[0].opened||ports[1].opened;
}
Ps2PadSample Ps2PadPoll(void) {
 Ps2PadSample p[2]={{0,128,128,false,false},{0,128,128,false,false}};
 for(int i=0;i<2;i++) {
  if(!ports[i].opened)continue;
  int state=padGetState(i,0);
  if(state==PAD_STATE_DISCONN||state==PAD_STATE_ERROR) {
   ports[i].configured=false;ports[i].retries=0;ports[i].last=p[i];continue;
  }
  if(state!=PAD_STATE_STABLE&&state!=PAD_STATE_FINDCTP1) {
   if(state==PAD_STATE_EXECCMD&&(int32_t)(SDL_GetTicks()-ports[i].retry_at)<0)p[i]=ports[i].last;
   continue;
  }
  struct padButtonStatus b;memset(&b,0,sizeof(b));
  if(!padRead(i,0,&b)||b.ok!=0){ports[i].last=p[i];continue;}
  p[i].connected=true;p[i].buttons=(uint16_t)~b.btns;
  p[i].analog=(b.mode>>4)==PAD_TYPE_DUALSHOCK||(b.mode>>4)==PAD_TYPE_ANALOG;
  if(p[i].analog){p[i].x=b.ljoy_h;p[i].y=b.ljoy_v;}
  ports[i].last=p[i];
  if(!ports[i].configured&&(int32_t)(SDL_GetTicks()-ports[i].retry_at)>=0) {
   int modes=padInfoMode(i,0,PAD_MODETABLE,-1);bool dual=false;
   for(int j=0;j<modes;j++)if(padInfoMode(i,0,PAD_MODETABLE,j)==PAD_TYPE_DUALSHOCK)dual=true;
   if(!dual||p[i].analog||ports[i].retries>=3)ports[i].configured=true;
   else {padSetMainMode(i,0,PAD_MMODE_DUALSHOCK,PAD_MMODE_UNLOCK);ports[i].retry_at=SDL_GetTicks()+250;ports[i].retries++;}
  }
 }
 if(active<0||!p[active].connected){active=p[0].connected?0:p[1].connected?1:-1;return(Ps2PadSample){0,128,128,false,false};}
 return p[active];
}
void Ps2PadShutdown(void) {
 for(int i=0;i<2;i++)if(ports[i].opened)padPortClose(i,0);
 deinit_joystick_driver(true);
}
#endif
