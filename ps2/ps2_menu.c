#ifdef _EE
#include "ps2_menu.h"
#include "ps2_video.h"
#include "src/ps2_compat.h"
#include <SDL_timer.h>
#include <stdio.h>
#include <stddef.h>
typedef struct MenuItem{const char *label;size_t offset;int step,min,max;}MenuItem;
#define ITEM(label,field,step,min,max){label,offsetof(Ps2Settings,field),step,min,max}
static const MenuItem items[]={
 {"SALVAR E VOLTAR",0,0,0,0},{"APLICAR SEM SALVAR",0,0,0,0},
 ITEM("QUADROS",frame_mode,1,0,1),ITEM("MOSTRAR FPS",show_fps,1,0,1),
 ITEM("PALETA PELO GS",gs_palette,1,0,1),ITEM("FILTRO SUAVE",filtering,1,0,1),
 ITEM("FORMATO DA TELA",aspect,1,0,1),ITEM("SATURACAO",saturation,10,0,200),
 ITEM("VOLUME",volume,5,0,100),ITEM("SOM ESTEREO",stereo,1,0,1),
 ITEM("ANALOGICO",analog,1,0,1),ITEM("ZONA MORTA",deadzone,4,12,64),
 ITEM("TROCAR X / CIRCULO",swap_ab,1,0,1),ITEM("TROCAR QUAD / TRI",swap_xy,1,0,1),
 ITEM("ITENS EM L1 / R1",switch_lr,1,0,1),ITEM("ALERTA DE VIDA",low_health_beep,1,0,1),
 ITEM("REDUZIR CLAROES",dim_flashes,1,0,1),{"RESTAURAR PADRAO",0,0,0,0},{"CANCELAR",0,0,0,0}
};
#undef ITEM
enum{ITEM_COUNT=sizeof(items)/sizeof(items[0]),PAGE_SIZE=7};
static const char *Value(int index,int v,char *buf,size_t size){
 if(index==2)return v?"60 FPS":"30 FPS";
 if(index==6)return v?"PIXEL 1:1":"TELA 4:3";
 if(items[index].max==1)return v?"SIM":"NAO";
 snprintf(buf,size,(index==7||index==8)?"%d%%":"%d",v);return buf;
}
bool Ps2MenuRun(Ps2PadState *pad){
 Ps2Settings draft=g_ps2_settings;unsigned selected=0;uint32_t next_repeat=0;uint16_t last_direction=0;
 const char *status="L2 + R2 + START ABRE ESTE MENU";bool release_gate=true;
 for(;;){
  Ps2PadSample raw=Ps2PadPoll();Ps2PadUpdate(pad,&raw,&draft);uint16_t held=pad->held;
  if(pad->axes&16)held|=Z3P_UP;
  if(pad->axes&32)held|=Z3P_DOWN;
  if(pad->axes&64)held|=Z3P_LEFT;
  if(pad->axes&128)held|=Z3P_RIGHT;
  uint16_t pressed=pad->pressed;
  if(release_gate){if(!held&&raw.connected)release_gate=false;pressed=0;held=0;}
  uint16_t direction=held&(Z3P_UP|Z3P_DOWN|Z3P_LEFT|Z3P_RIGHT);uint32_t now=SDL_GetTicks();
  bool repeat=direction&&(direction!=last_direction||(int32_t)(now-next_repeat)>=0);
  if(repeat)next_repeat=now+(direction==last_direction?100:350);
  last_direction=direction;
  if(repeat&&(direction&Z3P_UP))selected=(selected+ITEM_COUNT-1)%ITEM_COUNT;
  else if(repeat&&(direction&Z3P_DOWN))selected=(selected+1)%ITEM_COUNT;
  const MenuItem *item=&items[selected];bool activate=pressed&Z3P_CROSS;
  if((pressed&Z3P_CIRCLE)||(activate&&selected==ITEM_COUNT-1)){pad->blocked=true;return false;}
  if(activate&&selected<2){
   if(selected==0&&!Ps2SettingsSave(&draft))status="FALHA AO SALVAR - CONFIRA O CARTAO";
   else{g_ps2_settings=draft;pad->blocked=true;return true;}
  }else if(activate&&selected==ITEM_COUNT-2){Ps2SettingsDefaults(&draft);status="PADRAO RESTAURADO - SALVE PARA MANTER";}
  else if(item->step&&(activate||(repeat&&(direction&(Z3P_LEFT|Z3P_RIGHT))))){
   int *v=(int *)((char *)&draft+item->offset);*v+=!activate&&(direction&Z3P_LEFT)?-item->step:item->step;
   if(item->max==1)*v=!!(*v&1);else{if(*v<item->min)*v=item->min;if(*v>item->max)*v=item->max;}
   status="ALTERACOES AGUARDANDO APLICACAO";
  }
  Ps2VideoMenuBegin();Ps2VideoRect(28,24,584,4,0xd9b750);
  Ps2VideoText(36,42,"ZELDA3 - CONFIGURACOES PS2",3,0xe7d28b);
  char line[64];snprintf(line,sizeof(line),"PAGINA %u / 3",selected/PAGE_SIZE+1);Ps2VideoText(38,76,line,2,0x86acbb);
  unsigned first=selected/PAGE_SIZE*PAGE_SIZE;
  for(unsigned i=first;i<first+PAGE_SIZE&&i<ITEM_COUNT;i++){
   int y=106+(i-first)*34;if(i==selected)Ps2VideoRect(30,y-6,580,28,0x254557);
   Ps2VideoText(38,y,i==selected?">":" ",2,0xf0d16d);Ps2VideoText(58,y,items[i].label,2,0xe0e8e6);
   if(items[i].step){int v=*(int *)((char *)&draft+items[i].offset);Ps2VideoText(438,y,Value(i,v,line,sizeof(line)),2,0xf0d16d);}
  }
  const char *help="X: ALTERAR / CONFIRMAR   CIRCULO: CANCELAR";
  if(selected==2)help="30 FPS PADRAO   60 FPS OPCIONAL";
  if(selected==4)help="EXPERIMENTAL - PODE REDUZIR DESEMPENHO";
  if(selected==5)help="FILTRO SUAVE USA COR RGBA E FILTRAGEM GS";
  if(selected==12)help="PADRAO: CIRCULO ACAO / X ESPADA";
  if(selected==13)help="PADRAO: QUADRADO ITEM / TRIANGULO MAPA";
  if(!raw.connected)help="CONECTE UM CONTROLE NA PORTA 1 OU 2";
  Ps2VideoText(36,354,help,2,0x95b7c5);Ps2VideoText(36,378,status,2,0xe7d28b);
  snprintf(line,sizeof(line),"SAVE: %s",ps2_save_location());for(char *p=line;*p;p++)if(*p>='a'&&*p<='z')*p-=32;
  Ps2VideoText(36,402,line,2,0x86acbb);Ps2VideoMenuEnd();
 }
}
#endif
