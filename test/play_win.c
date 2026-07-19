/* play_win.c — chạy UI THẬT của LeafMuncher+ trong cửa sổ Windows (không cần bo).
 *
 * Khác play_host.c (console ASCII, thay cả tầng render), bản này chỉ thay MỖI tầng
 * driver gfx: hiện thực lại 8 hàm của gfx.h trên một buffer RGB565 landscape 320×240
 * rồi phóng to lên cửa sổ Win32 GDI. Nhờ vậy render.c + font8x16.c chạy NGUYÊN BẢN
 * → sprite 16×16, sâu có mắt, menu hộp viền, overlay PAUSED mờ... hiện đúng từng pixel
 * như thiết kế cho màn ILI9341.
 *
 *   firmware :  game.c → render.c → gfx.c (SDRAM/LTDC/DMA2D) → ILI9341
 *   bản này  :  game.c → render.c → play_win.c (buffer + GDI)  → cửa sổ Windows
 *
 * Build:  test\build_play.bat        (cmd/PowerShell — hoặc double-click)
 *         ./test/build_play.sh       (Git Bash)
 *         Script tự dò gcc (MinGW) và tự thêm Core/Src/theme.c nếu nhánh còn hệ theme.
 * Chạy :  ./play_win            (mũi tên/WASD lái, Enter/Space chọn, ESC/Q thoát)
 *         ./play_win --demo     (render 2 khung MENU/PLAYING ra file .raw — smoke test)
 *
 * Save/Continue dùng CHUNG host_save.bin với play_host (cùng layout sector 4 + CRC).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#include "game.h"
#include "store.h"
#include "levels.h"
#include "rng.h"
#include "render.h"
#include "gfx.h"
#include "font8x16.h"

#ifdef _WIN32
#include <windows.h>
#else
#error "play_win.c chi ho tro Windows (Win32 GDI)."
#endif

/* ══════════ 1. Backend gfx giả — hiện thực 8 hàm của gfx.h trên RAM PC ══════════
 * Toạ độ LANDSCAPE 320×240 thẳng (PC không cần trò xoay 90° của panel portrait). */

#define WIN_SCALE 3                              /* 320×240 → cửa sổ 960×720 */

static uint16_t g_fb[SCREEN_H][SCREEN_W];        /* "framebuffer SDRAM" phiên bản PC */
static uint32_t g_pix[SCREEN_H * SCREEN_W];      /* bản ARGB8888 đưa cho GDI */
static HWND     g_hwnd = NULL;

uint16_t gfx_rgb565(uint8_t r, uint8_t g, uint8_t b)
{
  return (uint16_t)(((r & 0xF8u) << 8) | ((g & 0xFCu) << 3) | (b >> 3));
}

void gfx_init(void) { memset(g_fb, 0, sizeof g_fb); }

static inline void put_px(int x, int y, uint16_t c)
{
  if (x < 0 || x >= SCREEN_W || y < 0 || y >= SCREEN_H) return;
  g_fb[y][x] = c;
}

void gfx_clear(uint16_t color)
{
  for (int y = 0; y < SCREEN_H; y++)
    for (int x = 0; x < SCREEN_W; x++) g_fb[y][x] = color;
}

void gfx_fill_rect(int x, int y, int w, int h, uint16_t c)
{
  if (w <= 0 || h <= 0) return;                  /* clip — y hệt gfx.c */
  if (x < 0) { w += x; x = 0; }
  if (y < 0) { h += y; y = 0; }
  if (x >= SCREEN_W || y >= SCREEN_H) return;
  if (x + w > SCREEN_W) w = SCREEN_W - x;
  if (y + h > SCREEN_H) h = SCREEN_H - y;
  if (w <= 0 || h <= 0) return;
  for (int j = 0; j < h; j++)
    for (int i = 0; i < w; i++) g_fb[y + j][x + i] = c;
}

void gfx_blit(const uint16_t *src, int x, int y, int w, int h)
{
  if (!src || w <= 0 || h <= 0) return;
  for (int i = 0; i < h; i++)
    for (int j = 0; j < w; j++) put_px(x + j, y + i, src[i * w + j]);
}

