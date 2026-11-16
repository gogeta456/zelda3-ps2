#ifndef Z3_PS2_AUDIO_TRANSPORT_H
#define Z3_PS2_AUDIO_TRANSPORT_H
#include <stdbool.h>
#define Z3_AUDIO_RPC_PUSH 0x5a33
#define Z3_AUDIO_PACKET_BYTES (534 * 2 * 2)
bool Ps2AudioTransportInit(void);
bool Ps2AudioTransportStart(const void *pcm, int bytes);
// 0 = in flight, 1 = done. Result: bytes accepted (0 = retry), prior queue size.
int Ps2AudioTransportPoll(int *accepted, int *queued);
#endif
