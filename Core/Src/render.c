#include "render.h"
#include "gfx.h"
#include "levels.h"   /* level_get → vẽ chướng ngại màn (T045) */
#include "theme.h"    /* theme_get → màu theo chủ đề (T073/US6) */

/* render — ánh xạ GameState → lệnh vẽ gfx (chỉ gọi gfx_*, KHÔNG chạm HAL — NT IV/V).
 * T024 (M2): vẽ HUD + sân + sâu, dispatch theo `mode`. M2 vẽ lại toàn khung mỗi tick
 * (chấp nhận được ở tốc snake, research §14); dirty-rect tối ưu ở T036+.
 * Bảng màu: theo `theme` (T073). T091: lá có hình thù (rừng=lá cây, sa mạc=cỏ lăn; vàng=đồng xu,
 * độc=đầu lâu, power-up=token chữ) + sâu phân khúc (đốt thụt có khe, đầu có mắt theo hướng) — ghép
 * fill_rect qua helper `px`. (Sprite chướng ngại theo theme còn để ngỏ — obstacle_sprite=NULL.) */

/* Ô lưới (c,r) → pixel landscape: sân nằm DƯỚI dải HUD cao HUD_H. */
static void cell_fill(int c, int r, uint16_t color)
{
  gfx_fill_rect(c * CELL, HUD_H + r * CELL, CELL, CELL, color);
}

/* T091: vẽ 1 khối con TRONG ô (c,r) tại offset (dx,dy), kích thước w×h — để dựng hình thù
 * (lá/sâu phân khúc) bằng vài fill_rect thay vì tô đặc cả ô. */