/* Trộn alpha 2 màu RGB565 — copy nguyên logic gfx.c (blend565). */
static uint16_t blend565(uint16_t bg, uint16_t fg, uint8_t a)
{
  uint32_t ia = 255u - a;
  uint32_t br = (bg >> 11) & 0x1Fu, bg6 = (bg >> 5) & 0x3Fu, bb = bg & 0x1Fu;
  uint32_t fr = (fg >> 11) & 0x1Fu, fg6 = (fg >> 5) & 0x3Fu, fb = fg & 0x1Fu;
  uint32_t r = (fr * a + br * ia) / 255u;
  uint32_t g = (fg6 * a + bg6 * ia) / 255u;
  uint32_t b = (fb * a + bb * ia) / 255u;
  return (uint16_t)((r << 11) | (g << 5) | b);
}

void gfx_blend_rect(int x, int y, int w, int h, uint16_t c, uint8_t a)
{
  if (w <= 0 || h <= 0) return;
  if (a == 255u) { gfx_fill_rect(x, y, w, h, c); return; }
  for (int i = 0; i < h; i++) {
    int py = y + i;
    if (py < 0 || py >= SCREEN_H) continue;
    for (int j = 0; j < w; j++) {
      int px = x + j;
      if (px < 0 || px >= SCREEN_W) continue;
      g_fb[py][px] = blend565(g_fb[py][px], c, a);
    }
  }
}

/* Vẽ chuỗi font 8×16 — copy nguyên logic gfx_text của gfx.c (gộp hoa, scale nguyên lần). */
void gfx_text(int x, int y, const char *s, uint16_t fg, uint16_t bg, int scale)
{
  if (!s) return;
  if (scale < 1) scale = 1;
  for (int cx = x; *s; s++, cx += FONT_W * scale) {
    unsigned char ch = (unsigned char)*s;
    if (ch >= 'a' && ch <= 'z') ch = (unsigned char)(ch - 'a' + 'A');
    if (ch < 0x20u || ch > 0x7Fu) ch = 0x20u;
    const uint8_t *glyph = FONT8X16[ch - 0x20u];
    for (int r = 0; r < FONT_H; r++) {
      uint8_t row = glyph[r];
      for (int col = 0; col < FONT_W; col++) {
        uint16_t c = (row & (0x80u >> col)) ? fg : bg;
        for (int dy = 0; dy < scale; dy++)
          for (int dx = 0; dx < scale; dx++)
            put_px(cx + col * scale + dx, y + r * scale + dy, c);
      }
    }
  }
}

static uint32_t argb_from_565(uint16_t c)        /* 565 → 8888 (nhân bản bit cao) */
{
  uint32_t r = (uint32_t)((c >> 11) & 0x1Fu);
  uint32_t g = (uint32_t)((c >> 5)  & 0x3Fu);
  uint32_t b = (uint32_t)( c        & 0x1Fu);
  r = (r << 3) | (r >> 2);
  g = (g << 2) | (g >> 4);
  b = (b << 3) | (b >> 2);
  return 0xFF000000u | (r << 16) | (g << 8) | b;
}

void gfx_present(void)                           /* "swap VSYNC" phiên bản PC */
{
  for (int y = 0; y < SCREEN_H; y++)
    for (int x = 0; x < SCREEN_W; x++)
      g_pix[y * SCREEN_W + x] = argb_from_565(g_fb[y][x]);
  if (g_hwnd) {
    InvalidateRect(g_hwnd, NULL, FALSE);
    UpdateWindow(g_hwnd);
  }
}

/* ══════════ 2. "Flash" giả = host_save.bin — dùng chung với play_host.c ══════════ */

static const char *SAVE_PATH = "host_save.bin";
static PersistData g_pd;
static SavedGame   g_slot[2];

static void host_slot_empty(SavedGame *s)
{
  memset(s, 0, sizeof *s);
  s->version = (uint16_t)SAVE_VERSION;
}

