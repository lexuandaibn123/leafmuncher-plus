#ifndef INPUT_H
#define INPUT_H

/* input — đọc joystick analog (ADC1+DMA) + nút (PB7 JOY_SW, PA0 B1) → InputEvent.
 * Lớp driver — chạm HAL. Hiệu chỉnh center + deadzone + trục trội/hysteresis + debounce.
 * Hợp đồng: contracts/input.md. Hiện thực T013–T015. */

#include <stdint.h>
#include "game.h"   /* InputEvent, Dir */

void       input_init(void);     /* bắt đầu ADC1 DMA + hiệu chỉnh center (16 mẫu/trục) */
InputEvent input_poll(void);     /* gọi @50Hz trong InputTask → sự kiện mới nhất */
uint32_t   input_entropy(void);  /* LSB nhiễu ADC tích luỹ — góp seed RNG (research §13) */

#endif /* INPUT_H */
