#ifdef _EE
#include "ps2_audio_transport.h"
#include <audsrv.h>
#include <kernel.h>
#include <sifrpc.h>
#include <string.h>
static SifRpcClientData_t client __attribute__((aligned(64)));
static struct { int bytes; unsigned char pcm[Z3_AUDIO_PACKET_BYTES]; }
    packet __attribute__((aligned(64)));
static int reply[16] __attribute__((aligned(64)));
bool Ps2AudioTransportInit(void) {
  // init_audio_driver has already bound and initialized the bundled audsrv.
  memset(&client, 0, sizeof(client));
  return sceSifBindRpc(&client, AUDSRV_IRX, 0) >= 0 && client.server != NULL;
}
bool Ps2AudioTransportStart(const void *pcm, int bytes) {
  if (bytes <= 0 || bytes > Z3_AUDIO_PACKET_BYTES || sceSifCheckStatRpc(&client))
    return false;
  packet.bytes = bytes;
  memcpy(packet.pcm, pcm, bytes);
  // Neither buffer is touched again until Poll sees completion. SIF flushes
  // the send cache; the receive buffer is read through its uncached alias.
  return sceSifCallRpc(&client, Z3_AUDIO_RPC_PUSH, SIF_RPC_M_NOWAIT,
      &packet, bytes + sizeof(int), reply, 8, NULL, NULL) >= 0;
}
int Ps2AudioTransportPoll(int *accepted, int *queued) {
  if (sceSifCheckStatRpc(&client)) return 0;
  volatile int *result = UNCACHED_SEG(reply);
  *accepted = result[0]; *queued = result[1];
  return 1;
}
#endif