static void host_store_init(void)
{
  PersistData pd; SavedGame s0, s1;
  FILE *f = fopen(SAVE_PATH, "rb");
  bool ok_pd = (f != NULL) && fread(&pd, sizeof pd, 1, f) == 1;
  bool ok_s0 = ok_pd && fread(&s0, sizeof s0, 1, f) == 1;
  bool ok_s1 = ok_s0 && fread(&s1, sizeof s1, 1, f) == 1;
  if (f) fclose(f);
  if (ok_pd && store_pd_valid(&pd)) g_pd = pd; else store_pd_defaults(&g_pd);
  if (ok_s0 && store_sg_valid(&s0)) g_slot[0] = s0; else host_slot_empty(&g_slot[0]);
  if (ok_s1 && store_sg_valid(&s1)) g_slot[1] = s1; else host_slot_empty(&g_slot[1]);
}

static bool host_store_commit(void)
{
  g_pd.magic = STORE_MAGIC;
  g_pd.version = STORE_VERSION;
  g_pd.crc = store_crc32(&g_pd, (uint32_t)(sizeof(PersistData) - sizeof(g_pd.crc)));
  FILE *f = fopen(SAVE_PATH, "wb");
  if (!f) return false;
  bool ok = fwrite(&g_pd, sizeof g_pd, 1, f) == 1 &&
            fwrite(&g_slot[0], sizeof g_slot[0], 1, f) == 1 &&
            fwrite(&g_slot[1], sizeof g_slot[1], 1, f) == 1;
  fclose(f);
  return ok;
}

static bool host_has_save(PlayMode m) { return ((unsigned)m <= 1u) && g_slot[m].valid == 1u; }

static void host_save_game(PlayMode m, const GameState *s)
{
  if ((unsigned)m > 1u) return;
  memset(&g_slot[m], 0, sizeof g_slot[m]);
  g_slot[m].valid = 1u;
  g_slot[m].version = (uint16_t)SAVE_VERSION;
  g_slot[m].state = *s;
  g_slot[m].crc = store_crc32(&g_slot[m], (uint32_t)(sizeof(SavedGame) - sizeof(g_slot[m].crc)));
  (void)host_store_commit();
}

static bool host_load_game(PlayMode m, GameState *out)
{
  if (!host_has_save(m)) return false;
  *out = g_slot[m].state;
  return true;
}

static void host_clear_save(PlayMode m)
{
  if (!host_has_save(m)) return;
  g_slot[m].valid = 0u;
  g_slot[m].crc = store_crc32(&g_slot[m], (uint32_t)(sizeof(SavedGame) - sizeof(g_slot[m].crc)));
  (void)host_store_commit();
}

/* ══════════ 3. Cửa sổ Win32 + bàn phím ══════════ */

static InputEvent g_keys[64];                    /* phím dồn giữa 2 vòng lặp, giữ thứ tự */
static int g_kn = 0;
static int g_quit = 0;

static void push_key(InputKind kind, Dir d)
{
  if (g_kn < 64) { g_keys[g_kn].kind = kind; g_keys[g_kn].dir = d; g_kn++; }
}

static LRESULT CALLBACK wndproc(HWND h, UINT msg, WPARAM wp, LPARAM lp)
{
  switch (msg) {
    case WM_KEYDOWN:
      switch (wp) {
        case VK_UP:    case 'W': push_key(IN_DIR, DIR_UP);    break;
        case VK_DOWN:  case 'S': push_key(IN_DIR, DIR_DOWN);  break;
        case VK_LEFT:  case 'A': push_key(IN_DIR, DIR_LEFT);  break;
        case VK_RIGHT: case 'D': push_key(IN_DIR, DIR_RIGHT); break;
        case VK_RETURN: case VK_SPACE:
          if (!(lp & 0x40000000)) push_key(IN_SELECT, DIR_UP);   /* bỏ auto-repeat khi giữ */
          break;
        case VK_ESCAPE: case 'Q': g_quit = 1; break;
        default: break;
      }
      return 0;
    case WM_PAINT: {
      PAINTSTRUCT ps;
      HDC dc = BeginPaint(h, &ps);
      BITMAPINFO bmi;
      memset(&bmi, 0, sizeof bmi);
      bmi.bmiHeader.biSize = sizeof bmi.bmiHeader;
      bmi.bmiHeader.biWidth = SCREEN_W;
      bmi.bmiHeader.biHeight = -SCREEN_H;        /* âm = top-down */
      bmi.bmiHeader.biPlanes = 1;
      bmi.bmiHeader.biBitCount = 32;
      bmi.bmiHeader.biCompression = BI_RGB;
      StretchDIBits(dc, 0, 0, SCREEN_W * WIN_SCALE, SCREEN_H * WIN_SCALE,
                    0, 0, SCREEN_W, SCREEN_H, g_pix, &bmi, DIB_RGB_COLORS, SRCCOPY);
      EndPaint(h, &ps);
      return 0;
    }
    case WM_CLOSE:   DestroyWindow(h); return 0;
    case WM_DESTROY: g_quit = 1; PostQuitMessage(0); return 0;
    default:         return DefWindowProc(h, msg, wp, lp);
  }
}

