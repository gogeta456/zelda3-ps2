#ifndef MOCK_TIMER_H
#define MOCK_TIMER_H
#include <stdint.h>
uint32_t SDL_GetTicks(void);
void SDL_Delay(uint32_t milliseconds);
#endif
