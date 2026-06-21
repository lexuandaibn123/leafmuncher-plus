#include "levels.h"

/* Bảng tick & mục tiêu mỗi màn (research §3,§9). Khai báo extern trong game.h.
 * Lát cắt cần cho T019 (game_init/game_start/game_step_ms); bitmap chướng ngại 20×13
 * đầy đủ + struct Level vẫn hiện thực ở T041. */
const uint16_t STEP_MS[LEVELS]       = { 180, 155, 130, 110, 95 };
const uint16_t TARGET_LEAVES[LEVELS] = {   6,   8,  10,  12, 14 };
