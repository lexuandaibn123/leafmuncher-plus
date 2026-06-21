#include "render.h"
#include "gfx.h"

/* render — ánh xạ GameState → lệnh vẽ gfx (chỉ gọi gfx_*, KHÔNG chạm HAL — NT IV/V).
 * T024 (M2): vẽ HUD + sân + sâu, dispatch theo `mode`. M2 vẽ lại toàn khung mỗi tick
 * (chấp nhận được ở tốc snake, research §14); dirty-rect tối ưu ở T036+.
 * Bảng màu: contracts/render-gfx.md (research §15). */

/* Ô lưới (c,r) → pixel landscape: sân nằm DƯỚI dải HUD cao HUD_H. */
static void cell_fill(int c, int r, uint16_t color)
{
  gfx_fill_rect(c * CELL, HUD_H + r * CELL, CELL, CELL, color);
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
  line[p++] = 'L'; line[p++] = 'V'; line[p++] = ' ';
  p += put_u32(line + p, (uint32_t)(gs->level_idx + 1));
  line[p] = 0;
  gfx_text(6, 8, line, gfx_rgb565(235, 235, 235), bg);
}

static void draw_leaf(const Leaf *lf, uint16_t color)
{
  if (lf->type != LEAF_NONE) {
    cell_fill(lf->pos.c, lf->pos.r, color);
  }
}

static void draw_playing(const GameState *gs)
{
  /* Nền sân. */
  gfx_fill_rect(0, HUD_H, SCREEN_W, SCREEN_H - HUD_H, gfx_rgb565(11, 26, 11));

  /* Lá (research §15). */
  draw_leaf(&gs->leaf_normal, gfx_rgb565(46, 204, 64));
  draw_leaf(&gs->leaf_gold,   gfx_rgb565(255, 215, 0));
  draw_leaf(&gs->leaf_poison, gfx_rgb565(177, 13, 201));
  draw_leaf(&gs->leaf_pu,     gfx_rgb565(40, 200, 220));

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

static void draw_by_mode(const GameState *gs)
{
  switch (gs->mode) {
    case ST_PLAYING:
    case ST_PAUSED:
      draw_hud(gs);
      draw_playing(gs);
      break;
    default:
      /* MENU/GAME_OVER/LEVEL_COMPLETE/WIN — M2 tối thiểu: nền + nhãn (đầy đủ ở T060/US4). */
      gfx_clear(gfx_rgb565(8, 8, 28));
      gfx_text(80, 112, "LEAFMUNCHER+", gfx_rgb565(235, 235, 235), gfx_rgb565(8, 8, 28));
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
