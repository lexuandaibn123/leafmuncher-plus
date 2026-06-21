# UI Design — LeafMuncher+

Thiết kế hiển thị trên TFT 320×240 (ILI9341 qua LTDC). Nguồn quyết định: [research.md](../../specs/001-leafmuncher-plus-game/research.md)
§1 (hình học), §14 (chiến lược vẽ), §15 (font & màu), §17 (theme). Đây là bản nháp M1 — màu/sprite tinh
chỉnh khi demo (mọi mục *(tunable)*).

## 1. Bố cục màn hình (320×240)

```text
 (0,0)                                   (319,0)
   ┌───────────────────────────────────────┐
   │  HUD  y = 0..31  (cao 32px)            │   SCORE / LEVEL / icon power-up
   ├───────────────────────────────────────┤  y = 32
   │                                         │
   │  SÂN CHƠI  20 cột × 13 hàng             │   ô 16×16px
   │  y = 32..239 (cao 208px)                │
   │  ô (c,r) → pixel (c*16, 32 + r*16)      │
   │                                         │
   └───────────────────────────────────────┘
 (0,239)                                 (319,239)
```

- **Lưới**: `COLS=20 · ROWS=13 · CELL=16` → 20·16=320, 13·16=208, +HUD 32 = 240 (khít tuyệt đối).
- **Biên ẩn**: đầu sâu ra ngoài `0..19 / 0..12` = chết (FR-005). Vẽ 1 đường viền mảnh quanh sân cho dễ
  nhìn, không chiếm ô.
- **HUD** (y=0..31): trái = `SCORE xxxx`; giữa/phải = `LEVEL n` (chế độ Màn) hoặc `HIGH xxxx` (Vô tận);
  icon + thanh đếm ngược power-up đang hiệu lực.

## 2. Font

- **Bitmap 8×16 ASCII** `const` trong Flash; tập glyph: `0-9 A-Z : + - ! (space)` (~40 glyph).
- Chữ trên màn dùng ASCII: `SCORE` `LEVEL` `GAME OVER` `WIN` `PAUSED` `HIGH` `CONTINUE` …
- (Stretch sau M3: sprite 16×16 / chữ tiếng Việt có dấu — đổi được mà không sửa kiến trúc module.)

## 3. Bảng màu mặc định (RGB565, research §15)

| Thành phần | Màu | Hex |
|---|---|---|
| Nền sân | xanh rêu đậm | `#0B1A0B` |
| Thân sâu | vàng-cam | `#FFB400` |
| Đầu sâu | cam đậm + 1 chấm mắt | (đậm hơn thân) |
| Lá thường | xanh lá | `#2ECC40` |
| Lá vàng (nhấp nháy) | vàng | `#FFD700` |
| Lá độc | tím | `#B10DC9` |
| Chướng ngại | xám | `#555555` |
| Power-up SPEED | cyan | — |
| Power-up SLOW | lam | — |
| Power-up GHOST | trắng | — |
| Power-up PHASE | cam-gạch | — |

> Màu thực tế do **theme** quyết định lúc render (research §17): `THEME_FOREST` (rừng) / `THEME_DESERT`
> (sa mạc) đổi nền/màu/sprite chướng ngại; **luật & layout không đổi**. Bảng trên là theme Rừng mặc định.

## 4. Tóm tắt API `gfx` (chi tiết: contracts/render-gfx.md)

| Hàm (dự kiến) | Vai trò | DMA2D mode |
|---|---|---|
| `gfx_init()` | 2 framebuffer SDRAM + LTDC layer RGB565 | — |
| `gfx_clear()` / `gfx_fill_rect()` | tô nền / tô ô–HUD | R2M |
| `gfx_blit()` / `gfx_text()` | blit sprite / vẽ chuỗi font 8×16 | M2M |
| `gfx_blend_rect()` | phủ mờ overlay (PAUSE) | M2M_BLEND |
| `gfx_present()` | đặt cờ swap → áp dụng tại ngắt line LTDC (VSYNC) | — |

- Framebuffer: A=`0xD0000000`, B=`0xD0025800`, mỗi cái `0x25800` byte (320·240·2, RGB565).
- **Chiến lược vẽ** (research §14): PLAYING dùng **dirty-rect** (chỉ vẽ ô đầu mới + ô đuôi cũ + ô lá +
  vùng điểm HUD); đổi trạng thái → vẽ lại toàn khung 1 lần. Swap tại VSYNC → không xé hình (SC-003).

## 5. Vẽ theo trạng thái (FSM)

| Trạng thái | Nội dung màn |
|---|---|
| MENU | tiêu đề + danh sách: chế độ (Màn/Vô tận), theme, (Tiếp tục nếu có ô lưu); mục sáng = `menu_sel` |
| PLAYING | sân + sâu + lá + HUD (dirty-rect) |
| PAUSED | màn PLAYING + overlay mờ + hộp menu 3 mục (Tiếp tục / Lưu & Thoát / Thoát) |
| GAME_OVER | điểm cuối (Vô tận: + điểm cao); LED đỏ |
| LEVEL_COMPLETE | thông báo qua màn; chờ nút (SELECT) sang màn kế |
| WIN | màn thắng (hoàn thành level cuối) |
