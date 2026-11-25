#include "ps2/ps2_pad.h"
#include "ps2/ps2_audio.h"
#include "ps2/ps2_indexed.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
static void Controls(void) {
  Ps2Settings c; Ps2SettingsDefaults(&c);
  Ps2PadState s={0}; Ps2PadSample p={0,128,128,true,true};
  const uint16_t physical[]={Z3P_CROSS,Z3P_SQUARE,Z3P_SELECT,Z3P_START,Z3P_UP,Z3P_DOWN,Z3P_LEFT,Z3P_RIGHT,Z3P_CIRCLE,Z3P_TRIANGLE,Z3P_L1,Z3P_R1};
  for(unsigned i=0;i<12;i++){p.buttons=physical[i];Ps2PadUpdate(&s,&p,&c);
    if(i==2||i==3){assert(s.snes==0);for(int k=0;k<Z3P_CHORD_WAIT;k++)Ps2PadUpdate(&s,&p,&c);} /* Start/Select are held back briefly */
    assert(s.snes==(1u<<i));p.buttons=0;for(int k=0;k<Z3P_TAP_FRAMES+1;k++)Ps2PadUpdate(&s,&p,&c);}
  c.swap_ab=c.swap_xy=1;
  for(unsigned i=0;i<4;i++){unsigned a[]={0,1,8,9},b[]={8,9,0,1};p.buttons=physical[a[i]];Ps2PadUpdate(&s,&p,&c);assert(s.snes==(1u<<b[i]));}
  Ps2SettingsDefaults(&c);p.buttons=0;p.x=255;Ps2PadUpdate(&s,&p,&c);assert(s.snes==128);
  p.buttons=Z3P_UP;Ps2PadUpdate(&s,&p,&c);assert(s.snes==16);
  p.buttons=0;Ps2PadUpdate(&s,&p,&c);assert(s.snes==128);
  p.analog=false;p.x=p.y=0;Ps2PadUpdate(&s,&p,&c);assert(s.snes==0);
  p.analog=true;Ps2PadUpdate(&s,&p,&c);assert(s.snes==80);
  p.x=p.y=128;p.buttons=Z3P_UP|Z3P_DOWN|Z3P_LEFT|Z3P_RIGHT;Ps2PadUpdate(&s,&p,&c);assert(s.snes==0);
  p.buttons=Z3P_CROSS;p.connected=false;Ps2PadUpdate(&s,&p,&c);assert(!s.held&&!s.snes);
  p.connected=true;p.buttons=Z3P_MENU;assert(Ps2PadUpdate(&s,&p,&c));assert(!s.snes);assert(!Ps2PadUpdate(&s,&p,&c));
  /* Start/Select are held back so the Start+Select chord never leaks a lone Start; a quick tap is replayed on release */
  p.buttons=0;Ps2PadUpdate(&s,&p,&c);s.blocked=false;memset(&s,0,sizeof s);
  p.buttons=Z3P_START;assert(!Ps2PadUpdate(&s,&p,&c));assert(!(s.snes&8));         /* first frame: held back */
  p.buttons=Z3P_START|Z3P_SELECT;assert(Ps2PadUpdate(&s,&p,&c));assert(!s.snes);     /* chord one frame later: menu, nothing leaked */
  p.buttons=0;Ps2PadUpdate(&s,&p,&c);assert(!s.snes);
  memset(&s,0,sizeof s);p.buttons=Z3P_START;Ps2PadUpdate(&s,&p,&c);p.buttons=0;Ps2PadUpdate(&s,&p,&c);assert(s.snes&8);  /* quick tap becomes a short press */
  for(int i=0;i<Z3P_TAP_FRAMES-1;i++){Ps2PadUpdate(&s,&p,&c);assert(s.snes&8);}Ps2PadUpdate(&s,&p,&c);assert(!(s.snes&8));
  memset(&s,0,sizeof s);p.buttons=Z3P_SELECT;for(int i=0;i<Z3P_CHORD_WAIT;i++){Ps2PadUpdate(&s,&p,&c);assert(!(s.snes&4));}Ps2PadUpdate(&s,&p,&c);assert(s.snes&4); /* a held Select reaches the game */
  p.buttons=0;memset(&s,0,sizeof s);
  s.blocked=true;p.buttons=Z3P_CROSS;Ps2PadUpdate(&s,&p,&c);assert(!s.snes);
  p.buttons=0;Ps2PadUpdate(&s,&p,&c);assert(!s.blocked);p.buttons=Z3P_CROSS;Ps2PadUpdate(&s,&p,&c);assert(s.snes==1);
  for(int y=0;y<256;y++)for(int x=0;x<256;x++){unsigned d=Ps2PadDirections(x,y,24,0);assert((d&48)!=48&&(d&192)!=192);}
  assert(Ps2PadDirections(150,128,24,0)==0);assert(Ps2PadDirections(150,128,24,128)==128);assert(Ps2PadDirections(145,128,24,128)==0);
}
static void Corrupt(const char *name,long pos){FILE *f=fopen(name,"r+b");assert(f);assert(!fseek(f,pos,SEEK_SET));int c=fgetc(f);assert(c!=EOF);assert(!fseek(f,pos,SEEK_SET));fputc(c^0x55,f);assert(!fclose(f));}
static void Persistence(void){
  assert(!mkdir("saves",0700));Ps2Settings a,b;assert(!Ps2SettingsLoad(&a));assert(a.volume==100&&a.analog&&a.gs_palette&&a.frame_mode==1&&!a.frameskip);
  a.volume=35;assert(Ps2SettingsSave(&a));assert(Ps2SettingsLoad(&b));assert(!memcmp(&a,&b,sizeof a));
  a.show_fps=1;assert(Ps2SettingsSave(&a));assert(Ps2SettingsLoad(&b)&&b.show_fps);
  assert(!truncate("saves/zelda3.alt.ini",30));assert(Ps2SettingsLoad(&b)&&!b.show_fps&&b.volume==35);
  Corrupt("saves/zelda3.ini",15);assert(!Ps2SettingsLoad(&b)&&b.volume==100);
  a.volume=500;a.deadzone=-10;a.saturation=300;a.frame_mode=0;a.frameskip=1;a.gs_palette=0;assert(Ps2SettingsSave(&a));assert(Ps2SettingsLoad(&b));assert(b.volume==100&&b.deadzone==12&&b.saturation==200);
  assert(b.frame_mode==1&&b.frameskip==0&&b.gs_palette==1); /* the fixed options (60 FPS, frameskip off, PPU on the GS) cannot be changed through the settings file */
  unsigned char data[8192],out[8192];memset(data,0x42,sizeof data);memset(out,0xa1,sizeof out);
  assert(!Ps2SramLoad(out,sizeof out)&&out[0]==0xa1);assert(Ps2SramSave(data,sizeof data));assert(Ps2SramLoad(out,sizeof out)&&!memcmp(data,out,sizeof data));
  memset(data,0x72,sizeof data);assert(Ps2SramSave(data,sizeof data));assert(Ps2SramLoad(out,sizeof out)&&!memcmp(data,out,sizeof data));
  Corrupt("saves/sram.b",4);assert(Ps2SramLoad(out,sizeof out)&&out[0]==0x42);
  Corrupt("saves/sram.a",100);memset(out,0xaa,sizeof out);assert(!Ps2SramLoad(out,sizeof out)&&out[0]==0xaa);
  assert(!Ps2SramSave(data,100));
}
static void Migration(void) {
  assert(!mkdir("migration",0700));assert(!chdir("migration"));assert(!mkdir("saves",0700));
  Ps2Settings a,b;Ps2SettingsDefaults(&a);a.volume=35;a.swap_ab=1;a.deadzone=32;
  assert(Ps2SettingsSave(&a));
  char buf[2048];FILE *f=fopen("saves/zelda3.ini","rb");assert(f);size_t n=fread(buf,1,sizeof(buf)-1,f);fclose(f);buf[n]=0;
  char *version=strstr(buf,"Version=2");assert(version);version[8]='1';char *footer=strstr(buf,";CRC32=");assert(footer);
  /* a file from an earlier build: 30 FPS, frameskip on, palette expansion off */
  char *line=strstr(buf,"frame_mode=1");assert(line);line[11]='0';
  line=strstr(buf,"frameskip=0");assert(line);line[10]='1';
  line=strstr(buf,"gs_palette=1");assert(line);line[11]='0';
  snprintf(footer,sizeof(buf)-(footer-buf),";CRC32=%08x\n",(unsigned)Ps2Crc32(buf,footer-buf));
  f=fopen("saves/zelda3.ini","wb");assert(f);assert(fwrite(buf,1,strlen(buf),f)==strlen(buf));fclose(f);
  assert(Ps2SettingsLoad(&b));assert(b.frame_mode==1&&!b.frameskip&&b.gs_palette&&b.volume==35&&b.swap_ab==1&&b.deadzone==32); /* forced to the fixed values, controls kept */
  assert(Ps2SettingsSave(&b));assert(Ps2SettingsLoad(&a)&&a.frame_mode==1&&!a.frameskip&&a.gs_palette);
  /* A file saved before "frameskip" existed has no such line: it must still load, with frameskip off. */
  Ps2SettingsDefaults(&a);a.volume=55;a.frameskip=0;assert(Ps2SettingsSave(&a));
  f=fopen("saves/zelda3.ini","rb");assert(f);n=fread(buf,1,sizeof(buf)-1,f);fclose(f);buf[n]=0;
  line=strstr(buf,"frameskip=");assert(line);char *eol=strchr(line,'\n');assert(eol);memmove(line,eol+1,strlen(eol+1)+1);
  footer=strstr(buf,";CRC32=");assert(footer);snprintf(footer,sizeof(buf)-(footer-buf),";CRC32=%08x\n",(unsigned)Ps2Crc32(buf,footer-buf));
  f=fopen("saves/zelda3.ini","wb");assert(f);assert(fwrite(buf,1,strlen(buf),f)==strlen(buf));fclose(f);
  assert(Ps2SettingsLoad(&b)&&b.volume==55&&b.frameskip==0);
  /* Files saved while the removed "linecache" option existed still carry that line and must keep loading. */
  Ps2SettingsDefaults(&a);a.volume=44;assert(Ps2SettingsSave(&a));
  f=fopen("saves/zelda3.ini","rb");assert(f);n=fread(buf,1,sizeof(buf)-1,f);fclose(f);buf[n]=0;
  footer=strstr(buf,";CRC32=");assert(footer);memmove(footer+12,footer,strlen(footer)+1);memcpy(footer,"linecache=1\n",12);
  footer=strstr(buf,";CRC32=");snprintf(footer,sizeof(buf)-(footer-buf),";CRC32=%08x\n",(unsigned)Ps2Crc32(buf,footer-buf));
  f=fopen("saves/zelda3.ini","wb");assert(f);assert(fwrite(buf,1,strlen(buf),f)==strlen(buf));fclose(f);
  assert(Ps2SettingsLoad(&b)&&b.volume==44);
  /* A file saved in the removed "medidores" mode has show_fps=2: it must load and keep the plain FPS counter on. */
  Ps2SettingsDefaults(&a);a.show_fps=1;a.volume=33;assert(Ps2SettingsSave(&a));
  f=fopen("saves/zelda3.ini","rb");assert(f);n=fread(buf,1,sizeof(buf)-1,f);fclose(f);buf[n]=0;
  line=strstr(buf,"show_fps=1");assert(line);line[9]='2';
  footer=strstr(buf,";CRC32=");assert(footer);snprintf(footer,sizeof(buf)-(footer-buf),";CRC32=%08x\n",(unsigned)Ps2Crc32(buf,footer-buf));
  f=fopen("saves/zelda3.ini","wb");assert(f);assert(fwrite(buf,1,strlen(buf),f)==strlen(buf));fclose(f);
  assert(Ps2SettingsLoad(&b)&&b.volume==33&&b.show_fps==1);
  assert(!chdir(".."));
}
static void Palette(void){
  uint16_t ix[256];uint32_t pal[256];static uint32_t pixels[224][256];
  for(unsigned i=0;i<256;i++){ix[i]=0xab00|i;pal[i]=i*0x010203;}
  Ps2IndexedBegin(true);assert(Ps2IndexedLine(0,ix,pal,0));assert(Ps2IndexedLine(1,ix,pal,0));assert(g_ps2_indexed.palette_count==1);
  Ps2IndexedResolve((uint8_t *)pixels,1024);for(int x=0;x<256;x++){assert(pixels[0][x]==pal[x]);assert(pixels[1][x]==pal[x]);}
  for(unsigned i=1;i<32;i++){pal[0]=i;assert(Ps2IndexedLine(i+1,ix,pal,i));}pal[0]=99;assert(!Ps2IndexedLine(34,ix,pal,99));assert(g_ps2_indexed.line_palette[34]==-1);
  Ps2IndexedBegin(false);assert(!Ps2IndexedLine(0,ix,pal,0));
}
int main(void){char tmp[]="/tmp/z3-port-test-XXXXXX";assert(mkdtemp(tmp));assert(!chdir(tmp));Controls();Persistence();Migration();Palette();unsigned phase=0;unsigned long long total=0;for(int i=0;i<60000;i++)total+=Ps2AudioFrameSamples(&phase);assert(total==32000718&&phase==1920);phase=0;unsigned milliseconds=0;for(int i=0;i<60000;i++)milliseconds+=Ps2FrameMilliseconds(&phase);assert(milliseconds==1001000&&phase==0);puts("PASS: controllers (65536 stick positions), settings/SRAM recovery, palettes, audio sample clock");return 0;}
