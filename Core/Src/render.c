#include "render.h"
#include "gfx.h"
#include "levels.h"

/* Bảng màu Forest (RGB565) */
#define RGB565(r, g, b) ((uint16_t)((((r) & 0xF8u) << 8) | (((g) & 0xFCu) << 3) | ((b) >> 3)))
#define CLR_BG        RGB565(11, 26, 11)     /* nền sân xanh thẫm   */
#define CLR_HUD_BG    RGB565(20, 20, 35)     /* nền dải HUD         */
#define CLR_TEXT      RGB565(235, 235, 235)  /* chữ HUD             */
#define CLR_WORM_HEAD RGB565(230, 126, 0)
#define CLR_WORM_BODY RGB565(255, 180, 0)
#define CLR_OBSTACLE  RGB565(95, 95, 110)    /* đá xám              */

/* Tô màu toàn bộ một ô trên sân (dưới dải HUD) */
static void cell_fill(int c, int r, uint16_t color)
{
  gfx_fill_rect(c * CELL, HUD_H + r * CELL, CELL, CELL, color);
}

/* Vẽ một khối màu nhỏ bên trong ô (c,r) với offset và kích thước cụ thể */
static void px(int c, int r, int dx, int dy, int w, int h, uint16_t color)
{
  gfx_fill_rect(c * CELL + dx, HUD_H + r * CELL + dy, w, h, color);
}

#define GLYPH_INK_TOP 1
#define GLYPH_INK_H   10
/* Tính tọa độ y để chữ được căn giữa theo chiều dọc */
static int text_cy(int box_y, int box_h, int scale)
{
  return box_y + (box_h - GLYPH_INK_H * scale) / 2 - GLYPH_INK_TOP * scale;
}

static int put_u32(char *dst, uint32_t v)
{
  char tmp[10];
  int n = 0;
  if (v == 0u) tmp[n++] = '0';
  while (v) { tmp[n++] = (char)('0' + (v % 10u)); v /= 10u; }
  for (int i = 0; i < n; i++) dst[i] = tmp[n - 1 - i];
  return n;
}

/* Lưu điểm cao nhất ở chế độ Vô Tận để hiển thị lên HUD */
static uint32_t s_endless_best = 0u;
void render_set_endless_best(uint32_t best) { s_endless_best = best; }

static void draw_hud(const GameState *gs)
{
  uint16_t bg = CLR_HUD_BG;
  gfx_fill_rect(0, 0, SCREEN_W, HUD_H, bg);

  char line[48];
  int p = 0;
  const char *s = "SCORE ";
  while (*s) line[p++] = *s++;
  p += put_u32(line + p, gs->score);
  line[p++] = ' '; line[p++] = ' ';
  if (gs->play_mode == MODE_ENDLESS) {
    const char *m = "ENDLESS  BEST ";
    while (*m) line[p++] = *m++;
    p += put_u32(line + p, s_endless_best);
  } else {
    line[p++] = 'L'; line[p++] = 'V'; line[p++] = ' ';
    p += put_u32(line + p, (uint32_t)(gs->level_idx + 1));
  }
  line[p] = 0;
  gfx_text(6, 8, line, CLR_TEXT, bg, 1);

  /* Hiển thị thời gian hiệu lực của các vật phẩm power-up ở góc phải */
  static const char PU_CH[PU_KINDS] = { 'S', 'W', 'G', 'P' };
  char pwr[20];
  int q = 0;
  for (int k = 0; k < PU_KINDS && q < 16; k++) {
    if (gs->power[k] > 0) {
      uint32_t sec = (uint32_t)((gs->power[k] + 999) / 1000);
      pwr[q++] = PU_CH[k];
      q += put_u32(pwr + q, sec);
      pwr[q++] = ' ';
    }
  }
  if (q > 0) {
    pwr[q - 1] = 0;
    gfx_text(SCREEN_W - q * 8, 8, pwr, gfx_rgb565(120, 220, 235), bg, 1);
  }
}

static uint16_t pu_color(PowerType t)
{
  switch (t) {
    case PU_SPEED: return gfx_rgb565(40, 200, 220);
    case PU_SLOW:  return gfx_rgb565(40, 90, 220);
    case PU_GHOST: return gfx_rgb565(235, 235, 235);
    case PU_PHASE: return gfx_rgb565(200, 90, 40);
    default:       return gfx_rgb565(40, 200, 220);
  }
}

