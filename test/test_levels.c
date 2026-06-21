/* test_levels — bất biến dữ liệu màn (host, SC-006). T039 (US2/M4).
 * Hợp đồng: contracts/levels.md. KHÔNG link HAL (Nguyên tắc II). */

#include <assert.h>
#include <stdio.h>
#include "game.h"
#include "levels.h"

int main(void) {
  assert(LEVEL_COUNT == LEVELS);

  uint16_t prev_step = 0xFFFFu;
  for (uint8_t i = 0; i < LEVEL_COUNT; i++) {
    const Level *lv = level_get(i);
    assert(lv != 0);
    assert(lv->obstacles != 0);

    /* Target > 0 và đồng nhất với bảng TARGET_LEAVES (game.h). */
    assert(lv->target_leaves > 0);
    assert(lv->target_leaves == TARGET_LEAVES[i]);

    /* step_ms giảm dần nghiêm ngặt, ≥ STEP_MS_MIN, đồng nhất STEP_MS. */
    assert(lv->step_ms == STEP_MS[i]);
    assert(lv->step_ms >= STEP_MS_MIN);
    assert(lv->step_ms < prev_step);     /* đơn điệu giảm (FR-008) */
    prev_step = lv->step_ms;

    /* Bitmap chỉ chứa {0,1}; đếm ô trống. */
    int free_n = 0;
    for (int r = 0; r < ROWS; r++) {
      for (int c = 0; c < COLS; c++) {
        uint8_t v = lv->obstacles[r][c];
        assert(v == 0 || v == 1);
        if (v == 0) free_n++;
      }
    }
    /* Đủ ô trống cho sâu khởi đầu + ≥1 ô sinh lá. */
    assert(free_n >= LEN_START + 1);

    /* Ô khởi đầu sâu (hàng ROWS/2, cột COLS/2, /2-1, /2-2) + lân cận đầu = TRỐNG. */
    int hr = ROWS / 2, hc = COLS / 2;
    assert(lv->obstacles[hr][hc]     == 0);   /* (10,6) đầu */
    assert(lv->obstacles[hr][hc - 1] == 0);   /* (9,6)  */
    assert(lv->obstacles[hr][hc - 2] == 0);   /* (8,6) đuôi */
    assert(lv->obstacles[hr][hc + 1] == 0);   /* (11,6) ô tiến tới đầu tiên */
  }

  /* level_is_last + ngoài phạm vi → NULL. */
  assert(level_is_last(LEVEL_COUNT - 1) == 1);
  assert(level_is_last(0) == 0);
  assert(level_get(LEVEL_COUNT) == 0);
  assert(level_get(200) == 0);

  printf("test_levels: all assertions passed (T039 level invariants)\n");
  return 0;
}
