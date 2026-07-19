#ifndef APPTASKS_H
#define APPTASKS_H

#include <stdint.h>

/* Khởi tạo và quản lý các task của FreeRTOS.
 * LƯU Ý TÊN FILE: đặt là apptasks.c/.h (KHÔNG phải tasks.c) để tránh trùng lặp 
 * file tasks.c của thư viện FreeRTOS khi biên dịch. */

/* Các hàm hỗ trợ cho quá trình khởi tạo phần cứng */
void led_green(int on);
void led_red(int on);

/* Hàm gọi khi khởi tạo phần cứng gặp lỗi, sẽ dừng hệ thống vĩnh viễn */
void safe_stop(void);

/* Quản lý đồng hồ mili-giây và báo hiệu nhịp sống của bo mạch (heartbeat LED) */
void     timebase_start(void);
uint32_t app_millis(void);
void     timebase_tick_isr(void);

/* Khởi tạo các task game cho FreeRTOS */
void tasks_start(void);

#endif /* APPTASKS_H */
