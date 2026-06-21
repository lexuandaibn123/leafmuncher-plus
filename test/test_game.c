/* test_game — unit-test logic thuần trên host (gcc). Nguyên tắc II / SC-006.
 * Phase 1: chỉ kiểm hằng số/kiểu chia sẻ (chưa có hàm logic). Test luật core thêm ở US1+ (T028…). */

#include <assert.h>
#include <stdio.h>
#include "game.h"

int main(void) {
  /* Hình học lưới khớp data-model/research §1 (lưới khít 320×240). */
  assert(COLS == 20 && ROWS == 13 && CELL == 16 && HUD_H == 32);
  assert(GRID_CELLS == 260);
  assert(COLS * CELL == SCREEN_W);
  assert(ROWS * CELL + HUD_H == SCREEN_H);

  /* Bất biến độ dài sâu (research §4). */
  assert(LEN_MIN <= LEN_START);
  assert(WORM_CAP == GRID_CELLS);

  /* GameState là POD đủ nhỏ để lưu Flash (research §20: ~1KB ≪ 16KB sector). */
  assert(sizeof(GameState) < 2048);

  printf("test_game (Phase 1 skeleton): all assertions passed; sizeof(GameState)=%lu\n",
         (unsigned long)sizeof(GameState));
  return 0;
}
