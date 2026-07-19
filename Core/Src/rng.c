#include "rng.h"

/* Bộ sinh số ngẫu nhiên PRNG
 * State được giữ ngoài (con trỏ) nên hàm thuần, dễ test.
 * Lưu ý: xorshift32 KHÔNG được mang state = 0 (sẽ kẹt ở 0 mãi). */

/* Hằng "vá" khi seed = 0 */
#define RNG_NONZERO 0x9E3779B9u

void rng_seed(uint32_t *state, uint32_t seed) {
  *state = (seed != 0u) ? seed : RNG_NONZERO;
}

uint32_t rng_next(uint32_t *state) {
  uint32_t x = *state;
  /* Bộ ba dịch (13, 17, 5) */
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  *state = x;
  return x;
}

uint32_t rng_range(uint32_t *state, uint32_t n) {
  if (n == 0u) {
    return 0u;
  }
  /* Rejection sampling để loại bias modulo. */
  uint32_t limit = UINT32_MAX - (UINT32_MAX % n);
  uint32_t r;
  do {
    r = rng_next(state);
  } while (r >= limit);
  return r % n;
}
