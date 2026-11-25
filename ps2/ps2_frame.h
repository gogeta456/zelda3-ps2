#ifndef Z3_PS2_FRAME_H
#define Z3_PS2_FRAME_H
#include <stdbool.h>
#include <stdint.h>
#include "ps2_timing.h"

typedef struct Ps2FrameClock {
  uint32_t deadline;
  unsigned phase, steps, skipped, late_skips;
} Ps2FrameClock;

static inline void Ps2FrameReset(Ps2FrameClock *clock, uint32_t now) {
  *clock = (Ps2FrameClock){.deadline = now};
}

/* Both modes update gameplay/audio at the same rate. 30 FPS omits every
 * other drawing pass, allowing both updates to use one 33 ms work budget.
 *
 * Late frames: when the clock is more than 34 ms behind, drawing is skipped for up
 * to two steps so gameplay can catch up. That is only valid while the audio queue is
 * actually starving. Gameplay is paced by that queue (and by the GS vsync), so once it
 * is healthy no step can finish faster than real time and a skip can never pay the
 * lateness back: the clock would stay "late" forever and keep dropping two of every
 * three drawing passes (10 FPS at 30 FPS mode, 20 at 60). So the lateness is forgiven
 * when the audio queue is healthy, and after the second skipped pass. With frameskip
 * off no pass is ever skipped for lateness: the game just runs slower when it cannot keep up. */
static inline bool Ps2FrameAdvance(Ps2FrameClock *clock, bool fps60, uint32_t now, bool audio_starved, bool frameskip) {
  clock->deadline += Ps2FrameMilliseconds(&clock->phase);
  ++clock->steps;
  if (!fps60 && (clock->steps & 1u)) return false;
  if ((int32_t)(now - clock->deadline) > 34) {
    if (frameskip && audio_starved && clock->skipped < 2) {
      ++clock->skipped;
      ++clock->late_skips;
      return false;
    }
    clock->deadline = now;
  }
  clock->skipped = 0;
  return true;
}

static inline unsigned Ps2FrameWait(Ps2FrameClock *clock, uint32_t now) {
  int32_t remaining = (int32_t)(clock->deadline - now);
  if (remaining < -250) Ps2FrameReset(clock, now);
  return remaining > 0 ? (unsigned)remaining : 0;
}
#endif
