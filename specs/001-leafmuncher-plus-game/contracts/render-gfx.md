# Contract — Render & GFX (lớp đồ hoạ)

`gfx` = primitive vẽ framebuffer (được phép gọi HAL DMA2D/LTDC). `render` = ánh xạ `GameState`→lời gọi
`gfx` (KHÔNG gọi HAL trực tiếp). Đảm bảo NT IV/V.

## API `gfx.h` (lớp chạm HAL)

```c
void     gfx_init(void);                                  // 2 framebuffer SDRAM + cấu hình LTDC layer
void     gfx_clear(uint16_t color);                       // R2M tô cả màn vào back buffer
void     gfx_fill_rect(int x,int y,int w,int h,uint16_t c);// R2M — vẽ 1 ô / nền HUD
void     gfx_blit(const uint16_t* src,int x,int y,int w,int h); // M2M — sprite/glyph từ flash
void     gfx_blend_rect(int x,int y,int w,int h,uint16_t c,uint8_t a); // M2M_BLEND — phủ mờ PAUSE
void     gfx_text(int x,int y,const char* s,uint16_t fg,uint16_t bg);  // dùng font 8×16 ASCII
void     gfx_present(void);                               // đặt cờ swap; áp dụng tại VSYNC (ngắt LTDC)
uint16_t gfx_rgb565(uint8_t r,uint8_t g,uint8_t b);       // helper màu
```

**Hợp đồng `gfx`**:
- Mọi lệnh vẽ tác động lên **back buffer**; `gfx_present()` mới hoán đổi để LTDC quét.
- Swap CHỈ xảy ra trong cửa sổ VSYNC (ngắt line LTDC) → không xé hình (FR-018/SC-003).
- `gfx_fill_rect` dùng DMA2D R2M; `gfx_blit`/`gfx_text` dùng M2M; chờ DMA2D xong trước khi present.
- Toạ độ theo pixel màn (0..319, 0..239). Vẽ ra ngoài bị cắt (clip), không tràn buffer.

## API `render.h` (thuần điều phối, gọi gfx)

```c
void render_frame(const GameState* gs, GameEvents ev);   // vẽ theo gs->mode
void render_force_full(const GameState* gs);             // vẽ lại toàn khung (khi đổi state)
```

## Hợp đồng render theo trạng thái

| `mode` | Vẽ gì | Chiến lược |
|---|---|---|
| MENU | tiêu đề + danh sách option, mục `menu_sel` sáng | full khi vào; chỉ vẽ lại 2 dòng đổi sáng |
| PLAYING | HUD (score/level/đồng hồ power-up) + sân + sâu + lá + chướng ngại | **dirty-rect**: ô đầu mới, ô đuôi cũ (nếu nhả), ô lá đổi, vùng score |
| PAUSED | giữ khung PLAYING + `gfx_blend_rect` mờ + hộp "PAUSED" | overlay 1 lần |
| GAME_OVER | hộp "GAME OVER" + score cuối + "press to restart"; LED đỏ | full |
| LEVEL_COMPLETE | hộp "LEVEL n COMPLETE" + score | full |
| WIN | hộp "YOU WIN!" + score | full |

## Ánh xạ ô → màu (research §15)

| Nội dung ô | Màu | Ghi chú vẽ |
|---|---|---|
| nền sân | `#0B1A0B` | dùng để xoá ô đuôi cũ |
| thân sâu | `#FFB400` | fill 16×16 |
| đầu sâu | `#E67E00` | fill + 1 chấm mắt 2×2 |
| lá thường | `#2ECC40` | fill |
| lá vàng | `#FFD700` | nhấp nháy (đảo sáng theo nhịp) |
| lá độc | `#B10DC9` | fill |
| chướng ngại | `#555555` | fill |
| power-up SPEED/SLOW/GHOST/PHASE | cyan / lam / trắng / cam-gạch | fill + ký tự gợi nhớ |

## Lưu ý hiện thực — gotchas phần cứng (đã xác minh trên bo M1, xem [research.md §21](../research.md))

- **DMA2D R2M (`gfx_clear`/`gfx_fill_rect`)**: màu nạp vào OCOLR phải là **ARGB8888**, không phải RGB565
  → dùng helper `argb_from_565()`. Truyền thẳng RGB565 sẽ xoay/lệch kênh màu.
- **DMA2D M2M (`gfx_blit`/`gfx_text`)** ở T010: input color mode để RGB565 nếu glyph/sprite là RGB565.
- **Panel ILI9341** phải có `0xB0=0xC2` (RGB Interface Signal Control) trong init, nếu không → sọc ngang.
- **Xoay 90°**: panel native portrait 240×320; `gfx` nhận toạ độ landscape 320×240 và map sang
  (px=ly, py=SCREEN_W−1−lx). Hình chữ nhật landscape → vẫn là chữ nhật trên framebuffer (1 lệnh DMA2D).
- **`gfx_present`**: hiện reload địa chỉ layer tại vblank (`HAL_LTDC_Reload` VERTICAL_BLANKING) — còn xé
  nhẹ; T011 chuyển sang swap trong **ngắt line LTDC** để hết hẳn.

## Bất biến render

- `render` KHÔNG sửa `GameState` (chỉ đọc — nhận `const*`).
- `render` KHÔNG gọi HAL trực tiếp; mọi pixel đi qua `gfx_*`.
- Không vẽ lên buffer đang được LTDC quét (chỉ vẽ back buffer; swap ở VSYNC).
