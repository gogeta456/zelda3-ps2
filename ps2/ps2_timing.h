#ifndef Z3_PS2_TIMING_H
#define Z3_PS2_TIMING_H
/* NTSC presentation is 60000/1001 fields/s. The bundled AUDSRV 32 kHz
 * converter consumes exactly 341 input samples per 512 output samples at
 * 48 kHz (31968.75 input samples/s). Match that actual consumption rate,
 * otherwise a nominal 32000-sample clock slowly fills the ring and stalls
 * gameplay. ZeldaRenderAudio resamples each complete game audio frame into
 * this budget, so changing the presentation mode does not change game speed.
 * Reduced fraction: (341 * 48000 / 512) / (60000 / 1001) = 1365364 / 2560.
 */
static inline int Ps2AudioFrameSamples(unsigned *phase) {
  *phase += 1365364u;
  int samples = *phase / 2560u;
  *phase %= 2560u;
  return samples;
}
static inline unsigned Ps2FrameMilliseconds(unsigned *phase) {
  *phase += 1001u;
  unsigned milliseconds = *phase / 60u;
  *phase %= 60u;
  return milliseconds;
}
#endif