static void win_create(void)
{
  WNDCLASSA wc;
  memset(&wc, 0, sizeof wc);
  wc.lpfnWndProc = wndproc;
  wc.hInstance = GetModuleHandle(NULL);
  wc.hCursor = LoadCursor(NULL, IDC_ARROW);
  wc.lpszClassName = "LeafMuncherWin";
  RegisterClassA(&wc);

  DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
  RECT rc = { 0, 0, SCREEN_W * WIN_SCALE, SCREEN_H * WIN_SCALE };
  AdjustWindowRect(&rc, style, FALSE);
  g_hwnd = CreateWindowA("LeafMuncherWin",
                         "LeafMuncher+ (PC — UI that, logic firmware nguyen ban)",
                         style, CW_USEDEFAULT, CW_USEDEFAULT,
                         rc.right - rc.left, rc.bottom - rc.top,
                         NULL, NULL, wc.hInstance, NULL);
  ShowWindow(g_hwnd, SW_SHOW);
}

/* ══════════ 4. Vòng đời — mirror GameTask (đã vá theo review play_host) ══════════ */

static void sync_save_flags_host(GameState *gs)
{
  gs->has_save[MODE_LEVEL]   = host_has_save(MODE_LEVEL)   ? 1u : 0u;
  gs->has_save[MODE_ENDLESS] = host_has_save(MODE_ENDLESS) ? 1u : 0u;
}

static uint32_t seed_now(void)
{
  return (uint32_t)time(NULL) ^ (uint32_t)GetTickCount64();
}

static void orchestrate(GameState *gs, GameMode mode_before)
{
  if (gs->load_request) {
    gs->load_request = 0u;
    PlayMode m = (PlayMode)gs->play_mode;
    if (host_has_save(m)) {
      (void)host_load_game(m, gs);
      gs->from_save = 1u;
    }
    sync_save_flags_host(gs);
  } else if (gs->save_request) {
    gs->save_request = 0u;
    host_save_game((PlayMode)gs->play_mode, gs);
    gs->mode = ST_MENU;
    gs->menu_sel = 0u;
    sync_save_flags_host(gs);
  } else if (mode_before == ST_MENU && gs->mode == ST_PLAYING) {
    gs->from_save = 0u;
  }

  if (mode_before != ST_GAME_OVER && mode_before != ST_WIN &&
      (gs->mode == ST_GAME_OVER || gs->mode == ST_WIN)) {
    bool need_commit = false;
    if (gs->play_mode == MODE_ENDLESS && gs->score > g_pd.endless_high) {
      g_pd.endless_high = gs->score;
      render_set_endless_best(g_pd.endless_high);          /* HUD "BEST" — như firmware */
      need_commit = true;
    }
    if (gs->from_save && host_has_save(gs->play_mode)) {
      host_clear_save(gs->play_mode);
    } else if (need_commit) {
      (void)host_store_commit();
    }
    gs->from_save = 0u;
    sync_save_flags_host(gs);
  }
}

