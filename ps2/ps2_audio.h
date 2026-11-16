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
unsigned Ps2AudioDropped(void);
bool Ps2AudioReady(void);
#include "ps2_timing.h"
#endif
