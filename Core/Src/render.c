#include "render.h"
#include "gfx.h"
#include "levels.h"   /* level_get → vẽ chướng ngại màn (T045) */

/* render — ánh xạ GameState → lệnh vẽ gfx (chỉ gọi gfx_*, KHÔNG chạm HAL — NT IV/V).
 * T024 (M2): vẽ HUD + sân + sâu, dispatch theo `mode`. M2 vẽ lại toàn khung mỗi tick
 * (chấp nhận được ở tốc snake, research §14); dirty-rect tối ưu ở T036+.
 * Bảng màu: contracts/render-gfx.md (research §15).
 * ⚠️ Lá/sâu hiện TÔ-MÀU-PHẲNG là TẠM — visual cuối (lá có dáng+gân, sâu phân khúc rõ) ở T091
 *    (research §15 "YÊU CẦU CHỐT"): ghép fill_rect/ô hoặc sprite gfx_blit (gộp theme T070). */

/* Ô lưới (c,r) → pixel landscape: sân nằm DƯỚI dải HUD cao HUD_H. */
static void cell_fill(int c, int r, uint16_t color)
{
  gfx_fill_rect(c * CELL, HUD_H + r * CELL, CELL, CELL, color);
}

/* Font 8×16: "mực" glyph nằm ở dòng 1..10 (5 dòng trống đáy) → căn dọc theo mực, KHÔNG theo ô,
 * nếu không chữ trông lệch lên trên trong hộp/nút. Trả y để vẽ chữ căn giữa hộp [box_y, box_h]. */
#define GLYPH_INK_TOP 1
#define GLYPH_INK_H   10
static int text_cy(int box_y, int box_h, int scale)
{
  return box_y + (box_h - GLYPH_INK_H * scale) / 2 - GLYPH_INK_TOP * scale;
}

/* uint32 → chuỗi thập phân (không dùng sprintf); trả số ký tự đã ghi. */
static int put_u32(char *dst, uint32_t v)
{
  char tmp[10];
  int n = 0;
  if (v == 0u) tmp[n++] = '0';
  while (v) { tmp[n++] = (char)('0' + (v % 10u)); v /= 10u; }
  for (int i = 0; i < n; i++) dst[i] = tmp[n - 1 - i];
  return n;
}

static void draw_hud(const GameState *gs)
{
  uint16_t bg = gfx_rgb565(20, 20, 35);
  gfx_fill_rect(0, 0, SCREEN_W, HUD_H, bg);

  char line[28];
  int p = 0;
  const char *s = "SCORE ";
  while (*s) line[p++] = *s++;
  p += put_u32(line + p, gs->score);
  line[p++] = ' '; line[p++] = ' ';
  if (gs->play_mode == MODE_ENDLESS) {           /* US5: Vô tận không có level → nhãn ENDLESS */
    const char *m = "ENDLESS";
    while (*m) line[p++] = *m++;
  } else {
    line[p++] = 'L'; line[p++] = 'V'; line[p++] = ' ';
    p += put_u32(line + p, (uint32_t)(gs->level_idx + 1));
  }
  line[p] = 0;
  gfx_text(6, 8, line, gfx_rgb565(235, 235, 235), bg, 1);

  /* T054: power-up đang hiệu lực — chữ + giây còn lại, căn phải HUD. */
  static const char PU_CH[PU_KINDS] = { 'S', 'W', 'G', 'P' };  /* Speed/sloW/Ghost/Phase */
  char pwr[20];
  int q = 0;
  for (int k = 0; k < PU_KINDS && q < 16; k++) {
    if (gs->power[k] > 0) {
      uint32_t sec = (uint32_t)((gs->power[k] + 999) / 1000);   /* làm tròn lên */
      pwr[q++] = PU_CH[k];
      q += put_u32(pwr + q, sec);
      pwr[q++] = ' ';
    }
  }
  if (q > 0) {
    pwr[q - 1] = 0;                              /* bỏ dấu cách cuối */
    gfx_text(SCREEN_W - q * 8, 8, pwr, gfx_rgb565(120, 220, 235), bg, 1);
  }
}

static void draw_leaf(const Leaf *lf, uint16_t color)
{
  if (lf->type != LEAF_NONE) {
    cell_fill(lf->pos.c, lf->pos.r, color);
  }
}

