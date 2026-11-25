#ifdef _EE
#include "ps2_menu.h"
#include "ps2_video.h"
#include "src/ps2_compat.h"
#include <SDL_timer.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

/* Pause menu: a centred black panel with a green frame, a white inner line and the
 * title cut into the top border. The root page has CONTINUAR JOGO and SAIR DO JOGO;
 * every option lives in a sub-menu (VIDEO, AUDIO, CONTROLES, SISTEMA). */

enum { KIND_SUBMENU, KIND_ACTION, KIND_TOGGLE, KIND_ASPECT, KIND_PERCENT, KIND_NUMBER, KIND_ONOFF, KIND_VMODE };
enum { ACT_RESUME, ACT_EXIT, ACT_SAVE, ACT_DEFAULTS, ACT_DISCARD, ACT_BACK };
enum { PAGE_MAIN, PAGE_VIDEO, PAGE_AUDIO, PAGE_CONTROLS, PAGE_SYSTEM, PAGE_EXIT, PAGE_COUNT };

typedef struct Entry {
  const char *label;
  int kind, arg;
  size_t offset;
  int step, min, max;
  const char *help;
} Entry;
typedef struct Page {
  const char *title;
  const Entry *entries;
  unsigned count;
} Page;

#define SUB(label, page, help) {label, KIND_SUBMENU, page, 0, 0, 0, 0, help}
#define ACT(label, act, help) {label, KIND_ACTION, act, 0, 0, 0, 0, help}
#define SET(label, kind, field, step, min, max, help) \
  {label, kind, 0, offsetof(Ps2Settings, field), step, min, max, help}
#define PAGE(title, entries) {title, entries, (unsigned)(sizeof(entries) / sizeof((entries)[0]))}

static const Entry main_entries[] = {
  ACT("CONTINUAR JOGO", ACT_RESUME, "APLICA E SALVA AS OPCOES"),
  SUB("VIDEO", PAGE_VIDEO, "MODO, TELA E CORES"),
  SUB("AUDIO", PAGE_AUDIO, "VOLUME E SOM"),
  SUB("CONTROLES", PAGE_CONTROLS, "BOTOES E ANALOGICO"),
  SUB("SISTEMA", PAGE_SYSTEM, "SALVAR E RESTAURAR"),
  SUB("SAIR DO JOGO", PAGE_EXIT, "FECHA O JOGO"),
};
static const Entry video_entries[] = {
  SET("MODO DE VIDEO", KIND_VMODE, video_mode, 1, 0, 2, "480I PADRAO - 480P PEDE CONFIRMACAO"),
  SET("MOSTRAR FPS", KIND_TOGGLE, show_fps, 1, 0, 1, "CONTADOR NO CANTO DA TELA"),
  SET("FORMATO DA TELA", KIND_ASPECT, aspect, 1, 0, 1, "TELA 4:3 OU PIXEL 1:1"),
  SET("FILTRO SUAVE", KIND_TOGGLE, filtering, 1, 0, 1, "USA COR RGBA E FILTRAGEM GS"),
  SET("SATURACAO", KIND_PERCENT, saturation, 10, 0, 200, "INTENSIDADE DAS CORES"),
  SET("REDUZIR CLAROES", KIND_TOGGLE, dim_flashes, 1, 0, 1, "SUAVIZA FLASHES FORTES"),
};
static const Entry audio_entries[] = {
  SET("VOLUME", KIND_PERCENT, volume, 5, 0, 100, "VOLUME GERAL"),
  SET("SOM ESTEREO", KIND_TOGGLE, stereo, 1, 0, 1, "ESTEREO OU MONO"),
  SET("ALERTA DE VIDA", KIND_TOGGLE, low_health_beep, 1, 0, 1, "BIPE COM POUCA VIDA"),
};
static const Entry controls_entries[] = {
  SET("ANALOGICO", KIND_TOGGLE, analog, 1, 0, 1, "USA O ANALOGICO ESQUERDO"),
  SET("ZONA MORTA", KIND_NUMBER, deadzone, 4, 12, 64, "SENSIBILIDADE DO ANALOGICO"),
  SET("TROCAR X / CIRCULO", KIND_TOGGLE, swap_ab, 1, 0, 1, "PADRAO: CIRCULO ACAO / X ESPADA"),
  SET("TROCAR QUAD / TRI", KIND_TOGGLE, swap_xy, 1, 0, 1, "PADRAO: QUADRADO ITEM / TRIANGULO MAPA"),
  SET("ITENS EM L1 / R1", KIND_TOGGLE, switch_lr, 1, 0, 1, "TROCA DE ITEM COM L1 E R1"),
};
static const Entry system_entries[] = {
  ACT("SALVAR AGORA", ACT_SAVE, "GRAVA AS OPCOES NO CARTAO"),
  ACT("RESTAURAR PADRAO", ACT_DEFAULTS, "VOLTA AO PADRAO - SALVE PARA MANTER"),
  ACT("DESCARTAR ALTERACOES", ACT_DISCARD, "FECHA O MENU SEM APLICAR"),
};
static const Entry exit_entries[] = {
  ACT("VOLTAR", ACT_BACK, "CONTINUA NO MENU"),
  ACT("SAIR DO JOGO", ACT_EXIT, "PROGRESSO NAO SALVO SERA PERDIDO"),
};
static const Page pages[PAGE_COUNT] = {
  PAGE("MENU", main_entries),        PAGE("VIDEO", video_entries),
  PAGE("AUDIO", audio_entries),      PAGE("CONTROLES", controls_entries),
  PAGE("SISTEMA", system_entries),   PAGE("SAIR DO JOGO?", exit_entries),
};
#undef SUB
#undef ACT
#undef SET
#undef PAGE

