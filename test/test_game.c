/* test_game — unit-test logic thuần trên host (gcc). Nguyên tắc II / SC-006.
 * Phase 1: hằng số/kiểu chia sẻ. T019: game_init/game_start + truy vấn đọc-chỉ.
 * Luật core (ăn/va chạm/power-up) thêm ở US1+ (T028…). */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "game.h"

static int occupied_count(const GameState *gs) {
  int n = 0;
  for (int r = 0; r < ROWS; r++)
    for (int c = 0; c < COLS; c++)
      n += gs->occupied[r][c] ? 1 : 0;
  return n;
}

/* Đốt thứ k (0 = đầu) trong ring buffer. */
static Cell worm_seg(const GameState *gs, int k) {
  int i = (gs->worm.head_idx + k) % WORM_CAP;
  return gs->worm.body[i];
}

int main(void) {
  /* ---- Phase 1: hình học lưới khớp data-model/research §1 ---- */
  assert(COLS == 20 && ROWS == 13 && CELL == 16 && HUD_H == 32);
  assert(GRID_CELLS == 260);
  assert(COLS * CELL == SCREEN_W);
  assert(ROWS * CELL + HUD_H == SCREEN_H);
  assert(LEN_MIN <= LEN_START);
  assert(WORM_CAP == GRID_CELLS);
  assert(sizeof(GameState) < 2048);

  /* ---- Bảng màn (levels.c) khớp research §3 ---- */
  assert(STEP_MS[0] == 180 && STEP_MS[LEVELS - 1] == 95);
  assert(TARGET_LEAVES[0] == 6 && TARGET_LEAVES[LEVELS - 1] == 14);

  /* ---- T019: game_init ---- */
  GameState gs;
  game_init(&gs, 12345u);
  assert(gs.mode == ST_MENU);
  assert(gs.play_mode == MODE_LEVEL);
  assert(gs.level_idx == 0 && gs.score == 0 && gs.leaves_eaten == 0);
  assert(gs.step_ms == STEP_MS[0]);
  assert(gs.rng == 12345u);                 /* seed != 0 giữ nguyên */
  assert(gs.menu_sel == 0);

  /* Sâu LEN_START ở giữa, nằm ngang, đầu hướng phải. */
  assert(gs.worm.len == LEN_START);
  assert(gs.worm.dir == DIR_RIGHT && gs.worm.next_dir == DIR_RIGHT);
  assert(gs.worm.grow_pending == 0);
  Cell head = worm_seg(&gs, 0);
  Cell neck = worm_seg(&gs, 1);
  Cell tail = worm_seg(&gs, LEN_START - 1);
  assert(head.c == COLS / 2 && head.r == ROWS / 2);     /* (10,6) */
  assert(neck.c == COLS / 2 - 1 && neck.r == ROWS / 2); /* (9,6)  */
  assert(tail.c == COLS / 2 - 2 && tail.r == ROWS / 2); /* (8,6)  */

  /* occupied chỉ gồm 3 đốt thân; mỗi đốt khớp occupied. */
  assert(occupied_count(&gs) == LEN_START);
  for (int k = 0; k < LEN_START; k++) {
    Cell s = worm_seg(&gs, k);
    assert(gs.occupied[s.r][s.c] == 1);
  }

  /* Chưa có lá nào. */
  assert(gs.leaf_normal.type == LEAF_NONE && gs.leaf_gold.type == LEAF_NONE);
  assert(gs.leaf_poison.type == LEAF_NONE && gs.leaf_pu.type == LEAF_NONE);
  for (int k = 0; k < PU_KINDS; k++) assert(gs.power[k] == 0);

  /* ---- T019: game_cell_content + game_step_ms ---- */
  Cell c_empty = { 0, 0 };
  assert(game_cell_content(&gs, c_empty) == LEAF_NONE);
  Cell c_oob = { -1, 5 };                                /* ngoài lưới → NONE, không tràn */
  assert(game_cell_content(&gs, c_oob) == LEAF_NONE);
  Cell c_oob2 = { COLS, ROWS };
  assert(game_cell_content(&gs, c_oob2) == LEAF_NONE);
  /* Đặt thử 1 lá vàng rồi truy vấn (kiểm ánh xạ ô→loại lá). */
  gs.leaf_gold.type = LEAF_GOLD; gs.leaf_gold.pos.c = 3; gs.leaf_gold.pos.r = 4;
  Cell c_gold = { 3, 4 };
  assert(game_cell_content(&gs, c_gold) == LEAF_GOLD);
  assert(game_step_ms(&gs) == STEP_MS[0]);

  /* ---- T019: game_start ---- */
  game_start(&gs);
  assert(gs.mode == ST_PLAYING);
  assert(gs.score == 0 && gs.level_idx == 0 && gs.leaves_eaten == 0);
  assert(gs.worm.len == LEN_START);
  assert(gs.leaf_gold.type == LEAF_NONE);   /* reset xoá lá đã đặt ở trên */
  assert(occupied_count(&gs) == LEN_START);

  /* ---- Tính xác định: cùng seed → cùng state byte-for-byte ---- */
  GameState a, b;
  game_init(&a, 0xC0FFEEu);
  game_init(&b, 0xC0FFEEu);
  assert(memcmp(&a, &b, sizeof a) == 0);
  /* Seed 0 được "vá" về khác 0 (xorshift không kẹt). */
  GameState z; game_init(&z, 0u);
  assert(z.rng != 0u);

  printf("test_game: all assertions passed (T019 init/start/queries); sizeof(GameState)=%lu\n",
         (unsigned long)sizeof(GameState));
  return 0;
}