/* ===== Dữ liệu hình ảnh (Sprite 16x16) ===== */

static uint16_t spr_color(char k)
{
  switch (k) {
    case 'o': return gfx_rgb565(18, 92, 34);     /* viền lá đậm   */
    case 'G': return gfx_rgb565(46, 204, 64);    /* lá xanh nền   */
    case 'L': return gfx_rgb565(130, 240, 130);  /* lá mặt sáng   */
    case 'V': return gfx_rgb565(28, 134, 52);    /* gân lá (đậm)  */
    case 'm': return gfx_rgb565(96, 64, 22);     /* cuống nâu     */
    case 'y': return gfx_rgb565(190, 150, 0);    /* viền vàng     */
    case 'Y': return gfx_rgb565(255, 215, 0);    /* vàng          */
    case 'W': return gfx_rgb565(255, 246, 178);  /* lóa sáng      */
    case 'p': return gfx_rgb565(92, 12, 112);    /* tím đậm       */
    case 'P': return gfx_rgb565(206, 138, 226);  /* xương tím     */
    default:  return 0;
  }
}

static void draw_sprite(int c, int r, const char *const rows[16])
{
  for (int y = 0; y < 16; y++) {
    const char *row = rows[y];
    int x = 0;
    while (x < 16) {
      char k = row[x];
      if (k == '.') { x++; continue; }
      int run = 1;
      while (x + run < 16 && row[x + run] == k) run++;
      px(c, r, x, y, run, 1, spr_color(k));
      x += run;
    }
  }
}

static const char *const SPR_LEAF[16] = {
  "................", "................", ".......oooooo...", ".....ooVLVLGo...",
  "....oLLVLVGVo...", "...oLVLLLVVGo...", "...oLVLVGVVVo...", "..oLLVLVVGGGo...",
  "..oLLLVVVVGo....", "..oLLGVVGGGo....", "..oLGVGGVVo.....", "..oGVGGGoo......",
  "..omoooo........", "..m.............", ".m..............", "................",
};
static const char *const SPR_GOLD[16] = {
  "................", "......yyy.......", "....yyYYYyy.....", "...yWWWYYYYy....",
  "..yWWWWYYYYYy...", "..yWWWWWYYYYy...", ".yYWWWWYYYYYYy..", ".yYYYWYYYYYYYy..",
  ".yYYYYYYYYYYYy..", "..yYYYYYYYYYy...", "..yYYYYYYYYYy...", "...yYYYYYYYy....",
  "....yyYYYyy.....", "......yyy.......", "................", "................",
};
static const char *const SPR_POISON[16] = {
  "................", ".....pppppp.....", "....pPPPPPPp....", "...pPPPPPPPPp...",
  "...pPPPPPPPPp...", "...pPppPPppPp...", "...pPppPPppPp...", "...pPPPPPPPPp...",
  "....pPPppPPp....", ".....pPPPPp.....", ".....pPppPp.....", ".....pPppPp.....",
  "......pppp......", "................", "................", "................",
};

static uint16_t shade565(uint16_t col, int num, int den)
{
  int r = (col >> 11) & 0x1F, g = (col >> 5) & 0x3F, b = col & 0x1F;
  r = r * num / den; g = g * num / den; b = b * num / den;
  if (r > 31) r = 31;
  if (g > 63) g = 63;
  if (b > 31) b = 31;
  return (uint16_t)((r << 11) | (g << 5) | b);
}

static void draw_obstacle(int c, int r, uint16_t base)
{
  static const uint8_t crack[6][2] = { {10,3},{10,4},{9,5},{9,6},{8,7},{8,8} };
  static const uint8_t grit[4][2]  = { {4,5},{6,11},{12,9},{5,9} };
  uint16_t hi = shade565(base, 135, 100), lo = shade565(base, 60, 100), dk = shade565(base, 45, 100);
  
  cell_fill(c, r, base);
  px(c, r, 0, 0, 16, 2, hi); px(c, r, 0, 0, 2, 16, hi);
  px(c, r, 0, 14, 16, 2, lo); px(c, r, 14, 0, 2, 16, lo);
  for (int i = 0; i < 6; i++) px(c, r, crack[i][0], crack[i][1], 1, 1, dk);
  for (int i = 0; i < 4; i++) px(c, r, grit[i][0], grit[i][1], 1, 1, dk);
}

