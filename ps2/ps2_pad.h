#ifndef Z3_PS2_PAD_H
#define Z3_PS2_PAD_H
#include <stdbool.h>
#include <stdint.h>
#include "ps2_settings.h"
enum {
 Z3P_SELECT=1,Z3P_L3=2,Z3P_R3=4,Z3P_START=8,
 Z3P_UP=16,Z3P_RIGHT=32,Z3P_DOWN=64,Z3P_LEFT=128,
 Z3P_L2=256,Z3P_R2=512,Z3P_L1=1024,Z3P_R1=2048,
 Z3P_TRIANGLE=4096,Z3P_CIRCLE=8192,Z3P_CROSS=16384,Z3P_SQUARE=32768,
 Z3P_MENU=Z3P_SELECT|Z3P_START
};
typedef struct Ps2PadSample { uint16_t buttons; uint8_t x,y; bool analog,connected; } Ps2PadSample;
// ss_wait/tap_*: Start and Select are held back for a few frames so that pressing them together (the menu chord) never leaks a
// lone Start or Select to the game; a short tap is replayed on release.
typedef struct Ps2PadState { uint16_t held,pressed,snes; uint8_t axes; bool blocked,menu_latch; uint8_t ss_wait,tap_left; uint16_t ss_last,tap_snes; } Ps2PadState;
enum { Z3P_CHORD_WAIT=4,Z3P_TAP_FRAMES=3 };
uint8_t Ps2PadDirections(uint8_t x,uint8_t y,int deadzone,uint8_t previous);
bool Ps2PadUpdate(Ps2PadState *s,const Ps2PadSample *p,const Ps2Settings *cfg);
bool Ps2PadInit(void);
Ps2PadSample Ps2PadPoll(void);
void Ps2PadShutdown(void);
#endif
