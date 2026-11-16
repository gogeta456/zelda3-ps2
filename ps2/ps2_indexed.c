#include "ps2_indexed.h"
#include <string.h>
Ps2IndexedFrame g_ps2_indexed;
void Ps2IndexedBegin(bool enabled){
 memset(g_ps2_indexed.line_palette,-1,sizeof(g_ps2_indexed.line_palette));
 g_ps2_indexed.palette_count=0;g_ps2_indexed.indexed_lines=0;
 g_ps2_indexed.last_palette=NULL;g_ps2_indexed.last_index=-1;g_ps2_indexed.enabled=enabled;
}
bool Ps2IndexedLine(unsigned y,const uint16_t *indices,const uint32_t *palette,uint32_t revision){
 Ps2IndexedFrame *f=&g_ps2_indexed;if(!f->enabled||y>=224)return false;unsigned p;
 if(f->last_palette==palette&&f->last_revision==revision&&f->last_index>=0)p=f->last_index;
 else{
  for(p=0;p<f->palette_count;p++)if(!memcmp(palette,f->palettes[p],1024))break;
  if(p==Z3_GS_PALETTES)return false;
  if(p==f->palette_count){memcpy(f->palettes[p],palette,1024);f->palette_count++;}
  f->last_palette=palette;f->last_revision=revision;f->last_index=p;
 }
 for(unsigned x=0;x<256;x++)f->pixels[y*256+x]=(uint8_t)indices[x];
 f->line_palette[y]=p;f->indexed_lines++;return true;
}
// Reference GS palette lookup for host regression tests; not used by the PS2.
void Ps2IndexedResolve(uint8_t *rgba,int pitch){
 for(int y=0;y<224;y++){
  int p=g_ps2_indexed.line_palette[y];if(p<0)continue;
  uint32_t *dst=(uint32_t *)(rgba+y*pitch);
  for(int x=0;x<256;x++)dst[x]=g_ps2_indexed.palettes[p][g_ps2_indexed.pixels[y*256+x]];
 }
}
