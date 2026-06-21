#ifndef FONT8X16_H
#define FONT8X16_H

/* Font bitmap 8×16 (MSB = cột trái), bảng cho ASCII in được 0x20..0x7F.
 * Sinh tự động từ tools/genfont.c (glyph dạng ASCII-art). Dùng bởi gfx_text (T010). */

#include <stdint.h>

#define FONT_W 8
#define FONT_H 16

extern const uint8_t FONT8X16[96][16];   /* [ch - 0x20][hàng] */

#endif /* FONT8X16_H */
