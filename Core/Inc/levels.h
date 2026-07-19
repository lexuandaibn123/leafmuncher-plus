#ifndef LEVELS_H
#define LEVELS_H

/* Dữ liệu 5 màn chơi: bitmap chướng ngại vật 20x13, 
 * mục tiêu qua màn, và tốc độ cập nhật khung hình. */

#include "game.h"

#define LEVEL_COUNT LEVELS

typedef struct {
  const uint8_t (*obstacles)[COLS];
  uint16_t target_leaves;
  uint16_t step_ms;
} Level;

const Level *level_get(uint8_t idx);
uint8_t      level_is_last(uint8_t idx);

#endif /* LEVELS_H */
