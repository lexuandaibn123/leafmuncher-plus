#ifndef RNG_H
#define RNG_H

/* rng — PRNG thuần (xorshift32). KHÔNG gọi HAL (Nguyên tắc II). Hợp đồng: research §13.
 * Skeleton Phase 1 — hiện thực ở T007. */

#include <stdint.h>

void     rng_seed(uint32_t *state, uint32_t seed);
uint32_t rng_next(uint32_t *state);
uint32_t rng_range(uint32_t *state, uint32_t n);   /* [0, n) */

#endif /* RNG_H */
