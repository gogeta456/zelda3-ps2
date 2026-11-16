#include "ps2_settings.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include "src/ps2_compat.h"
Ps2Settings g_ps2_settings;
typedef struct Setting { const char *name; size_t offset; int def,min,max; } Setting;
#define FIELD(n,d,lo,hi) {#n,offsetof(Ps2Settings,n),d,lo,hi}
static const Setting fields[]={
 FIELD(volume,100,0,100),FIELD(stereo,1,0,1),FIELD(analog,1,0,1),
 FIELD(deadzone,24,12,64),FIELD(swap_ab,0,0,1),FIELD(swap_xy,0,0,1),
 FIELD(filtering,0,0,1),FIELD(aspect,0,0,1),FIELD(saturation,100,0,200),
 FIELD(frame_mode,1,0,1),FIELD(show_fps,0,0,1),FIELD(gs_palette,0,0,1),
 FIELD(switch_lr,0,0,1),FIELD(low_health_beep,1,0,1),FIELD(dim_flashes,0,0,1)
};
#undef FIELD
#define NFIELDS (sizeof(fields)/sizeof(fields[0]))
static const char *const config_paths[]={"saves/zelda3.ini","saves/zelda3.alt.ini"};
static const char *const sram_paths[]={"saves/sram.a","saves/sram.b"};
uint32_t Ps2Crc32(const void *data,unsigned size) {
 const unsigned char *p=data; uint32_t crc=~0u;
 while(size--) { crc^=*p++; for(int b=0;b<8;b++) crc=(crc>>1)^(0xedb88320u & (0u-(crc&1))); }
 return ~crc;
}
void Ps2SettingsDefaults(Ps2Settings *s) {
 for(unsigned i=0;i<NFIELDS;i++) *(int *)((char *)s+fields[i].offset)=fields[i].def;
}
void Ps2SettingsUseDirectBoot(Ps2Settings *s) {
 // No configuration writes: preserve stored preferences for a future menu.
 s->frame_mode=1;
 s->gs_palette=0; // compatibility path until GS palette performance is measured
}
void Ps2SettingsClamp(Ps2Settings *s) {
 for(unsigned i=0;i<NFIELDS;i++) {
  int *v=(int *)((char *)s+fields[i].offset);
  if(*v<fields[i].min) *v=fields[i].min;
  if(*v>fields[i].max) *v=fields[i].max;
 }
}
static bool Newer(uint32_t a,uint32_t b) { return (int32_t)(a-b)>0; }
static bool WriteChecked(const char *path,const void *data,unsigned size) {
 FILE *f=fopen(path,"wb"); if(!f) return false;
 bool ok=fwrite(data,1,size,f)==size;
 if(fflush(f)!=0) ok=false;
 if(fclose(f)!=0) ok=false;
 return ok;
}
static bool ReadConfig(int bank,Ps2Settings *s,uint32_t *generation) {
 char buf[2048]; FILE *f=fopen(config_paths[bank],"rb"); if(!f) return false;
 size_t n=fread(buf,1,sizeof(buf)-1,f); bool ok=!ferror(f)&&feof(f); fclose(f);
 if(!ok) return false;
 buf[n]=0; char *footer=strstr(buf,";CRC32="); unsigned crc; int used=0;
 if(!footer || sscanf(footer,";CRC32=%8x\n%n",&crc,&used)!=1 || footer+used!=buf+n ||
    crc!=Ps2Crc32(buf,(unsigned)(footer-buf))) return false;
 *footer=0; unsigned version,gen; used=0;
 if(sscanf(buf,"[PS2]\nVersion=%u\nGeneration=%u\n%n",&version,&gen,&used)!=2 ||
    !used || (version!=1 && version!=2)) return false;
 *generation=gen; Ps2SettingsDefaults(s); uint32_t seen=0;
 for(char *p=buf+used;*p;) {
  char *eq=strchr(p,'='),*nl=strchr(p,'\n'),*end;
  if(!eq||!nl||eq>nl) return false;
  *eq=0; errno=0; long v=strtol(eq+1,&end,10);
  if(errno||end!=nl||v<INT_MIN||v>INT_MAX) return false;
  bool found=false;
  for(unsigned i=0;i<NFIELDS;i++) if(!strcmp(p,fields[i].name)) {
   if(seen&(1u<<i)) return false;
   *(int *)((char *)s+fields[i].offset)=(int)v; seen|=1u<<i; found=true; break;
  }
  if(!found) return false;
  p=nl+1;
 }
 if(seen!=(1u<<NFIELDS)-1) return false;
 // Revision 1 defaulted to Auto/60 with experimental palette streaming.
 // Migrate only these performance settings; retain controls, sound and saves.
 if(version==1) { s->frame_mode=0; s->gs_palette=0; }
 Ps2SettingsClamp(s); return true;
}
bool Ps2SettingsLoad(Ps2Settings *s) {
 Ps2Settings a,b; uint32_t ga=0,gb=0;
 bool va=ReadConfig(0,&a,&ga),vb=ReadConfig(1,&b,&gb);
 if(!va&&!vb) { Ps2SettingsDefaults(s); return false; }
 *s=vb&&(!va||Newer(gb,ga))?b:a; return true;
}
bool Ps2SettingsSave(const Ps2Settings *settings) {
 Ps2Settings s=*settings,old[2],check; uint32_t gen[2]={0,0},checked;
 bool valid[2]={ReadConfig(0,&old[0],&gen[0]),ReadConfig(1,&old[1],&gen[1])};
 int latest=valid[1]&&(!valid[0]||Newer(gen[1],gen[0]))?1:0;
 int bank=!valid[0]?0:!valid[1]?1:1-latest;
 uint32_t next=valid[0]||valid[1]?gen[latest]+1:1;
 Ps2SettingsClamp(&s); char buf[2048];
 int n=snprintf(buf,sizeof(buf),"[PS2]\nVersion=2\nGeneration=%u\n",(unsigned)next);
 for(unsigned i=0;i<NFIELDS;i++)
  n+=snprintf(buf+n,sizeof(buf)-n,"%s=%d\n",fields[i].name,*(int *)((char *)&s+fields[i].offset));
 uint32_t crc=Ps2Crc32(buf,n);
 n+=snprintf(buf+n,sizeof(buf)-n,";CRC32=%08x\n",(unsigned)crc);
 return WriteChecked(config_paths[bank],buf,n)&&ReadConfig(bank,&check,&checked)&&
        checked==next&&!memcmp(&check,&s,sizeof(s));
}
/* Both SRAM data and generation are protected. Write the older bank only. */
static void Put32(unsigned char *p,uint32_t v) { for(int i=0;i<4;i++) p[i]=v>>(i*8); }
static uint32_t Get32(const unsigned char *p) { return p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24; }
static bool ReadSram(int bank,void *data,unsigned size,uint32_t *gen) {
 unsigned char h[16]; FILE *f=fopen(sram_paths[bank],"rb"); if(!f) return false;
 bool ok=fread(h,1,16,f)==16&&!memcmp(h,"Z3S1",4)&&Get32(h+8)==size&&
         fread(data,1,size,f)==size&&fgetc(f)==EOF&&!ferror(f);
 fclose(f);
 if(!ok||Get32(h+12)!=(Ps2Crc32(data,size)^Ps2Crc32(h,12))) return false;
 *gen=Get32(h+4); return true;
}
bool Ps2SramLoad(void *data,unsigned size) {
 if(size!=8192) return false;
 unsigned char *buf=malloc(size*2); if(!buf) return false;
 uint32_t gen[2]={0,0}; bool a=ReadSram(0,buf,size,&gen[0]),b=ReadSram(1,buf+size,size,&gen[1]);
 if(a||b) memcpy(data,buf+((b&&(!a||Newer(gen[1],gen[0])))?size:0),size);
 free(buf); return a||b;
}
bool Ps2SramSave(const void *data,unsigned size) {
 if(size!=8192) return false;
 unsigned char *buf=malloc(size+16),*check=malloc(size);
 if(!buf||!check) { free(buf);free(check);return false; }
 uint32_t gen[2]={0,0},read_gen;
 bool valid[2]={ReadSram(0,check,size,&gen[0]),ReadSram(1,check,size,&gen[1])};
 int latest=valid[1]&&(!valid[0]||Newer(gen[1],gen[0]))?1:0;
 int bank=!valid[0]?0:!valid[1]?1:1-latest;
 uint32_t next=valid[0]||valid[1]?gen[latest]+1:1;
 memcpy(buf,"Z3S1",4);Put32(buf+4,next);Put32(buf+8,size);
 Put32(buf+12,Ps2Crc32(data,size)^Ps2Crc32(buf,12));memcpy(buf+16,data,size);
 bool ok=WriteChecked(sram_paths[bank],buf,size+16)&&ReadSram(bank,check,size,&read_gen)&&read_gen==next&&!memcmp(data,check,size);
 free(buf);free(check);return ok;
}