static char pu_letter(PowerType t)
{
  switch (t) {
    case PU_SPEED: return 'S';
    case PU_SLOW:  return 'W';
    case PU_GHOST: return 'G';
    case PU_PHASE: return 'P';
    default:       return '?';
  }
}

static void draw_powerup_token(int c, int r, PowerType t, uint16_t bg)
{
  uint16_t col = pu_color(t);
  uint16_t hi = shade565(col, 130, 100), lo = shade565(col, 65, 100);
  px(c, r, 1, 1, 14, 14, col);
  px(c, r, 1, 1, 1, 1, bg); px(c, r, 14, 1, 1, 1, bg);
  px(c, r, 1, 14, 1, 1, bg); px(c, r, 14, 14, 1, 1, bg);
  px(c, r, 3, 2, 9, 2, hi); px(c, r, 3, 12, 9, 2, lo);
  
  char s[2] = { pu_letter(t), 0 };
  gfx_text(c * CELL + 4, HUD_H + r * CELL, s, gfx_rgb565(20, 20, 30), col, 1);
}

#define CONN_R 1u
#define CONN_L 2u
#define CONN_U 4u
#define CONN_D 8u

static void draw_worm_body(int c, int r, uint16_t col, uint16_t bg, unsigned conn, int is_head, Dir dir)
{
  uint16_t hi = shade565(col, 130, 100), lo = shade565(col, 62, 100);
  px(c, r, 1, 1, 14, 14, col);
  px(c, r, 1, 1, 1, 1, bg); px(c, r, 14, 1, 1, 1, bg);
  px(c, r, 1, 14, 1, 1, bg); px(c, r, 14, 14, 1, 1, bg);
  
  if (conn & CONN_R) px(c, r, 14, 4, 2, 8, col);
  if (conn & CONN_L) px(c, r, 0, 4, 2, 8, col);
  if (conn & CONN_U) px(c, r, 4, 0, 8, 2, col);
  if (conn & CONN_D) px(c, r, 4, 14, 8, 2, col);
  
  px(c, r, 3, 2, 9, 2, hi);
  px(c, r, 3, 12, 9, 2, lo);
  
  if (is_head) {
    uint16_t white = gfx_rgb565(245, 245, 245), pup = gfx_rgb565(15, 15, 15);
    int e1x, e1y, e2x, e2y;
    switch (dir) {
      case DIR_RIGHT: e1x = 10; e1y = 3;  e2x = 10; e2y = 10; break;
      case DIR_LEFT:  e1x = 3;  e1y = 3;  e2x = 3;  e2y = 10; break;
      case DIR_UP:    e1x = 3;  e1y = 3;  e2x = 10; e2y = 3;  break;
      default:        e1x = 3;  e1y = 10; e2x = 10; e2y = 10; break;
    }
    px(c, r, e1x, e1y, 3, 3, white); px(c, r, e2x, e2y, 3, 3, white);
    px(c, r, e1x + 1, e1y + 1, 1, 1, pup); px(c, r, e2x + 1, e2y + 1, 1, 1, pup);
  }
}

static unsigned conn_bit(int dc, int dr)
{
  if (dc == 1 && dr == 0) return CONN_R;
  if (dc == -1 && dr == 0) return CONN_L;
  if (dc == 0 && dr == -1) return CONN_U;
  if (dc == 0 && dr == 1) return CONN_D;
  return 0u;
}

