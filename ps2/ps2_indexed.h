#ifndef Z3_PS2_INDEXED_H
#define Z3_PS2_INDEXED_H
#include <stdbool.h>
#include <stdint.h>
enum{Z3_GS_WIDTH=256,Z3_GS_HEIGHT=224,Z3_GS_PALETTES=32};
typedef struct Ps2IndexedFrame{
 uint8_t pixels[256*256] __attribute__((aligned(128)));
 uint32_t palettes[Z3_GS_PALETTES][256] __attribute__((aligned(128)));
 int8_t line_palette[Z3_GS_HEIGHT];
 unsigned palette_count,indexed_lines;
 const uint32_t *last_palette;uint32_t last_revision;int last_index;
 bool enabled;
}Ps2IndexedFrame;
extern Ps2IndexedFrame g_ps2_indexed;
void Ps2IndexedBegin(bool enabled);
bool Ps2IndexedLine(unsigned y,const uint16_t *indices,const uint32_t *palette,uint32_t revision);
void Ps2IndexedResolve(uint8_t *rgba,int pitch);
#endif
