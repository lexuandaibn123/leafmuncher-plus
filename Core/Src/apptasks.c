#include "apptasks.h"
#include "main.h"   /* LD3_*/LD4_* pin defines, HAL_GPIO */
#include "tim.h"    /* htim7 */

/* apptasks — lớp tích hợp HAL. Phase 2 đang hiện thực dần:
 *   T016 — LED helper + safe-stop
 *   T017 — đồng hồ ms thực (TIM7) + heartbeat LED xanh ~1Hz
 * (3 task Input/Game/Render: T020–T025.) */

/* ── T016: LED helper + safe-stop ─────────────────────────────────────────── */

void led_green(int on)
{
  HAL_GPIO_WritePin(LD3_GPIO_Port, LD3_Pin, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void led_red(int on)
{
  HAL_GPIO_WritePin(LD4_GPIO_Port, LD4_Pin, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void safe_stop(void)
{
  /* Init phần cứng lỗi: tắt xanh, bật đỏ báo lỗi, chặn ngắt rồi dừng vĩnh viễn. */
  led_green(0);
  led_red(1);
  __disable_irq();
  for (;;) { }
}

/* ── T017: đồng hồ ms thực (TIM7) + heartbeat ─────────────────────────────── */

static volatile uint32_t s_ms = 0u;     /* ms đã trôi từ timebase_start (ISR ghi) */

/* Đảo LED xanh mỗi 500ms → chu kỳ 1000ms = nhịp heartbeat ~1Hz. */
#define HEARTBEAT_HALF_MS 500u

void timebase_start(void)
{
  /* Bật ngắt update TIM7 (PSC=7199, ARR=9 → 1ms). NVIC đã bật trong MX_TIM7 MspInit. */
  if (HAL_TIM_Base_Start_IT(&htim7) != HAL_OK)
  {
    safe_stop();
  }
}

uint32_t app_millis(void)
{
  return s_ms;   /* đọc 32-bit aligned là nguyên tử trên Cortex-M4 */
}

void timebase_tick_isr(void)
{
  /* Gọi mỗi 1ms từ TIM7 ISR (stm32f4xx_it.c, sau HAL_TIM_IRQHandler). */
  uint32_t ms = s_ms + 1u;
  s_ms = ms;
  if ((ms % HEARTBEAT_HALF_MS) == 0u)
  {
    HAL_GPIO_TogglePin(LD3_GPIO_Port, LD3_Pin);   /* heartbeat LED xanh */
  }
}
