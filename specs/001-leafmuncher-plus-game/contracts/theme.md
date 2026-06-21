# Contract: `theme` — chủ đề hiển thị

Thuần hiển thị (cosmetic). KHÔNG đụng logic/va chạm/layout (Nguyên tắc II, V). Chỉ `render` dùng module này.

## Kiểu

```c
typedef enum { THEME_FOREST = 0, THEME_DESERT = 1, THEME_COUNT } ThemeId;

typedef struct {
  const char *name;          // "Khu rung" / "Sa mac"
  uint16_t bg;               // nền sân (RGB565)
  uint16_t grid;             // đường lưới mờ
  uint16_t hud_bg, text;     // nền HUD + chữ
  uint16_t worm_head, worm_body;
  uint16_t leaf_normal, leaf_gold, leaf_poison, leaf_power;
  uint16_t obstacle;         // màu nền ô chướng ngại
  const uint8_t *obstacle_sprite; // 16x16 sprite vẽ chướng ngại theo chủ đề (thân cây / đá)
  // (tuỳ chọn) hàm/route vẽ nền cảnh trang trí — không chiếm ô logic
} Theme;
```

## API

| Hàm | Mô tả |
|---|---|
| `const Theme *theme_get(ThemeId id)` | Trả con trỏ bảng theme (`const`, trong flash). |
| `int theme_count(void)` | Số theme khả dụng (= `THEME_COUNT`). |
| `ThemeId theme_next(ThemeId id)` | Theme kế tiếp (cuộn vòng) — cho menu đổi theme. |

## Bất biến / hợp đồng

- Toàn bộ dữ liệu theme là `const` trong Flash; module không cấp phát động, không gọi HAL.
- Đổi theme **chỉ** đổi cách `render` vẽ; KHÔNG đổi `occupied`, va chạm, mục tiêu, tốc độ.
- Ô chướng ngại vẫn do `levels` quyết định; theme chỉ chọn **sprite/màu** để vẽ ô đó.
- `theme_get(id)` với `id` hợp lệ luôn trả non-NULL; ngoài dải → trả theme mặc định (`THEME_FOREST`).
