#ifndef RENDER_H
#define RENDER_H

/* render — ánh xạ GameState → lệnh vẽ gfx (dirty-rect khi PLAYING, vẽ đủ khi đổi trạng thái).
 * Đọc theme hiện hành để tô màu/sprite. Hợp đồng: contracts/render-gfx.md.
 * Skeleton Phase 1 — hiện thực ở Phase 2+ (T024, T036…). */

#include "game.h"

void render_force_full(const GameState *gs);             /* vẽ lại toàn khung (khi đổi state) */
void render_frame(const GameState *gs, GameEvents ev);   /* vẽ theo gs->mode mỗi khung */

/* T074/US5: điểm cao Vô tận để HUD hiển thị "BEST n". `endless_high` thuộc `store` (không nằm trong
 * GameState — data-model §2.7) → tasks set giá trị này; mặc định 0. */
void render_set_endless_best(uint32_t best);

#endif /* RENDER_H */