/* T054: màu power-up theo loại (research §15): SPEED cyan / SLOW lam / GHOST trắng / PHASE cam-gạch. */
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

static void draw_playing(const GameState *gs)
{
  /* Nền sân. */
  gfx_fill_rect(0, HUD_H, SCREEN_W, SCREEN_H - HUD_H, gfx_rgb565(11, 26, 11));

  /* Chướng ngại của màn (T045) — màu xám đá. */
  const Level *lv = level_get(gs->level_idx);
  if (lv != 0 && lv->obstacles != 0) {
    uint16_t ob = gfx_rgb565(95, 95, 110);
    for (int r = 0; r < ROWS; r++)
      for (int c = 0; c < COLS; c++)
        if (lv->obstacles[r][c]) cell_fill(c, r, ob);
  }

  /* Lá (research §15). */
  draw_leaf(&gs->leaf_normal, gfx_rgb565(46, 204, 64));
  draw_leaf(&gs->leaf_poison, gfx_rgb565(177, 13, 201));

  /* T051: lá vàng nhấp nháy — ẩn ô ~500ms một nhịp khi đồng hồ life_ms còn dương. */
  if (gs->leaf_gold.type == LEAF_GOLD) {
    int32_t life = gs->leaf_gold.life_ms;
    int blink_on = (life < 0) || (((life / 500) & 1) == 0);   /* vô hạn → luôn hiện */
    if (blink_on) {
      draw_leaf(&gs->leaf_gold, gfx_rgb565(255, 215, 0));
    }
  }

  /* T054: power-up — màu theo loại (research §15). */
  if (gs->leaf_pu.type == LEAF_POWERUP) {
    draw_leaf(&gs->leaf_pu, pu_color(gs->leaf_pu.pu_type));
  }

  /* Sâu: đầu cam đậm + chấm mắt, thân vàng-cam. */
  const Worm *w = &gs->worm;
  for (uint16_t k = 0; k < w->len; k++) {
    uint16_t i = (uint16_t)((w->head_idx + k) % WORM_CAP);
    Cell sg = w->body[i];
    if (k == 0) {
      cell_fill(sg.c, sg.r, gfx_rgb565(230, 126, 0));
      gfx_fill_rect(sg.c * CELL + CELL / 2, HUD_H + sg.r * CELL + CELL / 3,
                    3, 3, gfx_rgb565(10, 10, 10));   /* mắt */
    } else {
      cell_fill(sg.c, sg.r, gfx_rgb565(255, 180, 0));
    }
  }
}

/* T037: màn GAME_OVER — nền đỏ thẫm, tiêu đề, điểm cuối, nhắc bấm chơi lại. */
static void draw_game_over(const GameState *gs)
{
  uint16_t bg = gfx_rgb565(28, 8, 8);
  uint16_t fg = gfx_rgb565(240, 240, 240);
  gfx_clear(bg);
  /* Tiêu đề phóng to ×2; căn giữa thủ công theo độ dài chuỗi. */
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

/* T045: banner chữ giữa màn (LEVEL_COMPLETE/WIN) — tiêu đề + điểm + nhắc bấm. */
static void draw_banner(const GameState *gs, const char *title, int title_len,
                        uint16_t title_col, uint16_t bg, const char *hint, int hint_len)
{
  uint16_t fg = gfx_rgb565(240, 240, 240);
  gfx_clear(bg);
  gfx_text((SCREEN_W - title_len * 16) / 2, 56, title, title_col, bg, 2);   /* tiêu đề ×2 */

  char line[20];
  int p = 0;
  const char *s = "SCORE ";
  while (*s) line[p++] = *s++;
  p += put_u32(line + p, gs->score);
  line[p] = 0;
  gfx_text((SCREEN_W - p * 8) / 2, 118, line, fg, bg, 1);

  gfx_text((SCREEN_W - hint_len * 8) / 2, 158, hint, gfx_rgb565(180, 180, 180), bg, 1);
}

/* Khung viền 2px: tô màu viền rồi tô ruột → hộp có border (dùng cho nút/menu). */
static void draw_frame(int x, int y, int w, int h, uint16_t border, uint16_t fill)
{
  gfx_fill_rect(x, y, w, h, border);
  gfx_fill_rect(x + 2, y + 2, w - 4, h - 4, fill);
}

/* Một mục menu dạng nút có viền. `sel`=con trỏ đang ở mục này; `locked`=mục "SOON" (mock).
 * Mục thường: chọn → xanh sáng. Mục khoá: chọn → viền sáng (thấy con trỏ) nhưng ruột xám + thẻ SOON. */
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
    gfx_text(cx - w / 2 + 12, ty, label, txt, fill, 1);                        /* nhãn trái */
    gfx_text(cx + w / 2 - 12 - 4 * 8, ty, "SOON", gfx_rgb565(170, 120, 55), fill, 1);  /* thẻ SOON phải */
  } else {
    gfx_text(cx - len * 8 / 2, ty, label, txt, fill, 1);
  }
}