/* Panel geometry on the 640x448 canvas: about 85% of the screen, centred. */
enum { PX = 48, PY = 40, PW = 544, PH = 368, ROW = 34 };
#define C_GREEN 0x28c040
#define C_WHITE 0xf8f8f8
#define C_BLACK 0x000000
#define C_BAR 0x0c3a18
#define C_TEXT 0xc4d0c4
#define C_DIM 0x6f8f74
#define C_VALUE 0x7cf08c

static bool g_exit_requested;
bool Ps2MenuExitRequested(void) { return g_exit_requested; }

static int *Field(Ps2Settings *s, const Entry *e) { return (int *)((char *)s + e->offset); }

static const char *Value(const Entry *e, int v, char *buf, size_t size) {
  switch (e->kind) {
  case KIND_TOGGLE: return v ? "SIM" : "NAO";
  case KIND_ONOFF: return v ? "ON" : "OFF";
  case KIND_ASPECT: return v ? "PIXEL 1:1" : "TELA 4:3";
  case KIND_VMODE: return v == 2 ? "480P" : v == 1 ? "240P" : "480I";
  case KIND_PERCENT: snprintf(buf, size, "%d%%", v); return buf;
  case KIND_NUMBER: snprintf(buf, size, "%d", v); return buf;
  default: return "";
  }
}

static int TextWidth(const char *s, int scale) { return (int)strlen(s) * 6 * scale; }

static void Cursor(int x, int y) {
  static const int widths[7] = {2, 4, 6, 8, 6, 4, 2};
  for (int i = 0; i < 7; i++) Ps2VideoRect(x, y + i * 2, widths[i], 2, C_GREEN);
}

static void DrawFrame(const char *title) {
  Ps2VideoRect(PX + 2, PY, PW - 4, PH, C_GREEN);              /* green frame, notched corners */
  Ps2VideoRect(PX, PY + 2, PW, PH - 4, C_GREEN);
  Ps2VideoRect(PX + 8, PY + 6, PW - 16, PH - 12, C_WHITE);    /* white inner line */
  Ps2VideoRect(PX + 6, PY + 8, PW - 12, PH - 16, C_WHITE);
  Ps2VideoRect(PX + 8, PY + 8, PW - 16, PH - 16, C_BLACK);    /* panel */
  Ps2VideoRect(PX + 2, PY + 2, 2, 2, C_WHITE);                /* corner studs */
  Ps2VideoRect(PX + PW - 4, PY + 2, 2, 2, C_WHITE);
  Ps2VideoRect(PX + 2, PY + PH - 4, 2, 2, C_WHITE);
  Ps2VideoRect(PX + PW - 4, PY + PH - 4, 2, 2, C_WHITE);
  Ps2VideoRect(PX + 16, PY, TextWidth(title, 2) + 12, 14, C_BLACK); /* title cut into the border */
  Ps2VideoText(PX + 22, PY, title, 2, C_WHITE);
}

static bool Commit(Ps2PadState *pad, const Ps2Settings *draft) {
  (void)Ps2SettingsSave(draft); /* a missing memory card must never trap the player in the menu */
  g_ps2_settings = *draft;
  pad->blocked = true;
  return true;
}