/* --demo: render 2 khung (MENU + PLAYING) ra .raw để kiểm tra pipeline không cần cửa sổ. */
static void dump_raw(const char *path)
{
  FILE *f = fopen(path, "wb");
  if (!f) return;
  for (int y = 0; y < SCREEN_H; y++)
    for (int x = 0; x < SCREEN_W; x++) {
      uint32_t p = argb_from_565(g_fb[y][x]);
      uint8_t rgb[3] = { (uint8_t)(p >> 16), (uint8_t)(p >> 8), (uint8_t)p };
      fwrite(rgb, 1, 3, f);
    }
  fclose(f);
}

static int run_demo(void)
{
  gfx_init();
  GameState gs;
  game_init(&gs, 12345u);
  render_set_endless_best(0);

  render_force_full(&gs);                        /* khung MENU thật từ render.c */
  dump_raw("play_win_menu.raw");

  gs.play_mode = MODE_LEVEL;
  game_start(&gs);
  static const Dir script[4] = { DIR_RIGHT, DIR_DOWN, DIR_LEFT, DIR_UP };
  for (int t = 0; t < 14 && gs.mode == ST_PLAYING; t++) {
    InputEvent in = { IN_DIR, script[(t / 5) % 4] };
    (void)game_step(&gs, in, game_step_ms(&gs));
  }
  render_force_full(&gs);                        /* khung PLAYING thật */
  dump_raw("play_win_play.raw");

  int colors = 0;                                /* đếm sơ số màu khác nhau — sanity */
  uint16_t seen[64]; int ns = 0;
  for (int y = 0; y < SCREEN_H; y += 7)
    for (int x = 0; x < SCREEN_W; x += 7) {
      uint16_t c = g_fb[y][x]; int dup = 0;
      for (int i = 0; i < ns; i++) if (seen[i] == c) { dup = 1; break; }
      if (!dup && ns < 64) seen[ns++] = c;
    }
  colors = ns;
  printf("play_win --demo: mode=%d score=%u; 2 frame .raw da ghi; ~%d mau khac nhau\n",
         (int)gs.mode, gs.score, colors);
  return (colors >= 4 && gs.mode == ST_PLAYING) ? 0 : 1;
}

static int app_main(void)
{
  win_create();
  gfx_init();
  host_store_init();

  GameState gs;
  game_init(&gs, seed_now());
  render_set_endless_best(g_pd.endless_high);
  sync_save_flags_host(&gs);

  InputEvent pend = { IN_NONE, DIR_UP };
  ULONGLONG next_tick = GetTickCount64();
  int dirty = 1;

  while (!g_quit) {
    GameMode mode_before = gs.mode;

    MSG msg;                                     /* bơm message → wndproc dồn phím vào g_keys */
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
      if (msg.message == WM_QUIT) g_quit = 1;
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
    if (g_quit) break;

    for (int i = 0; i < g_kn; i++) {             /* xử phím theo đúng thứ tự gõ */
      InputEvent ev = g_keys[i];
      if (gs.mode == ST_PLAYING) {
        pend = ev;                               /* latest wins — như input_q_latest */
      } else {
        if (ev.kind == IN_SELECT && gs.mode == ST_MENU)
          rng_seed(&gs.rng, seed_now());         /* T061 */
        game_input_ui(&gs, ev);
        dirty = 1;
      }
    }
    g_kn = 0;

    ULONGLONG now = GetTickCount64();
    if (mode_before == ST_PLAYING && gs.mode == ST_PLAYING && now >= next_tick) {
      InputEvent in = pend;
      pend.kind = IN_NONE;
      (void)game_step(&gs, in, game_step_ms(&gs));
      next_tick += game_step_ms(&gs);
      if (next_tick < now) next_tick = now;
      dirty = 1;
    }

    orchestrate(&gs, mode_before);

    if (mode_before != ST_PLAYING && gs.mode == ST_PLAYING) {
      next_tick = GetTickCount64() + game_step_ms(&gs);
      pend.kind = IN_NONE;
    }

    if (dirty) {                                 /* "RenderTask": render.c THẬT vẽ khung */
      render_frame(&gs, EV_MOVED);
      gfx_present();
      dirty = 0;
    }
    Sleep(4);
  }
  return 0;
}

int main(int argc, char **argv)
{
  if (argc > 1 && strcmp(argv[1], "--demo") == 0) return run_demo();
  return app_main();
}
