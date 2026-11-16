// PlayStation 2 compatibility layer: finds the data directory (works on real hardware and on PCSX2/ARMSX2 "host:")
// and writes a small progress log (zelda3_log.txt) that helps diagnose problems on a machine without a debugger.
#ifndef ZELDA3_PS2_COMPAT_H_
#define ZELDA3_PS2_COMPAT_H_
#ifdef _EE
#include <stdio.h>
#include <sys/stat.h>
FILE *ps2_fopen(const char *path, const char *mode);
int ps2_mkdir(const char *path, int mode);
int ps2_rename(const char *from, const char *to);
void ps2_log(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void ps2_pick_data_dir(const char *argv0);
void ps2_log_stop_file(void);
void ps2_prepare_save_dir(void);
const char *ps2_save_location(void);
#define fopen(p, m) ps2_fopen(p, m)
#define mkdir(p, m) ps2_mkdir(p, m)
#define rename(a, b) ps2_rename(a, b)
#define Z3LOG(...) ps2_log(__VA_ARGS__)
#define Z3LOG_STOP() ps2_log_stop_file()
#else
#define Z3LOG(...) ((void)0)
#define Z3LOG_STOP() ((void)0)
#endif
#endif