static void draw_playing(const GameState *gs)
{
  gfx_fill_rect(0, HUD_H, SCREEN_W, SCREEN_H - HUD_H, CLR_BG);

  const Level *lv = level_get(gs->level_idx);
  if (lv != 0 && lv->obstacles != 0) {
    for (int r = 0; r < ROWS; r++)
      for (int c = 0; c < COLS; c++)
        if (lv->obstacles[r][c]) draw_obstacle(c, r, CLR_OBSTACLE);
  }

  if (gs->leaf_normal.type != LEAF_NONE) {
    draw_sprite(gs->leaf_normal.pos.c, gs->leaf_normal.pos.r, SPR_LEAF);
  }

  if (gs->leaf_poison.type == LEAF_POISON) {
    draw_sprite(gs->leaf_poison.pos.c, gs->leaf_poison.pos.r, SPR_POISON);
  }

  if (gs->leaf_gold.type == LEAF_GOLD) {
    int32_t life = gs->leaf_gold.life_ms;
    int blink_on = (life < 0) || (((life / 500) & 1) == 0);
    if (blink_on) {
      draw_sprite(gs->leaf_gold.pos.c, gs->leaf_gold.pos.r, SPR_GOLD);
    }
  }

  if (gs->leaf_pu.type == LEAF_POWERUP) {
    draw_powerup_token(gs->leaf_pu.pos.c, gs->leaf_pu.pos.r, gs->leaf_pu.pu_type, CLR_BG);
  }

  const Worm *w = &gs->worm;
  for (uint16_t k = 0; k < w->len; k++) {
    uint16_t i = (uint16_t)((w->head_idx + k) % WORM_CAP);
    Cell sg = w->body[i];
    unsigned conn = 0u;
    if (k > 0) {
      Cell pv = w->body[(w->head_idx + k - 1) % WORM_CAP];
      conn |= conn_bit(pv.c - sg.c, pv.r - sg.r);
    }
    if (k + 1 < w->len) {
      Cell nx = w->body[(w->head_idx + k + 1) % WORM_CAP];
      conn |= conn_bit(nx.c - sg.c, nx.r - sg.r);
    }
    if (k == 0) draw_worm_body(sg.c, sg.r, CLR_WORM_HEAD, CLR_BG, conn, 1, w->dir);
    else        draw_worm_body(sg.c, sg.r, CLR_WORM_BODY, CLR_BG, conn, 0, w->dir);
  }
}

static void draw_game_over(const GameState *gs)
{
  uint16_t bg = gfx_rgb565(28, 8, 8);
  uint16_t fg = gfx_rgb565(240, 240, 240);
  gfx_clear(bg);
  
  gfx_text((SCREEN_W - 9 * 16) / 2, 56, "GAME OVER", gfx_rgb565(255, 80, 80), bg, 2);

  char line[20];
  int p = 0;
  const char *s = "SCORE ";
  while (*s) line[p++] = *s++;
  p += put_u32(line + p, gs->score);
  line[p] = 0;
  gfx_text((SCREEN_W - p * 8) / 2, 118, line, fg, bg, 1);

  gfx_text((SCREEN_W - 12 * 8) / 2, 158, "PRESS BUTTON", gfx_rgb565(180, 180, 180), bg, 1);
}

static void draw_banner(const GameState *gs, const char *title, int title_len,
                        uint16_t title_col, uint16_t bg, const char *hint, int hint_len)
{
  uint16_t fg = gfx_rgb565(240, 240, 240);
  gfx_clear(bg);
  gfx_text((SCREEN_W - title_len * 16) / 2, 56, title, title_col, bg, 2);

  char line[20];
  int p = 0;
  const char *s = "SCORE ";
  while (*s) line[p++] = *s++;
  p += put_u32(line + p, gs->score);
  line[p] = 0;
  gfx_text((SCREEN_W - p * 8) / 2, 118, line, fg, bg, 1);

  gfx_text((SCREEN_W - hint_len * 8) / 2, 158, hint, gfx_rgb565(180, 180, 180), bg, 1);
}

static void draw_frame(int x, int y, int w, int h, uint16_t border, uint16_t fill)
{
  gfx_fill_rect(x, y, w, h, border);
  gfx_fill_rect(x + 2, y + 2, w - 4, h - 4, fill);
}

static void draw_menu_item(int cx, int y, int w, int h, const char *label, int len,
                           int sel, int locked)
{
  uint16_t border = sel ? gfx_rgb565(255, 235, 120) : gfx_rgb565(70, 72, 92);
  uint16_t fill, txt;
  
  if (locked) {
    fill = sel ? gfx_rgb565(44, 46, 64) : gfx_rgb565(18, 20, 34);
    txt  = gfx_rgb565(125, 127, 145);
  } else {
    fill = sel ? gfx_rgb565(30, 74, 30) : gfx_rgb565(20, 24, 40);
    txt  = sel ? gfx_rgb565(255, 255, 255) : gfx_rgb565(150, 150, 162);
  }
  
  draw_frame(cx - w / 2, y, w, h, border, fill);
  int ty = text_cy(y, h, 1);
  
  if (locked) {
    gfx_text(cx - w / 2 + 12, ty, label, txt, fill, 1);
    gfx_text(cx + w / 2 - 12 - 4 * 8, ty, "SOON", gfx_rgb565(170, 120, 55), fill, 1);
  } else {
    gfx_text(cx - len * 8 / 2, ty, label, txt, fill, 1);
  }
}

