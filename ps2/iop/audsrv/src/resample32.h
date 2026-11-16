#ifndef Z3_RESAMPLE32_H
#define Z3_RESAMPLE32_H
#include <stdint.h>
// Exact 32 kHz -> 48 kHz nearest-neighbor conversion. Preserve the fractional
// phase across SPU blocks; consuming 341 every time loses 32 samples/second.
// src must contain 342 interleaved frames (one frame of lookahead at phase 1).
static inline unsigned z3_resample32(const int16_t *src, int channels,
    int16_t *left, int16_t *right, unsigned *phase) {
  unsigned p = *phase, index = 0;
  for (unsigned i = 0; i < 512; i++) {
    left[i] = src[index * channels];
    right[i] = src[index * channels + (channels == 2)];
    p += 2;
    if (p >= 3) { p -= 3; ++index; }
  }
  *phase = p;
  return index;
}
#endif
