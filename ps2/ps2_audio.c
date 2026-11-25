#ifdef _EE
#include "ps2_audio.h"
#include "src/audio.h"
#include <audsrv.h>
#include <ps2_audio_driver.h>
#include <SDL_timer.h>

static bool ready, paused;
static int channels, volume, frame_bytes, feed_bytes;
static unsigned phase, recoveries;
static bool starved = true;
static int16_t pcm[534 * 2] __attribute__((aligned(64)));
static const char silence[4096] __attribute__((aligned(64))) = {0};

bool Ps2AudioInit(int ch, int vol) {
  channels = ch == 1 ? 1 : 2;
  volume = vol < 0 ? 0 : vol > 100 ? 100 : vol;
  phase = 0;
  paused = false;
  ready = false;
  frame_bytes = 534 * channels * 2;
  feed_bytes = ((512 * 32000) / 48000) * channels * 2;
  if (init_audio_driver() < 0) return false;
  audsrv_stop_audio();
  struct audsrv_fmt_t fmt = {32000, 16, channels};
  if (audsrv_set_format(&fmt) != 0 || audsrv_set_volume(volume) != 0) return false;
  /* The bundled IOP server starts empty and never advances into unwritten
   * PCM. Two fragments (about 21 ms) cover scheduling jitter. No drain loop,
   * speculative synthesis, or runtime format reset is necessary. */
  ready = audsrv_play_audio(silence, feed_bytes * 2) == feed_bytes * 2;
  if (!ready) audsrv_stop_audio();
  return ready;
}

void Ps2AudioShutdown(void) {
  audsrv_stop_audio();
  ready = false;
  deinit_audio_driver();
}

void Ps2AudioPause(bool pause) {
  if (!ready || paused == pause) return;
  paused = pause;
  if (paused) audsrv_stop_audio();
  else if (audsrv_play_audio(silence, 4) != 4) ready = false;
}

void Ps2AudioVolume(int v) {
  volume = v < 0 ? 0 : v > 100 ? 100 : v;
  if (ready) audsrv_set_volume(volume);
}

bool Ps2AudioSubmitFrame(void) {
  if (paused) { starved = true; return false; }
  int samples = Ps2AudioFrameSamples(&phase);
  ZeldaRenderAudio(pcm, samples, channels);
  if (!ready) { starved = true; return false; }

  int q = audsrv_queued();
  if (q < 0) goto fail;
  starved = q < frame_bytes;
  if (q < feed_bytes) ++recoveries; // low-water diagnostic; no blocking recovery
  /* Keep at most three game frames queued. In the default 30 FPS mode two
   * game/audio updates precede one drawing pass, without doubling game speed. */
  uint32_t start = SDL_GetTicks();
  while (q > frame_bytes * 2) {
    if ((uint32_t)(SDL_GetTicks() - start) > 40) goto fail;
    SDL_Delay(1);
    q = audsrv_queued();
  }
  if (q < 0 || audsrv_play_audio((const char *)pcm, samples * channels * 2) != samples * channels * 2)
    goto fail;
  return true;
fail:
  audsrv_stop_audio();
  ready = false;
  starved = true;
  return false;
}

bool Ps2AudioStarved(void) { return starved; }

unsigned Ps2AudioRecoveries(void) { return recoveries; }
bool Ps2AudioReady(void) { return ready; }
#endif
