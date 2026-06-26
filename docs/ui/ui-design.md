# UI Design — LeafMuncher+

Thiết kế hiển thị trên TFT 320×240 (ILI9341 qua LTDC). Nguồn quyết định: [research.md](../../specs/001-leafmuncher-plus-game/research.md)
§1 (hình học), §14 (chiến lược vẽ), §15 (font & màu), §17 (theme). **Bản M8 — màu/sprite/font khớp
mã đã chốt** (`Core/Src/render.c`, `theme.c`); preview sprite ở [`sprites/preview.png`](sprites/preview.png),
bản đồ màn ở [`levels/`](levels/).

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

- **Bitmap 8×16** `FONT8X16[96][16]` `const` trong Flash — ASCII in được `0x20–0x7F` (96 glyph),
  định nghĩa trong `Core/Src/font8x16.c`. Game dùng chữ **HOA** + số + dấu `:` `+` `-` `&` `!`.
- "Mực" glyph nằm dòng 1..10 (5 dòng trống đáy) → căn dọc theo mực (`text_cy` trong render.c), không
  theo ô, nếu không chữ lệch lên trên trong hộp/nút.
- Chữ trên màn: `SCORE` `LV n` `ENDLESS BEST n` `PAUSED` `RESUME` `SAVE & EXIT` `EXIT` `START`
  `ENDLESS` `THEME:` `CONTINUE LV` `CONTINUE ENDL` `GAME OVER` `YOU WIN!` `LEVEL CLEAR` …

## 3. Bảng màu thực tế theo theme (RGB565, `Core/Src/theme.c`)

Màu do **theme** quyết định lúc render (research §17). Entity (sâu / lá vàng / độc / power-up) **giữ
màu giống nhau giữa 2 theme** để dễ nhận diện; theme đổi **nền sân / lưới / HUD / màu đá chướng ngại +
lá thường** tạo không khí khác — **luật & layout không đổi**.

| Thành phần (field) | `THEME_FOREST` (rừng) | `THEME_DESERT` (sa mạc) |
|---|---|---|
| Nền sân `bg` | `#0B1A0B` (xanh thẫm) | `#281E10` (nâu cát) |
| Lưới `grid` | `#182A18` | `#483A20` |
| Nền HUD `hud_bg` | `#141423` | `#241C12` |
| Chữ `text` | `#EBEBEB` | `#F5EEDC` |
| Đá chướng ngại `obstacle` | `#5F5F6E` (xám) | `#966E46` (nâu) |
| Lá thường `leaf_normal` | `#2ECC40` (lá cây xanh) | `#C8A55F` (cỏ lăn khô) |
| Đầu sâu `worm_head` | `#E67E00` (cam đậm) | ← giống |
| Thân sâu `worm_body` | `#FFB400` (vàng-cam) | ← giống |
| Lá vàng `leaf_gold` | `#FFD700` | ← giống |
| Lá độc `leaf_poison` | `#B10DC9` (tím) | ← giống |
| Lá power-up `leaf_power` | `#28C8DC` (cyan) | ← giống |

**Màu power-up** (theo loại, `pu_color`/`pu_letter` trong render.c): SPEED `S` cyan `#28C8DC` ·
SLOW `W` lam `#285ADC` · GHOST `G` trắng `#EBEBEB` · PHASE `P` cam-gạch `#C85A28`.

## 3b. Sprite item & sâu (T091 — indexed 16×16, run-length)

Item vẽ bằng **bitmap 16×16 indexed** (chuỗi ASCII + bảng màu ký tự `spr_color`, gom pixel cùng màu mỗi
hàng thành 1 `fill_rect`; `.` = trong suốt). Nguyên tắc pixel-art: sáng trên-trái, viền *sel-out*,
≤6 màu/sprite. Nguồn chuẩn & duyệt mắt qua [`tools/gen_sprites_preview.py`](../../tools/gen_sprites_preview.py).

| Item | Sprite | Màu chính (ký tự) |
|---|---|---|
| Lá rừng | phiến phình + gân giữa/xương cá + cuống | viền `o #125C22` · nền `G #2ECC40` · sáng `L #82F082` · gân `V #1C8634` · cuống `m #604016` |
| Cỏ lăn (sa mạc) | búi nhánh khô đan hở | cát `T #C4A260` · cát sáng `H #E4CA96` · nhánh `D #765428` |
| Lá vàng | đồng xu tròn + lóa | viền `y #BE9600` · vàng `Y #FFD700` · lóa `W #FFF6B2` |
| Lá độc | đầu lâu tím đối xứng | tím đậm `p #5C0C70` · xương `P #CE8AE2` |
| Power-up | token bo góc + chữ S/W/G/P | màu theo loại (mục trên) |

- **Chướng ngại** = khối đá **vát 3D** suy sáng/tối từ màu `obstacle` của theme (`shade565`: mép trên-trái
  sáng ×1.35, dưới-phải tối ×0.6) + vết nứt chéo & sạn — không tô phẳng.
- **Sâu liền mạch**: mỗi đốt là thân thụt 1px bo góc; **cầu nối** sang ô kề khi rẽ (`conn_bit`); highlight
  trên / bóng dưới; **đầu** thêm 2 mắt quay theo hướng đi.

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
