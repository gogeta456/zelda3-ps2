#include "snes/dsp.h"
#include <assert.h>

// Differential check against the previous float resampler, including rates
// used by desktop builds, saturation edges and mono negative rounding.
static void Reference(const Dsp *dsp, int16_t *out, int count, int channels) {
  float step = 534.0f / count, position = 0;
  for (int i = 0; i < count; i++, position += step) {
    int left = dsp->sampleBuffer[(int)position * 2];
    int right = dsp->sampleBuffer[(int)position * 2 + 1];
    if (channels == 1) out[i] = (left + right) >> 1;
    else { out[i * 2] = left; out[i * 2 + 1] = right; }
  }
}
int main(void) {
  Dsp dsp = {0};
  int16_t expected[4096], actual[4096];
  const int counts[] = {1, 267, 532, 533, 534, 535, 735, 800, 1600};
  uint32_t random = 17;
  for (int pass = 0; pass < 1000; pass++) {
    for (int i = 0; i < 1068; i++) {
      random = random * 1664525u + 1013904223u;
      dsp.sampleBuffer[i] = pass == 0 ? (i & 1 ? INT16_MIN : INT16_MAX) : (int16_t)random;
    }
    for (unsigned n = 0; n < sizeof(counts) / sizeof(*counts); n++) {
      for (int channels = 1; channels <= 2; channels++) {
        memset(actual, 0x55, sizeof(actual));
        memset(expected, 0x55, sizeof(expected));
        Reference(&dsp, expected, counts[n], channels);
        dsp.sampleOffset = 534;
        dsp_getSamples(&dsp, actual, counts[n], channels);
        assert(dsp.sampleOffset == 0);
        assert(!memcmp(expected, actual, sizeof(actual)));
      }
    }
  }
  puts("PASS: 18000 PCM comparisons against previous resampler, mono/stereo and guard bytes");
  return 0;
}
