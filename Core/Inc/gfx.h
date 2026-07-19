#ifndef GFX_H
#define GFX_H

/* API vẽ cơ bản lên bộ đệm khung (framebuffer) trong SDRAM.
 * Hỗ trợ vẽ nền, sao chép hình ảnh và xuất chuỗi (font). */

#include <stdint.h>

void     gfx_init(void);
void     gfx_clear(uint16_t color);
void     gfx_fill_rect(int x, int y, int w, int h, uint16_t c);
void     gfx_blit(const uint16_t *src, int x, int y, int w, int h);
void     gfx_blend_rect(int x, int y, int w, int h, uint16_t c, uint8_t a);
void     gfx_text(int x, int y, const char *s, uint16_t fg, uint16_t bg, int scale);
void     gfx_present(void);
uint16_t gfx_rgb565(uint8_t r, uint8_t g, uint8_t b);

#endif /* GFX_H */
