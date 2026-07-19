#ifndef INPUT_H
#define INPUT_H

/* Đọc tín hiệu joystick analog (ADC1+DMA) và nút bấm.
 * Hiệu chỉnh vị trí trung tâm, vùng nhiễu (deadzone), và chống dội nút. */

#include <stdint.h>
#include "game.h"

void       input_init(void);
InputEvent input_poll(void);
uint32_t   input_entropy(void);  /* Dùng nhiễu ADC để khởi tạo seed PRNG */

#endif /* INPUT_H */
