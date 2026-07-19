#ifndef RENDER_H
#define RENDER_H

/* Ánh xạ trạng thái GameState thành các lệnh vẽ đồ họa.
 * Bảng màu cố định lấy theo chủ đề Forest. */

#include "game.h"

void render_force_full(const GameState *gs);
void render_frame(const GameState *gs, GameEvents ev);

/* Cập nhật điểm cao nhất của chế độ vô tận để hiển thị lên màn hình. */
void render_set_endless_best(uint32_t best);

#endif /* RENDER_H */
