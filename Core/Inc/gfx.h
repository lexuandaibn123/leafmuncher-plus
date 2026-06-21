#ifndef GFX_H
#define GFX_H

/* gfx — primitive vẽ trên framebuffer SDRAM (DMA2D fill/blit/blend, double-buffer, swap VSYNC).
 * Lớp driver — CHẠM HAL (DMA2D/LTDC/FMC). Hợp đồng: contracts/render-gfx.md.
 *
 * Toạ độ API là LANDSCAPE 320×240 (SCREEN_W×SCREEN_H, gốc trái-trên), trùng hệ toạ độ game.
 * Panel ILI9341 native là PORTRAIT 240×320 → gfx xoay 90° trong phần mềm khi map sang framebuffer.
 *
 * T008: gfx_init/clear/fill_rect/rgb565 (+ init SDRAM + present cơ bản).
 * T010: gfx_blit/gfx_text · T011: gfx_present đồng bộ VSYNC · T012: gfx_blend_rect. */

#include <stdint.h>

void     gfx_init(void);                                          /* SDRAM init seq + 2 framebuffer + LTDC layer */
void     gfx_clear(uint16_t color);                               /* R2M tô cả back buffer */
void     gfx_fill_rect(int x, int y, int w, int h, uint16_t c);   /* R2M — toạ độ landscape, tự clip */
void     gfx_blit(const uint16_t *src, int x, int y, int w, int h);            /* M2M (T010) */
void     gfx_blend_rect(int x, int y, int w, int h, uint16_t c, uint8_t a);    /* M2M_BLEND (T012) */
void     gfx_text(int x, int y, const char *s, uint16_t fg, uint16_t bg);      /* font 8×16 (T010) */
void     gfx_present(void);                                       /* hoán đổi buffer (T011: tại VSYNC) */
uint16_t gfx_rgb565(uint8_t r, uint8_t g, uint8_t b);             /* helper màu */

#endif /* GFX_H */
