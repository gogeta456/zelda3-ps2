#ifndef Z3_PS2_AUDIO_H
#define Z3_PS2_AUDIO_H
#include <stdbool.h>
#include <stdint.h>
bool Ps2AudioInit(int channels,int volume);
void Ps2AudioShutdown(void);
void Ps2AudioPause(bool paused);
void Ps2AudioVolume(int volume);
bool Ps2AudioSubmitFrame(void);
unsigned Ps2AudioRecoveries(void);
bool Ps2AudioReady(void);
/* True when the PCM queue held less than one game frame at the last submit (or audio is off):
 * only then is the game really behind real time. */
bool Ps2AudioStarved(void);
#include "ps2_timing.h"
#endif