bool Ps2MenuRun(Ps2PadState *pad) {
  Ps2Settings draft = g_ps2_settings;
  unsigned page = PAGE_MAIN, selected[PAGE_COUNT] = {0};
  uint32_t next_repeat = 0;
  uint16_t last_direction = 0;
  const char *status = NULL;
  bool release_gate = true;
  g_exit_requested = false;
  for (;;) {
    Ps2PadSample raw = Ps2PadPoll();
    Ps2PadUpdate(pad, &raw, &draft);
    uint16_t held = pad->held;
    if (pad->axes & 16) held |= Z3P_UP;
    if (pad->axes & 32) held |= Z3P_DOWN;
    if (pad->axes & 64) held |= Z3P_LEFT;
    if (pad->axes & 128) held |= Z3P_RIGHT;
    uint16_t pressed = pad->pressed;
    if (release_gate) {
      if (!held && raw.connected) release_gate = false;
      pressed = 0;
      held = 0;
    }
    uint16_t direction = held & (Z3P_UP | Z3P_DOWN | Z3P_LEFT | Z3P_RIGHT);
    uint32_t now = SDL_GetTicks();
    bool repeat = direction && (direction != last_direction || (int32_t)(now - next_repeat) >= 0);
    if (repeat) next_repeat = now + (direction == last_direction ? 100 : 350);
    last_direction = direction;

    const Page *pg = &pages[page];
    unsigned *sel = &selected[page];
    if (repeat && (direction & Z3P_UP)) *sel = (*sel + pg->count - 1) % pg->count;
    else if (repeat && (direction & Z3P_DOWN)) *sel = (*sel + 1) % pg->count;
    const Entry *e = &pg->entries[*sel];
    bool activate = pressed & Z3P_CROSS;

    if (pressed & Z3P_START) return Commit(pad, &draft);
    if (pressed & Z3P_CIRCLE) {
      if (page == PAGE_MAIN) return Commit(pad, &draft);
      page = PAGE_MAIN;
    } else if (activate && e->kind == KIND_SUBMENU) {
      page = (unsigned)e->arg;
      if (page == PAGE_EXIT) selected[PAGE_EXIT] = 0; /* never default to "sair" */
    } else if (activate && e->kind == KIND_ACTION) {
      switch (e->arg) {
      case ACT_RESUME: return Commit(pad, &draft);
      case ACT_EXIT: g_exit_requested = true; pad->blocked = true; return false;
      case ACT_DISCARD: pad->blocked = true; return false;
      case ACT_BACK: page = PAGE_MAIN; break;
      case ACT_SAVE:
        status = Ps2SettingsSave(&draft) ? "OPCOES SALVAS" : "FALHA AO SALVAR - CONFIRA O CARTAO";
        break;
      case ACT_DEFAULTS:
        Ps2SettingsDefaults(&draft);
        status = "PADRAO RESTAURADO - SALVE PARA MANTER";
        break;
      }
    } else if (e->step && (activate || (repeat && (direction & (Z3P_LEFT | Z3P_RIGHT))))) {
      int *v = Field(&draft, e);
      if (e->max == 1) *v = !*v;
      else {
        *v += !activate && (direction & Z3P_LEFT) ? -e->step : e->step;
        if (*v < e->min) *v = e->min;
        if (*v > e->max) *v = e->max;
      }
      status = "ALTERACOES APLICAM AO CONTINUAR";
    }

    pg = &pages[page];
    sel = &selected[page];
    e = &pg->entries[*sel];
    Ps2VideoMenuBegin();
    DrawFrame(pg->title);
    char buf[24];
    int row = pg->count > 8 ? 26 : pg->count > 7 ? 30 : ROW; /* long pages (VIDEO) use tighter rows */
    for (unsigned i = 0; i < pg->count; i++) {
      int y = PY + 34 + (int)i * row;
      bool on = i == *sel;
      if (on) {
        Ps2VideoRect(PX + 16, y - 8, PW - 32, row - 4, C_BAR);
        Ps2VideoRect(PX + 16, y - 8, 4, row - 4, C_GREEN);
        Cursor(PX + 28, y);
      }
      Ps2VideoText(PX + 52, y, pg->entries[i].label, 2, on ? C_WHITE : C_TEXT);
      if (pg->entries[i].kind == KIND_SUBMENU) {
        Ps2VideoText(PX + PW - 42, y, ">", 2, on ? C_GREEN : C_DIM);
      } else if (pg->entries[i].step) {
        const char *text = Value(&pg->entries[i], *Field(&draft, &pg->entries[i]), buf, sizeof buf);
        Ps2VideoText(PX + PW - 30 - TextWidth(text, 2), y, text, 2, C_VALUE);
      }
    }
    Ps2VideoRect(PX + 16, PY + PH - 86, PW - 32, 2, C_GREEN);
    const char *help = raw.connected ? e->help : "CONECTE UM CONTROLE NA PORTA 1 OU 2";
    Ps2VideoText(PX + 24, PY + PH - 74, help, 2, C_TEXT);
    if (status) {
      Ps2VideoText(PX + 24, PY + PH - 52, status, 2, C_VALUE);
    } else if (page == PAGE_SYSTEM) {
      char line[64];
      snprintf(line, sizeof line, "SAVE: %s", ps2_save_location());
      for (char *p = line; *p; p++) if (*p >= 'a' && *p <= 'z') *p -= 32;
      Ps2VideoText(PX + 24, PY + PH - 52, line, 2, C_DIM);
    }
    Ps2VideoText(PX + 24, PY + PH - 30, page == PAGE_MAIN ? "X: ESCOLHER   O: CONTINUAR" : "X: ESCOLHER   O: VOLTAR", 2, C_DIM);
    Ps2VideoMenuEnd();
  }
}
#endif
