#ifndef THEME_H
#define THEME_H

/* theme — chủ đề hiển thị (cosmetic). THUẦN: KHÔNG gọi HAL, dữ liệu const trong Flash.
 * Chỉ `render` dùng. Đổi theme KHÔNG đổi logic/va chạm/layout (NT II, V).
 * Hợp đồng: contracts/theme.md. ThemeId định nghĩa trong game.h. */

#include <stdint.h>
#include "game.h"   /* ThemeId { THEME_FOREST, THEME_DESERT, THEME_COUNT } */

typedef struct {
  const char *name;                 /* "FOREST" / "DESERT" */
  uint16_t bg;                      /* nền sân (RGB565) */
  uint16_t grid;                    /* đường lưới mờ (dự phòng) */
  uint16_t hud_bg, text;            /* nền HUD + chữ */
  uint16_t worm_head, worm_body;
  uint16_t leaf_normal, leaf_gold, leaf_poison, leaf_power;
  uint16_t obstacle;                /* màu ô chướng ngại */
  const uint8_t *obstacle_sprite;   /* sprite 16×16 (NULL = tô màu phẳng; sprite ở T091/polish) */
} Theme;

const Theme *theme_get(ThemeId id);    /* id hợp lệ → non-NULL; ngoài dải → THEME_FOREST */
int         theme_count(void);         /* = THEME_COUNT */
ThemeId     theme_next(ThemeId id);    /* theme kế (cuộn vòng) */

#endif /* THEME_H */
