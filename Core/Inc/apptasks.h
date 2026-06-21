#ifndef APPTASKS_H
#define APPTASKS_H

#include <stdint.h>

/* apptasks — vòng đời FreeRTOS: khởi tạo 3 task (Input/Game/Render) + queue/mutex/semaphore,
 * LED + safe-stop. Lớp tích hợp — chạm HAL/CMSIS-RTOS. Quyết định: research §12.
 *
 * LƯU Ý TÊN FILE: đặt là apptasks.c/.h (KHÔNG phải tasks.c) để tránh trùng object với
 * Middlewares/.../FreeRTOS/Source/tasks.c — Makefile sinh object theo notdir nên 2 file cùng
 * tên "tasks.c" sẽ đè nhau (build/tasks.o), gây thiếu symbol FreeRTOS. Module vẫn gọi là "tasks".
 *
 * Skeleton Phase 1 — hiện thực dần ở Phase 2 (T016–T017 đã xong; 3 task ở T020–T025). */

/* ── T016: LED helper + safe-stop ─────────────────────────────────────────────
 * LD3 xanh = PG13, LD4 đỏ = PG14 (STM32F429I-DISC1). Bao HAL_GPIO để lớp trên
 * không phải biết chân cụ thể. */
void led_green(int on);   /* PG13 — bật/tắt LED xanh */
void led_red(int on);     /* PG14 — bật/tắt LED đỏ */

/* safe_stop — gọi khi init phần cứng lỗi: tắt xanh, bật đỏ báo lỗi, chặn ngắt
 * rồi dừng vĩnh viễn (không trở lại). Dùng thay cho việc chạy tiếp khi HW hỏng. */
void safe_stop(void);

/* ── T017: đồng hồ mili-giây thực (TIM7) + heartbeat LED xanh ~1Hz ─────────────
 * TIM7 đã cấu hình ở .ioc (PSC=7199, ARR=9 → ngắt update mỗi 1ms). */
void     timebase_start(void);    /* HAL_TIM_Base_Start_IT(&htim7); lỗi → safe_stop() */
uint32_t app_millis(void);        /* số ms đã trôi từ timebase_start (tăng trong ISR) */
void     timebase_tick_isr(void); /* gọi từ TIM7 ISR (stm32f4xx_it.c): ++ms + heartbeat */

/* ── Vòng đời FreeRTOS (T020+) ───────────────────────────────────────────── */
void tasks_start(void);   /* gọi từ freertos.c vùng USER CODE */

#endif /* APPTASKS_H */
