#ifndef Z3_PS2_TIMING_H
#define Z3_PS2_TIMING_H
/* NTSC presentation is 60000/1001 fields per second. Use the same rational
 * clock for PCM and deadlines, so a nominal "60 FPS" mode does not slowly
 * drain the audio ring or periodically skip pictures on a full-speed PS2. */
static inline int Ps2AudioFrameSamples(unsigned *phase) {
  *phase += 32000u * 1001u;
  int samples = *phase / 60000u;
  *phase %= 60000u;
  return samples;
}
static inline unsigned Ps2FrameMilliseconds(unsigned *phase) {
  *phase += 1001u;
  unsigned milliseconds = *phase / 60u;
  *phase %= 60u;
  return milliseconds;
}
#endif