static int menu_label(MenuItemId id, char *buf)
{
  int n = 0;
  const char *s = (id == MI_CONTINUE_LEVEL)   ? "CONTINUE LV"   :
                  (id == MI_CONTINUE_ENDLESS) ? "CONTINUE ENDL" :
                  (id == MI_START)            ? "START"         : "ENDLESS";
  while (s[n] && n < 22) { buf[n] = s[n]; n++; }
  buf[n] = 0;
  return n;
}

static void draw_menu(const GameState *gs)
{
  uint16_t bg = gfx_rgb565(8, 10, 26);
  gfx_clear(bg);

  int tx = (SCREEN_W - 12 * 16) / 2;
  gfx_text(tx, 24, "LEAFMUNCHER", gfx_rgb565(120, 230, 120), bg, 2);
  gfx_text(tx + 11 * 16, 24, "+", gfx_rgb565(255, 215, 0), bg, 2);
  gfx_fill_rect(tx, 60, 12 * 16, 2, gfx_rgb565(60, 120, 60));

  MenuItemId items[MENU_MAX_ITEMS];
  int n = game_menu_items(gs, items);

  const int H = 26, S = 30, top = 72, bot = 232;
  int block = (n - 1) * S + H;
  int y0 = top + ((bot - top) - block) / 2;
  if (y0 < top) y0 = top;

  for (int i = 0; i < n; i++) {
    char buf[24];
    int len = menu_label(items[i], buf);
    draw_menu_item(SCREEN_W / 2, y0 + i * S, 200, H, buf, len, gs->menu_sel == (uint8_t)i, 0);
  }

  int hint_y = y0 + block + 8;
  if (hint_y <= 230) {
    gfx_text((SCREEN_W - 19 * 8) / 2, hint_y, "AXIS: MOVE  BTN: OK", gfx_rgb565(90, 92, 112), bg, 1);
  }
}

static void draw_paused(const GameState *gs)
{
  draw_hud(gs);
  draw_playing(gs);
  gfx_blend_rect(0, HUD_H, SCREEN_W, SCREEN_H - HUD_H, gfx_rgb565(0, 0, 0), 160);

  int bw = 216, bh = 158, bx = (SCREEN_W - bw) / 2, by = (SCREEN_H - bh) / 2;
  uint16_t border = gfx_rgb565(255, 235, 120);
  uint16_t fill   = gfx_rgb565(22, 26, 44);
  uint16_t barbg  = gfx_rgb565(40, 46, 74);
  
  draw_frame(bx, by, bw, bh, border, fill);
  gfx_fill_rect(bx + 2, by + 2, bw - 4, 40, barbg);
  gfx_text(SCREEN_W / 2 - 6 * 16 / 2, text_cy(by + 2, 40, 2), "PAUSED", border, barbg, 2);

  static const char *const L[3]  = { "RESUME", "SAVE & EXIT", "EXIT" };
  static const int         LN[3] = { 6, 11, 4 };
  int iy = by + 50, iw = 184, ih = 26, step = 30;
  
  for (int i = 0; i < 3; i++) {
    draw_menu_item(SCREEN_W / 2, iy + i * step, iw, ih, L[i], LN[i],
                   gs->menu_sel == (uint8_t)i, 0);
  }
}

static void draw_by_mode(const GameState *gs)
{
  switch (gs->mode) {
    case ST_PLAYING:
      draw_hud(gs);
      draw_playing(gs);
      break;
    case ST_PAUSED:
      draw_paused(gs);
      break;
    case ST_MENU:
      draw_menu(gs);
      break;
    case ST_GAME_OVER:
      draw_game_over(gs);
      break;
    case ST_LEVEL_COMPLETE:
      draw_banner(gs, "LEVEL CLEAR", 11, gfx_rgb565(120, 230, 120),
                  gfx_rgb565(8, 24, 8), "PRESS: NEXT", 11);
      break;
    case ST_WIN:
      draw_banner(gs, "YOU WIN!", 8, gfx_rgb565(255, 215, 0),
                  gfx_rgb565(8, 8, 28), "PRESS BUTTON", 12);
      break;
    default:
      draw_menu(gs);
      break;
  }
}

void render_force_full(const GameState *gs)
{
  draw_by_mode(gs);
}

void render_frame(const GameState *gs, GameEvents ev)
{
  (void)ev;
  draw_by_mode(gs);
}
