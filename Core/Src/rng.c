#include "rng.h"

/* rng — PRNG thuần xorshift32 (Marsaglia). KHÔNG gọi HAL (Nguyên tắc II).
 * Hợp đồng: research §13. T007 (M1).
 *
 * State được giữ ngoài (con trỏ) nên hàm thuần, dễ test xác định trên host.
 * Lưu ý: xorshift32 KHÔNG được mang state = 0 (sẽ kẹt ở 0 mãi). */

/* Hằng "vá" khi seed = 0 — số lẻ bất kỳ khác 0 (golden-ratio 32-bit). */
#define RNG_NONZERO 0x9E3779B9u

void rng_seed(uint32_t *state, uint32_t seed) {
  *state = (seed != 0u) ? seed : RNG_NONZERO;
}

uint32_t rng_next(uint32_t *state) {
  uint32_t x = *state;
  /* Bộ ba dịch (13, 17, 5) — chu kỳ tối đa 2^32 - 1. */
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  *state = x;
  return x;
}

uint32_t rng_range(uint32_t *state, uint32_t n) {
  if (n == 0u) {
    return 0u;            /* tránh chia 0; quy ước [0,0) = {0} */
  }
  /* Rejection sampling để loại bias modulo (n nhỏ ≤ GRID_CELLS, gần như không lặp). */
  uint32_t limit = UINT32_MAX - (UINT32_MAX % n);
  uint32_t r;
  do {
    r = rng_next(state);
  } while (r >= limit);
  return r % n;
}
