#ifndef FONT8X16_H
#define FONT8X16_H

/* Font bitmap 8x16 (MSB = cột trái), bảng cho ASCII in được 0x20..0x7F. */

#include <stdint.h>

#define FONT_W 8
#define FONT_H 16

extern const uint8_t FONT8X16[96][16];   /* [ch - 0x20][hàng] */

#endif /* FONT8X16_H */
