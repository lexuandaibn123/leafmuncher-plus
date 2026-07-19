#ifndef RNG_H
#define RNG_H

/* Bộ sinh số ngẫu nhiên giả (PRNG).
 * Sử dụng thuật toán xorshift32 đơn giản và hiệu quả. */

#include <stdint.h>

void     rng_seed(uint32_t *state, uint32_t seed);
uint32_t rng_next(uint32_t *state);
uint32_t rng_range(uint32_t *state, uint32_t n);

#endif /* RNG_H */
