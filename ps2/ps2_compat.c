// See src/ps2_compat.h
#ifdef _EE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>

extern const unsigned char kSaveIconSys[], kSaveIconSysEnd[], kSaveIcon[], kSaveIconEnd[];  // ps2/save_icon.S
FILE *ps2_fopen(const char *path, const char *mode);

static int g_style;              // spelling of file names on this medium (see MakeName)
static char g_dir[256];          // prefix prepended to relative paths ("" = use the cwd as is)

static int IsRelative(const char *p) { return p[0] != '/' && p[0] != '\\' && strchr(p, ':') == NULL; }

// ---- file names on different media ----
// Discs (ISO9660) hold upper-case names with a version suffix ("ZELDA3.INI;1"), and level 1 images only allow 8.3 names.
enum { kStyleAsIs, kStyleIso, kStyleIsoNoVer, kStyleShort, kNumStyles };

static const char *Alias83(const char *upper) {
  if (!strcmp(upper, "ZELDA3_ASSETS.DAT"))
    return "Z3ASSETS.DAT";
  return upper;
}

static void MakeName(char *out, size_t cap, const char *dir, const char *rel, int style) {
  char up[128];
  size_t i = 0;
  for (; rel[i] && i < sizeof(up) - 1; i++)
    up[i] = (rel[i] >= 'a' && rel[i] <= 'z') ? (char)(rel[i] - 32) : rel[i];
  up[i] = 0;
  switch (style) {
  case kStyleAsIs: snprintf(out, cap, "%s%s", dir, rel); break;
  case kStyleIso: snprintf(out, cap, "%s%s;1", dir, up); break;
  case kStyleIsoNoVer: snprintf(out, cap, "%s%s", dir, up); break;
  default: snprintf(out, cap, "%s%s;1", dir, Alias83(up)); break;
  }
}

static int IsDiscPath(const char *p) {
  return !strncmp(p, "cdrom", 5) || !strncmp(p, "cdfs", 4);
}

// Opens `rel` inside `dir`, trying the spellings that make sense for the medium; *style_out tells which one worked.
static FILE *OpenIn(const char *dir, const char *rel, const char *mode, int first_style, int *style_out) {
  char name[400];
  int disc = IsDiscPath(dir);
  for (int k = 0; k < (disc ? (int)kNumStyles : 1); k++) {
    int style = disc ? (first_style + k) % kNumStyles : kStyleAsIs;
    MakeName(name, sizeof(name), dir, rel, style);
    FILE *f = fopen(name, mode);
    if (f) {
      if (style_out)
        *style_out = style;
      return f;
    }
  }
  return NULL;
}

void ps2_pick_data_dir(const char *argv0) {
  static const char kWanted[] = "zelda3_assets.dat";
  char cand[8][256];
  char argv_dir[256];
  int n = 0;
  argv_dir[0] = 0;
  if (argv0) {  // folder of the ELF, taken literally from argv[0]
    const char *cut = NULL;
    for (const char *s = argv0; *s; s++)
      if (*s == '/' || *s == '\\' || *s == ':')
        cut = s;
    if (cut && (size_t)(cut - argv0) + 2 < sizeof(argv_dir)) {
      memcpy(argv_dir, argv0, cut - argv0 + 1);
      argv_dir[cut - argv0 + 1] = 0;
    }
  }
  if (argv0 && IsDiscPath(argv0)) {          // started from a CD/DVD: the files are next to the ELF
    if (argv_dir[0]) strcpy(cand[n++], argv_dir);
    strcpy(cand[n++], "cdrom0:\\");
    strcpy(cand[n++], "cdfs:/");
  } else {
    cand[n++][0] = 0;                        // cwd as set up by the libc from argv[0] (USB, memory card, HDD, ...)
    strcpy(cand[n++], "host:");              // PCSX2 / ARMSX2 "host:" root = folder that holds the ELF
    if (argv_dir[0]) strcpy(cand[n++], argv_dir);
    strcpy(cand[n++], "mass:/zelda3/");
    strcpy(cand[n++], "mass:/");
  }
  for (int i = 0; i < n; i++) {
    int style = kStyleAsIs;
    FILE *f = OpenIn(cand[i], kWanted, "rb", kStyleAsIs, &style);
    if (f) {
      fclose(f);
      strcpy(g_dir, cand[i]);
      g_style = style;
      return;
    }
  }
}

// ---- saves go to the memory card ----
// The game keeps its battery save in saves/sram.dat. On the PS2 that file lives in a save folder on the memory card, together
// with the icon the PS2 browser shows. Without a card at startup, use a writable data folder.
static const char kMcFolder[] = "ZELDA3-LTTP";
static char g_mc[48];     // "mc0:/ZELDA3-LTTP/" of the card that holds (or will hold) the save
static int g_mc_ready;    // save folder and icon files exist on the card

static int IsSavePath(const char *p) {
  return !strcmp(p, "saves/sram.dat") || !strcmp(p, "saves/sram.bak") ||
         !strcmp(p, "saves/sram.a") || !strcmp(p, "saves/sram.b") ||
         !strcmp(p, "saves/zelda3.ini") || !strcmp(p, "saves/zelda3.alt.ini");
}

static const char *SaveFileName(const char *p) {
  return strrchr(p, '/') + 1;
}

