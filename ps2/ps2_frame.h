#ifndef Z3_PS2_FRAME_H
#define Z3_PS2_FRAME_H
#include <stdbool.h>
#include <stdint.h>
#include "ps2_timing.h"

typedef struct Ps2FrameClock {
  uint32_t deadline;
  unsigned phase, steps;
} Ps2FrameClock;

static inline void Ps2FrameReset(Ps2FrameClock *clock, uint32_t now) {
  *clock = (Ps2FrameClock){.deadline = now};
}

/* Both modes update gameplay/audio at the same rate. 30 FPS omits every
 * other drawing pass, allowing both updates to use one 33 ms work budget. */
static inline bool Ps2FrameAdvance(Ps2FrameClock *clock, bool fps60) {
  clock->deadline += Ps2FrameMilliseconds(&clock->phase);
  ++clock->steps;
  // Never discard a picture to catch up after a slow frame. The only reduced
  // presentation rate is the explicitly selected 30 FPS mode.
  return fps60 || !(clock->steps & 1u);
}

static inline unsigned Ps2FrameWait(Ps2FrameClock *clock, uint32_t now) {
  int32_t remaining = (int32_t)(clock->deadline - now);
  // Rebase after a long stall without changing the 30 FPS presentation parity.
  if (remaining < -250) clock->deadline = now;
  return remaining > 0 ? (unsigned)remaining : 0;
}
#endif
