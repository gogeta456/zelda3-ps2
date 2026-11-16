#ifndef MOCK_AUDSRV_H
#define MOCK_AUDSRV_H
#define AUDSRV_IRX 0x870884e
struct audsrv_fmt_t{int freq,bits,channels;};
int audsrv_stop_audio(void);
int audsrv_set_volume(int volume);
int audsrv_set_format(struct audsrv_fmt_t *format);
int audsrv_available(void);
int audsrv_play_audio(const char *data,int bytes);
int audsrv_queued(void);
#endif
