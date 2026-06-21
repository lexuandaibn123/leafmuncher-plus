#ifndef LEVELS_H
#define LEVELS_H

/* levels — dữ liệu 5 màn (THUẦN, const trong flash): bitmap chướng ngại 20×13 +
 * target + step_ms. KHÔNG gọi HAL (Nguyên tắc II). Hợp đồng: contracts/levels.md.
 * T041 (US2/M4). */

#include "game.h"

#define LEVEL_COUNT LEVELS     /* 5 — đồng nhất với hằng tốc độ trong game.h */

typedef struct {
  const uint8_t (*obstacles)[COLS];  /* trỏ tới mảng [ROWS][COLS], 1 = chướng ngại */
  uint16_t target_leaves;            /* số lá thường cần ăn để qua màn */
  uint16_t step_ms;                  /* chu kỳ tick cơ bản của màn */
} Level;

const Level *level_get(uint8_t idx);   /* idx 0..LEVEL_COUNT-1; NULL nếu ngoài phạm vi */
uint8_t      level_is_last(uint8_t idx);

#endif /* LEVELS_H */
