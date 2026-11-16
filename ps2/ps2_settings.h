#ifndef Z3_PS2_SETTINGS_H
#define Z3_PS2_SETTINGS_H
#include <stdbool.h>
#include <stdint.h>
typedef struct Ps2Settings {
  int volume, stereo, analog, deadzone, swap_ab, swap_xy;
  int filtering, aspect, saturation, frame_mode, show_fps, gs_palette;
  int switch_lr, low_health_beep, dim_flashes;
} Ps2Settings;
extern Ps2Settings g_ps2_settings;
void Ps2SettingsDefaults(Ps2Settings *s);
void Ps2SettingsUseDirectBoot(Ps2Settings *s);
void Ps2SettingsClamp(Ps2Settings *s);
bool Ps2SettingsLoad(Ps2Settings *s);
bool Ps2SettingsSave(const Ps2Settings *s);
uint32_t Ps2Crc32(const void *data, unsigned size);
bool Ps2SramLoad(void *data, unsigned size);
bool Ps2SramSave(const void *data, unsigned size);
#endif
