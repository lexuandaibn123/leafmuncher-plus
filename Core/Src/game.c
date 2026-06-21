#include "game.h"
#include "rng.h"
#include <string.h>

/* game — logic thuần (Nguyên tắc II: KHÔNG gọi HAL/CMSIS/FreeRTOS).
 * T019: khởi tạo phiên + truy vấn đọc-chỉ. game_step/game_input_ui ở US1+ (T032–T035, T057+).
 *
 * ===== Ring buffer thân sâu (data-model §2.1) =====
 * Đốt đầu ở body[head_idx]; đốt thứ k (0 = đầu) ở body[(head_idx + k) % WORM_CAP];
 * đuôi ở k = len-1. game_step (T032) đẩy đầu mới vào (head_idx - 1 + CAP) % CAP. */

static int cell_in_grid(int c, int r)
{
  return (c >= 0 && c < COLS && r >= 0 && r < ROWS);
}

/* Dựng lại occupied[][] từ thân sâu (+ chướng ngại màn ở T041). */
static void grid_rebuild(GameState *gs)
{
  memset(gs->occupied, 0, sizeof gs->occupied);
  /* TODO(T041): nạp bitmap chướng ngại của màn vào occupied trước khi đánh dấu thân. */
  for (uint16_t k = 0; k < gs->worm.len; k++) {
    uint16_t i = (uint16_t)((gs->worm.head_idx + k) % WORM_CAP);
    Cell s = gs->worm.body[i];
    gs->occupied[s.r][s.c] = 1u;
  }
}

/* Đặt sâu dài LEN_START ở giữa sân, nằm ngang, đầu hướng phải. */
static void worm_spawn_center(GameState *gs)
{
  Worm *w = &gs->worm;
  int cx = COLS / 2, cy = ROWS / 2;   /* (10, 6) */
  w->len = LEN_START;
  w->head_idx = 0u;
  w->dir = DIR_RIGHT;
  w->next_dir = DIR_RIGHT;
  w->grow_pending = 0u;
  for (uint16_t k = 0; k < LEN_START; k++) {
    w->body[k].c = (int8_t)(cx - (int)k);   /* đầu (10,6); thân (9,6), (8,6) */
    w->body[k].r = (int8_t)cy;
  }
}

/* Reset toàn phiên về đầu ván màn 0 (dùng chung cho game_init & game_start). */
static void reset_session(GameState *gs)
{
  gs->level_idx    = 0u;
  gs->score        = 0u;
  gs->leaves_eaten = 0u;
  gs->step_ms      = STEP_MS[0];

  gs->leaf_normal.type = LEAF_NONE;
  gs->leaf_gold.type   = LEAF_NONE;
  gs->leaf_poison.type = LEAF_NONE;
  gs->leaf_pu.type     = LEAF_NONE;
  for (int k = 0; k < PU_KINDS; k++) {
    gs->power[k] = 0;
  }

  worm_spawn_center(gs);
  grid_rebuild(gs);
}

void game_init(GameState *gs, uint32_t seed)
{
  memset(gs, 0, sizeof *gs);
  rng_seed(&gs->rng, seed);
  gs->play_mode = MODE_LEVEL;
  gs->menu_sel  = 0u;
  reset_session(gs);
  gs->mode = ST_MENU;          /* khởi tạo dừng ở MENU (FR-014) */
}

void game_start(GameState *gs)
{
  reset_session(gs);           /* nạp màn 0, reset sâu/score/lá */
  gs->mode = ST_PLAYING;
}

LeafType game_cell_content(const GameState *gs, Cell c)
{
  if (!cell_in_grid(c.c, c.r)) {
    return LEAF_NONE;
  }
  const Leaf *leaves[4] = {
    &gs->leaf_normal, &gs->leaf_gold, &gs->leaf_poison, &gs->leaf_pu
  };
  for (int i = 0; i < 4; i++) {
    if (leaves[i]->type != LEAF_NONE &&
        leaves[i]->pos.c == c.c && leaves[i]->pos.r == c.r) {
      return leaves[i]->type;
    }
  }
  return LEAF_NONE;            /* trống (hoặc thân/chướng ngại — render đọc worm/occupied) */
}

uint16_t game_step_ms(const GameState *gs)
{
  return gs->step_ms;          /* tick hiệu dụng; hệ số power-up áp ở game_step (T034) */
}