static FILE *McOpenRead(const char *name) {
  static const char *const kSlots[2] = { "mc0:", "mc1:" };
  char path[96];
  if (g_mc[0]) {
    snprintf(path, sizeof(path), "%s%s", g_mc, name);
    return fopen(path, "rb");
  }
  for (int i = 0; i < 2; i++) {
    snprintf(path, sizeof(path), "%s/%s/%s", kSlots[i], kMcFolder, name);
    FILE *f = fopen(path, "rb");
    if (f) {
      snprintf(g_mc, sizeof(g_mc), "%s/%s/", kSlots[i], kMcFolder);
      return f;
    }
  }
  return NULL;
}

// Writes an icon file to the card. A file with the same name next to the game replaces the built-in one.
static int CopyIconFile(const char *dest, const char *override_name, const unsigned char *data, size_t size) {
  FILE *existing = fopen(dest, "rb");
  if (existing) { fclose(existing); return 1; }
  unsigned char *buf = NULL;
  size_t n = 0;
  FILE *src = ps2_fopen(override_name, "rb");
  if (src) {
    fseek(src, 0, SEEK_END);
    long len = ftell(src);
    fseek(src, 0, SEEK_SET);
    if (len > 0 && len < 200000 && (buf = (unsigned char *)malloc((size_t)len)) != NULL) {
      n = (size_t)len;
      if (fread(buf, 1, n, src) != n) {
        free(buf);
        buf = NULL;
      }
    }
    fclose(src);
  }
  FILE *f = fopen(dest, "wb");
  if (!f) {
    free(buf);
    return 0;
  }
  int ok = buf ? fwrite(buf, 1, n, f) == n : fwrite(data, 1, size, f) == size;
  if (fflush(f) != 0) ok = 0;
  if (fclose(f) != 0) ok = 0;
  free(buf);
  return ok;
}

static int McPrepare(void) {
  static const char *const kSlots[2] = { "mc0:", "mc1:" };
  char path[96];
  if (g_mc_ready)
    return 1;
  for (int i = 0; i < 2; i++) {
    if (g_mc[0] && strncmp(g_mc, kSlots[i], 4))
      continue;  // the save is known to be on the other card
    snprintf(path, sizeof(path), "%s/%s", kSlots[i], kMcFolder);
    mkdir(path, 0777);  // fails harmlessly if it already exists
    snprintf(path, sizeof(path), "%s/%s/icon.sys", kSlots[i], kMcFolder);
    if (!CopyIconFile(path, "icon.sys", kSaveIconSys, (size_t)(kSaveIconSysEnd - kSaveIconSys)))
      continue;  // no card in this slot
    snprintf(path, sizeof(path), "%s/%s/zelda3.icn", kSlots[i], kMcFolder);
    if (!CopyIconFile(path, "zelda3.icn", kSaveIcon, (size_t)(kSaveIconEnd - kSaveIcon)))
      continue;
    snprintf(g_mc, sizeof(g_mc), "%s/%s/", kSlots[i], kMcFolder);
    g_mc_ready = 1;
    return 1;
  }
  return 0;
}

// Select once per session: game progress takes precedence over preferences.
// Never silently move a save if the selected card is later removed.
void ps2_prepare_save_dir(void) {
  static const char *const names[] = {
    "sram.a", "sram.b", "sram.dat", "zelda3.ini", "zelda3.alt.ini"
  };
  for (unsigned i = 0; i < sizeof(names) / sizeof(names[0]); ++i) {
    FILE *f = McOpenRead(names[i]);
    if (f) { fclose(f); break; }
  }
  if (McPrepare() || g_mc[0]) return;
  char path[300];
  snprintf(path, sizeof(path), "%ssaves", g_dir);
  mkdir(path, 0777);
}

const char *ps2_save_location(void) {
  return g_mc[0] ? g_mc : "SAVES/ - SEM MEMORY CARD";
}

FILE *ps2_fopen(const char *path, const char *mode) {
  char full[512];
  if (IsSavePath(path) && g_mc[0]) {
    snprintf(full, sizeof(full), "%s%s", g_mc, SaveFileName(path));
    return fopen(full, mode);
  }
  if (g_dir[0] && IsRelative(path)) {
    if (mode[0] == 'r')
      return OpenIn(g_dir, path, mode, g_style, NULL);
    snprintf(full, sizeof(full), "%s%s", g_dir, path);
    return fopen(full, mode);
  }
  return fopen(path, mode);
}

int ps2_mkdir(const char *path, int mode) {
  char full[512];
  if (g_dir[0] && IsRelative(path)) {
    snprintf(full, sizeof(full), "%s%s", g_dir, path);
    path = full;
  }
  return mkdir(path, mode);
}

int ps2_rename(const char *from, const char *to) {
  char a[512], b[512];
  if ((IsSavePath(from) || IsSavePath(to)) && g_mc[0]) {
    errno = ENOSYS;  // memory-card writes use alternating validated banks instead
    return -1;
  }
  if (g_dir[0] && IsRelative(from)) {
    snprintf(a, sizeof(a), "%s%s", g_dir, from);
    from = a;
  }
  if (g_dir[0] && IsRelative(to)) {
    snprintf(b, sizeof(b), "%s%s", g_dir, to);
    to = b;
  }
  return rename(from, to);
}
#endif
