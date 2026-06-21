/* test_rng — kiểm tính xác định & biên của xorshift32 (T026 / M2).
 * Logic thuần, gcc host, KHÔNG link HAL — Nguyên tắc II / SC-006. */

#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include "rng.h"

int main(void) {
  /* 1) Xác định: cùng seed → cùng chuỗi. */
  uint32_t a, b;
  rng_seed(&a, 12345u);
  rng_seed(&b, 12345u);
  for (int i = 0; i < 1000; i++) {
    assert(rng_next(&a) == rng_next(&b));
  }

  /* 2) Seed khác → chuỗi khác (ít nhất 1 giá trị đầu khác). */
  uint32_t c, d;
  rng_seed(&c, 1u);
  rng_seed(&d, 2u);
  assert(rng_next(&c) != rng_next(&d));

  /* 3) Seed 0 được vá thành state khác 0 → không kẹt ở 0. */
  uint32_t z;
  rng_seed(&z, 0u);
  assert(z != 0u);
  int all_zero = 1;
  for (int i = 0; i < 100; i++) {
    if (rng_next(&z) != 0u) { all_zero = 0; break; }
  }
  assert(!all_zero);

  /* 4) rng_range luôn trong [0, n). */
  uint32_t s;
  rng_seed(&s, 0xC0FFEEu);
  for (int i = 0; i < 100000; i++) {
    uint32_t r = rng_range(&s, 260u);   /* GRID_CELLS */
    assert(r < 260u);
  }
  /* n == 0 → 0 (quy ước, tránh chia 0). */
  assert(rng_range(&s, 0u) == 0u);
  /* n == 1 → luôn 0. */
  for (int i = 0; i < 100; i++) {
    assert(rng_range(&s, 1u) == 0u);
  }

  /* 5) Phủ phân bố thô: rng_range(&,6) phải chạm đủ 6 mặt (xúc xắc). */
  uint32_t hit[6] = {0};
  uint32_t e;
  rng_seed(&e, 777u);
  for (int i = 0; i < 6000; i++) {
    hit[rng_range(&e, 6u)]++;
  }
  for (int f = 0; f < 6; f++) {
    assert(hit[f] > 0u);
  }

  printf("test_rng: all assertions passed (xorshift32 deterministic, range bounded)\n");
  return 0;
}
