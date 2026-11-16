#ifdef _EE
#include "ps2_audio.h"
#include "ps2_audio_transport.h"
#include "src/audio.h"
#include <audsrv.h>
#include <ps2_audio_driver.h>
#include <SDL_timer.h>
static bool ready, paused, in_flight;
static int channels, volume, feed_bytes;
static unsigned phase, recoveries, dropped;
// Retain PCM until acknowledged: no buffer reuse while SIF owns a transfer.
static struct { int bytes; int16_t pcm[534 * 2]; } pending[4];
static unsigned head, count;
static int16_t discard[534 * 2];
static const char silence[4096] __attribute__((aligned(64))) = {0};
static void PollOutput(void) {
  int accepted, queued;
  if (!in_flight || !Ps2AudioTransportPoll(&accepted, &queued)) return;
  in_flight = false;
  if (accepted == pending[head].bytes) {
    if (queued < feed_bytes) ++recoveries;
    head = (head + 1) % 4; --count;
  } else if (accepted != 0) ready = false;
}
static bool DrainTransfer(void) {
  // Only initialization/shutdown may wait. Never used in the game loop.
  uint32_t start = SDL_GetTicks();
  while (in_flight) {
    PollOutput();
    if (!in_flight) break;
    if ((uint32_t)(SDL_GetTicks() - start) >= 100) return false;
    SDL_Delay(1);
  }
  return true;
}
bool Ps2AudioInit(int ch, int vol) {
  if (!DrainTransfer()) return false;
  channels = ch == 1 ? 1 : 2;
  volume = vol < 0 ? 0 : vol > 100 ? 100 : vol;
  phase = head = count = recoveries = dropped = 0;
  paused = ready = false;
  feed_bytes = 342 * channels * 2;
  if (init_audio_driver() < 0) return false;
  audsrv_stop_audio();
  struct audsrv_fmt_t fmt = {32000, 16, channels};
  if (audsrv_set_format(&fmt) != 0 || audsrv_set_volume(volume) != 0 ||
      !Ps2AudioTransportInit()) return false;
  ready = audsrv_play_audio(silence, feed_bytes * 2) == feed_bytes * 2;
  if (!ready) audsrv_stop_audio();
  return ready;
}
void Ps2AudioShutdown(void) {
  ready = false;
  if (!DrainTransfer()) return; // Keep SIF-owned storage alive on a stuck IOP.
  audsrv_stop_audio(); deinit_audio_driver();
}
void Ps2AudioPause(bool pause) {
  if (!ready || paused == pause) return;
  if (!DrainTransfer()) { ready = false; return; }
  paused = pause;
  if (paused) audsrv_stop_audio();
  else if (audsrv_play_audio(silence, 4) != 4) ready = false;
}
void Ps2AudioVolume(int v) {
  volume = v < 0 ? 0 : v > 100 ? 100 : v;
  if (ready && !in_flight) audsrv_set_volume(volume);
}
bool Ps2AudioSubmitFrame(void) {
  if (paused) return false;
  PollOutput();
  int samples = Ps2AudioFrameSamples(&phase);
  if (!ready || count == 4) {
    // DSP/game state still advances when the device stalls. Only audio can
    // overflow: gameplay and video frames are never discarded.
    ZeldaRenderAudio(discard, samples, channels);
    if (count == 4) ++dropped;
    // A full retained queue must still retry after a busy IOP replied 0.
    if (ready && !in_flight) {
      in_flight = Ps2AudioTransportStart(pending[head].pcm, pending[head].bytes);
      if (!in_flight) ready = false;
    }
    return false;
  }
  unsigned tail = (head + count) % 4;
  ZeldaRenderAudio(pending[tail].pcm, samples, channels);
  pending[tail].bytes = samples * channels * 2; ++count;
  if (!in_flight) {
    in_flight = Ps2AudioTransportStart(pending[head].pcm, pending[head].bytes);
    if (!in_flight) ready = false;
  }
  return ready;
}
unsigned Ps2AudioRecoveries(void) { return recoveries; }
unsigned Ps2AudioDropped(void) { return dropped; }
bool Ps2AudioReady(void) { return ready; }
#endif
