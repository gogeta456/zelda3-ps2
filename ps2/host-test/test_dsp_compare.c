#include "snes/dsp.h"
#include <assert.h>
#include <stddef.h>
void Reference_dsp_reset(Dsp *);
void Reference_dsp_write(Dsp *, uint8_t, uint8_t);
void Reference_dsp_cycle(Dsp *);
void Reference_dsp_getSamples(Dsp *, int16_t *, int, int);
static uint32_t random_state = 0x4419;
static uint32_t Random(void) { return random_state = random_state * 1664525u + 1013904223u; }
int main(void) {
  static uint8_t ram_a[65536], ram_b[65536];
  static Dsp a, b;
  int16_t pcm_a[1068], pcm_b[1068];
  for (int i = 0; i < 65536; i++) ram_a[i] = ram_b[i] = Random() >> 24;
  a.apu_ram = ram_a; b.apu_ram = ram_b;
  dsp_reset(&a); Reference_dsp_reset(&b);
  for (int cycle = 0; cycle < 534 * 1024; cycle++) {
    if (cycle % 13 == 0) {
      uint8_t reg = Random() >> 25, value = Random() >> 24;
      dsp_write(&a, reg, value); Reference_dsp_write(&b, reg, value);
    }
    dsp_cycle(&a); Reference_dsp_cycle(&b);
    if (a.sampleOffset == 534) {
      int channels = (Random() >> 24 & 1) + 1;
      int samples = (Random() >> 24 & 1) + 533;
      memset(pcm_a, 0x55, sizeof(pcm_a)); memset(pcm_b, 0x55, sizeof(pcm_b));
      dsp_getSamples(&a, pcm_a, samples, channels);
      Reference_dsp_getSamples(&b, pcm_b, samples, channels);
      assert(!memcmp(pcm_a, pcm_b, sizeof(pcm_a)));
      assert(!memcmp(&a.ram, &b.ram, sizeof(Dsp) - offsetof(Dsp, ram)));
      assert(!memcmp(ram_a, ram_b, sizeof(ram_a)));
    }
  }
  puts("PASS: 546816 DSP cycles, PCM/echo RAM/internal state identical to previous DSP");
  return 0;
}
