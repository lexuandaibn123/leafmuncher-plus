#include "theme.h"

/* theme — 2 bảng màu const (T070). THUẦN, không HAL. RGB565 đóng gói bằng macro (không gọi
 * gfx_rgb565 để giữ module độc lập lớp gfx). Entity (sâu/lá vàng/độc/power-up) giữ màu giống
 * nhau giữa 2 theme để dễ nhận diện; theme đổi nền sân / HUD / chướng ngại tạo không khí khác. */

#define RGB565(r, g, b) ((uint16_t)((((r) & 0xF8u) << 8) | (((g) & 0xFCu) << 3) | ((b) >> 3)))

static const Theme THEMES[THEME_COUNT] = {
  /* THEME_FOREST — rừng: nền xanh thẫm, chướng ngại đá xám. */
  {
    .name        = "FOREST",
    .bg          = RGB565(11, 26, 11),
    .grid        = RGB565(24, 42, 24),
    .hud_bg      = RGB565(20, 20, 35),
    .text        = RGB565(235, 235, 235),
    .worm_head   = RGB565(230, 126, 0),
    .worm_body   = RGB565(255, 180, 0),
    .leaf_normal = RGB565(46, 204, 64),
    .leaf_gold   = RGB565(255, 215, 0),
    .leaf_poison = RGB565(177, 13, 201),
    .leaf_power  = RGB565(40, 200, 220),
    .obstacle    = RGB565(95, 95, 110),
    .obstacle_sprite = 0,
  },
  /* THEME_DESERT — sa mạc: nền nâu cát, chướng ngại đá nâu. */
  {
    .name        = "DESERT",
    .bg          = RGB565(40, 30, 16),
    .grid        = RGB565(72, 58, 32),
    .hud_bg      = RGB565(36, 28, 18),
    .text        = RGB565(245, 238, 220),
    .worm_head   = RGB565(230, 126, 0),
    .worm_body   = RGB565(255, 180, 0),
    .leaf_normal = RGB565(46, 204, 64),
    .leaf_gold   = RGB565(255, 215, 0),
    .leaf_poison = RGB565(177, 13, 201),
    .leaf_power  = RGB565(40, 200, 220),
    .obstacle    = RGB565(150, 110, 70),
    .obstacle_sprite = 0,
  },
};

const Theme *theme_get(ThemeId id)
{
  if ((int)id < 0 || (int)id >= THEME_COUNT) {
    return &THEMES[THEME_FOREST];          /* ngoài dải → mặc định */
  }
  return &THEMES[id];
}

int theme_count(void)
{
  return THEME_COUNT;
}

ThemeId theme_next(ThemeId id)
{
  return (ThemeId)(((int)id + 1) % THEME_COUNT);
}