/* T060: màn MENU — tiêu đề ×2, 3 mục điều hướng được; START chạy, ENDLESS/THEME khoá (SOON). */
static void draw_menu(const GameState *gs)
{
  uint16_t bg = gfx_rgb565(8, 10, 26);
  gfx_clear(bg);

  /* Tiêu đề phóng to ×2: "LEAFMUNCHER" xanh lá + "+" cam. */
  int tx = (SCREEN_W - 12 * 16) / 2;
  gfx_text(tx, 24, "LEAFMUNCHER", gfx_rgb565(120, 230, 120), bg, 2);
  gfx_text(tx + 11 * 16, 24, "+", gfx_rgb565(255, 215, 0), bg, 2);
  gfx_fill_rect(tx, 60, 12 * 16, 2, gfx_rgb565(60, 120, 60));   /* gạch accent dưới tiêu đề */

  draw_menu_item(SCREEN_W / 2, 90,  176, 32, "START",   5, gs->menu_sel == 0, 0);
  draw_menu_item(SCREEN_W / 2, 128, 176, 32, "ENDLESS", 7, gs->menu_sel == 1, 0);  /* US5: chạy được */
  draw_menu_item(SCREEN_W / 2, 166, 176, 32, "THEME",   5, gs->menu_sel == 2, 1);  /* US6: SOON */

  gfx_text((SCREEN_W - 19 * 8) / 2, 210, "AXIS: MOVE  BTN: OK", gfx_rgb565(90, 92, 112), bg, 1);
}

/* T060: overlay PAUSED — vẽ sân, phủ mờ, hộp có viền + thanh tiêu đề (gfx_blend_rect, research §14). */
static void draw_paused(const GameState *gs)
{
  draw_hud(gs);
  draw_playing(gs);
  gfx_blend_rect(0, HUD_H, SCREEN_W, SCREEN_H - HUD_H, gfx_rgb565(0, 0, 0), 160);

  int bw = 200, bh = 104, bx = (SCREEN_W - bw) / 2, by = (SCREEN_H - bh) / 2;
  uint16_t border = gfx_rgb565(255, 235, 120);
  uint16_t fill   = gfx_rgb565(22, 26, 44);
  uint16_t barbg  = gfx_rgb565(40, 46, 74);
  draw_frame(bx, by, bw, bh, border, fill);
  /* Thanh tiêu đề cao 40px = vừa đủ chứa TRỌN ô chữ ×2 (32px) — nếu thấp hơn, dòng glyph
   * trống dưới cùng (vẽ nền barbg) tràn 1px xuống ruột → trông như đường ranh bị lệch. */
  gfx_fill_rect(bx + 2, by + 2, bw - 4, 40, barbg);
  gfx_text(SCREEN_W / 2 - 6 * 16 / 2, text_cy(by + 2, 40, 2), "PAUSED", border, barbg, 2);

  gfx_text(SCREEN_W / 2 - 12 * 8 / 2, by + 54, "PRESS BUTTON", gfx_rgb565(225, 225, 235), fill, 1);
  gfx_text(SCREEN_W / 2 - 9 * 8 / 2, by + 76, "TO RESUME", gfx_rgb565(150, 150, 165), fill, 1);
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
      draw_menu(gs);                       /* an toàn: mọi mode còn lại về MENU */
      break;
  }
}

void render_force_full(const GameState *gs)
{
  draw_by_mode(gs);
}

void render_frame(const GameState *gs, GameEvents ev)
{
  (void)ev;            /* M2: vẽ đủ mỗi khung; dùng ev cho dirty-rect ở T036+ */
  draw_by_mode(gs);
}