static void px(int c, int r, int dx, int dy, int w, int h, uint16_t color)
{
  gfx_fill_rect(c * CELL + dx, HUD_H + r * CELL + dy, w, h, color);
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
  const Theme *th = theme_get(gs->theme_id);
  uint16_t bg = th->hud_bg;
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
  gfx_text(6, 8, line, th->text, bg, 1);

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

/* ===== T091: sprite item 16×16 (indexed; vẽ run-length) + đá vát + sâu cải tiến =====
 * Bitmap & bảng màu là nguồn chuẩn từ tools/gen_sprites_preview.py (đã duyệt PNG):
 * sáng trên-trái, viền sel-out, ≤6 màu/sprite (nguyên tắc pixel-art 16×16). '.' = trong suốt. */

/* Bảng màu pixel-sprite: mỗi ký tự → 1 màu (các ký tự không trùng nhau giữa các sprite). */
static uint16_t spr_color(char k)
{
  switch (k) {
    case 'o': return gfx_rgb565(18, 92, 34);     /* viền lá đậm   */
    case 'G': return gfx_rgb565(46, 204, 64);    /* lá xanh nền   */
    case 'L': return gfx_rgb565(130, 240, 130);  /* lá mặt sáng   */
    case 'V': return gfx_rgb565(28, 134, 52);    /* gân lá (đậm)  */
    case 'm': return gfx_rgb565(96, 64, 22);     /* cuống nâu     */
    case 'T': return gfx_rgb565(196, 162, 96);   /* cát           */
    case 'H': return gfx_rgb565(228, 202, 150);  /* cát sáng      */
    case 'D': return gfx_rgb565(118, 84, 40);    /* nhánh khô     */
    case 'y': return gfx_rgb565(190, 150, 0);    /* viền vàng     */
    case 'Y': return gfx_rgb565(255, 215, 0);    /* vàng          */
    case 'W': return gfx_rgb565(255, 246, 178);  /* lóa sáng      */
    case 'p': return gfx_rgb565(92, 12, 112);    /* tím đậm       */
    case 'P': return gfx_rgb565(206, 138, 226);  /* xương tím     */
    default:  return 0;
  }
}

/* Vẽ 1 sprite 16×16 tại ô (c,r): gom pixel cùng màu trên mỗi hàng thành 1 fill_rect. */
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
static const char *const SPR_TUMBLE[16] = {
  ".......D........", ".....DDTDD......", "...DTDD.DDTD....", "....D..D..D.....",
  "..HD.HDTTD.DD...", ".TDD.DD.DD.DDT..", ".D..D..D..D..D..", "DTDT.HT.DT.DTTD.",
  "..DT.DTTTT..T...", ".D..D..D..D..D..", "..TD.DD.D..DT...", "..D..DD..D..D...",
  "....D..TT.D.....", ".....T.TDT......", "................", "................",
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

/* Đổi độ sáng màu RGB565 theo tỉ lệ num/den (để dựng vát sáng/tối từ màu theme). */
static uint16_t shade565(uint16_t col, int num, int den)
{
  int r = (col >> 11) & 0x1F, g = (col >> 5) & 0x3F, b = col & 0x1F;
  r = r * num / den; g = g * num / den; b = b * num / den;
  if (r > 31) r = 31;
  if (g > 63) g = 63;
  if (b > 31) b = 31;
  return (uint16_t)((r << 11) | (g << 5) | b);
}

/* Chướng ngại: khối đá VÁT 3D suy từ màu theme — mép trên-trái sáng, dưới-phải tối,
 * thêm vết nứt chéo + sạn (khớp tools/gen_sprites_preview.py). */
static void draw_obstacle(int c, int r, uint16_t base)
{
  static const uint8_t crack[6][2] = { {10,3},{10,4},{9,5},{9,6},{8,7},{8,8} };
  static const uint8_t grit[4][2]  = { {4,5},{6,11},{12,9},{5,9} };
  uint16_t hi = shade565(base, 135, 100), lo = shade565(base, 60, 100), dk = shade565(base, 45, 100);
  cell_fill(c, r, base);
  px(c, r, 0, 0, 16, 2, hi); px(c, r, 0, 0, 2, 16, hi);    /* mép trên + trái sáng */
  px(c, r, 0, 14, 16, 2, lo); px(c, r, 14, 0, 2, 16, lo);  /* mép dưới + phải tối  */
  for (int i = 0; i < 6; i++) px(c, r, crack[i][0], crack[i][1], 1, 1, dk);
  for (int i = 0; i < 4; i++) px(c, r, grit[i][0], grit[i][1], 1, 1, dk);
}

/* Power-up: token bo góc theo màu loại + highlight/bóng + chữ S/W/G/P giữa ô. */
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
  px(c, r, 1, 1, 14, 14, col);                            /* thân token */
  px(c, r, 1, 1, 1, 1, bg); px(c, r, 14, 1, 1, 1, bg);    /* bo 4 góc */
  px(c, r, 1, 14, 1, 1, bg); px(c, r, 14, 14, 1, 1, bg);
  px(c, r, 3, 2, 9, 2, hi); px(c, r, 3, 12, 9, 2, lo);    /* highlight/bóng */
  char s[2] = { pu_letter(t), 0 };
  gfx_text(c * CELL + 4, HUD_H + r * CELL, s, gfx_rgb565(20, 20, 30), col, 1);
}

/* Cờ cầu nối đốt sâu sang ô kề (đọc liền mạch khi rẽ). */
#define CONN_R 1u
#define CONN_L 2u
#define CONN_U 4u
#define CONN_D 8u

/* Một đốt/đầu sâu: thân thụt bo góc + cầu nối theo `conn` + highlight trên/bóng dưới.
 * `is_head` thêm 2 mắt (trắng + đồng tử) quay theo `dir`. `bg` dùng để bo góc. */
static void draw_worm_body(int c, int r, uint16_t col, uint16_t bg, unsigned conn, int is_head, Dir dir)
{
  uint16_t hi = shade565(col, 130, 100), lo = shade565(col, 62, 100);
  px(c, r, 1, 1, 14, 14, col);                            /* thân thụt 1px */
  px(c, r, 1, 1, 1, 1, bg); px(c, r, 14, 1, 1, 1, bg);    /* bo 4 góc */
  px(c, r, 1, 14, 1, 1, bg); px(c, r, 14, 14, 1, 1, bg);
  if (conn & CONN_R) px(c, r, 14, 4, 2, 8, col);          /* cầu nối 4 hướng */
  if (conn & CONN_L) px(c, r, 0, 4, 2, 8, col);
  if (conn & CONN_U) px(c, r, 4, 0, 8, 2, col);
  if (conn & CONN_D) px(c, r, 4, 14, 8, 2, col);
  px(c, r, 3, 2, 9, 2, hi);                               /* highlight trên */
  px(c, r, 3, 12, 9, 2, lo);                              /* bóng dưới */
  if (is_head) {
    uint16_t white = gfx_rgb565(245, 245, 245), pup = gfx_rgb565(15, 15, 15);
    int e1x, e1y, e2x, e2y;
    switch (dir) {
      case DIR_RIGHT: e1x = 10; e1y = 3;  e2x = 10; e2y = 10; break;
      case DIR_LEFT:  e1x = 3;  e1y = 3;  e2x = 3;  e2y = 10; break;
      case DIR_UP:    e1x = 3;  e1y = 3;  e2x = 10; e2y = 3;  break;
      default:        e1x = 3;  e1y = 10; e2x = 10; e2y = 10; break;   /* DIR_DOWN */
    }
    px(c, r, e1x, e1y, 3, 3, white); px(c, r, e2x, e2y, 3, 3, white);
    px(c, r, e1x + 1, e1y + 1, 1, 1, pup); px(c, r, e2x + 1, e2y + 1, 1, 1, pup);
  }
}

/* Δ(ô kề − ô này) → cờ hướng cầu nối (chỉ khi kề trực giao; PHASE wrap thì bỏ qua). */
static unsigned conn_bit(int dc, int dr)
{
  if (dc == 1 && dr == 0) return CONN_R;
  if (dc == -1 && dr == 0) return CONN_L;
  if (dc == 0 && dr == -1) return CONN_U;
  if (dc == 0 && dr == 1) return CONN_D;
  return 0u;                                              /* không kề (wrap) → không nối */
}

static void draw_playing(const GameState *gs)
{
  const Theme *th = theme_get(gs->theme_id);   /* T073: màu theo theme hiện hành */
  int desert = (gs->theme_id == THEME_DESERT);

  /* Nền sân. */
  gfx_fill_rect(0, HUD_H, SCREEN_W, SCREEN_H - HUD_H, th->bg);

  /* Chướng ngại của màn (T045/T073) — đá vát 3D theo màu theme. */
  const Level *lv = level_get(gs->level_idx);
  if (lv != 0 && lv->obstacles != 0) {
    for (int r = 0; r < ROWS; r++)
      for (int c = 0; c < COLS; c++)
        if (lv->obstacles[r][c]) draw_obstacle(c, r, th->obstacle);
  }

  /* T091: lá thường — sprite theo theme: rừng = lá cây (có gân), sa mạc = cỏ lăn khô. */
  if (gs->leaf_normal.type != LEAF_NONE) {
    int lc = gs->leaf_normal.pos.c, lr = gs->leaf_normal.pos.r;
    draw_sprite(lc, lr, desert ? SPR_TUMBLE : SPR_LEAF);
  }

  /* T091: lá độc — đầu lâu tím. */
  if (gs->leaf_poison.type == LEAF_POISON) {
    draw_sprite(gs->leaf_poison.pos.c, gs->leaf_poison.pos.r, SPR_POISON);
  }

  /* T051/T091: lá vàng — đồng xu, nhấp nháy ~500ms khi sắp hết hạn. */
  if (gs->leaf_gold.type == LEAF_GOLD) {
    int32_t life = gs->leaf_gold.life_ms;
    int blink_on = (life < 0) || (((life / 500) & 1) == 0);   /* vô hạn → luôn hiện */
    if (blink_on) {
      draw_sprite(gs->leaf_gold.pos.c, gs->leaf_gold.pos.r, SPR_GOLD);
    }
  }

  /* T054/T091: power-up — token chữ S/W/G/P theo loại. */
  if (gs->leaf_pu.type == LEAF_POWERUP) {
    draw_powerup_token(gs->leaf_pu.pos.c, gs->leaf_pu.pos.r, gs->leaf_pu.pu_type, th->bg);
  }

  /* T091: sâu liền mạch — mỗi đốt nối sang ô kề (đầu + đuôi trong chuỗi), bo góc,
   * highlight trên/bóng dưới; đầu có 2 mắt theo hướng. */
  const Worm *w = &gs->worm;
  for (uint16_t k = 0; k < w->len; k++) {
    uint16_t i = (uint16_t)((w->head_idx + k) % WORM_CAP);
    Cell sg = w->body[i];
    unsigned conn = 0u;
    if (k > 0) {                                          /* nối tới đốt trước (về phía đầu) */
      Cell pv = w->body[(w->head_idx + k - 1) % WORM_CAP];
      conn |= conn_bit(pv.c - sg.c, pv.r - sg.r);
    }
    if (k + 1 < w->len) {                                 /* nối tới đốt sau (về phía đuôi) */
      Cell nx = w->body[(w->head_idx + k + 1) % WORM_CAP];
      conn |= conn_bit(nx.c - sg.c, nx.r - sg.r);
    }
    if (k == 0) draw_worm_body(sg.c, sg.r, th->worm_head, th->bg, conn, 1, w->dir);
    else        draw_worm_body(sg.c, sg.r, th->worm_body, th->bg, conn, 0, w->dir);
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

  /* US6: mục THEME hiện tên theme hiện hành; SELECT cuộn vòng (FOREST/DESERT). */
  char tlabel[16];
  int tn = 0;
  const char *tp = "THEME:";
  while (*tp) tlabel[tn++] = *tp++;
  const char *nm = theme_get(gs->theme_id)->name;
  while (*nm && tn < 15) tlabel[tn++] = *nm++;
  tlabel[tn] = 0;
  draw_menu_item(SCREEN_W / 2, 166, 176, 32, tlabel, tn, gs->menu_sel == 2, 0);

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
