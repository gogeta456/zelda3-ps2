#ifndef Z3_PS2_MENU_H
#define Z3_PS2_MENU_H
#include "ps2_pad.h"
/* Returns true when the edited settings were applied, false when they were discarded.
 * Ps2MenuExitRequested() is true after a call in which the player chose "SAIR DO JOGO". */
bool Ps2MenuRun(Ps2PadState *pad);
bool Ps2MenuExitRequested(void);
#endif
