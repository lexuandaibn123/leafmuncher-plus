#ifndef APPTASKS_H
#define APPTASKS_H

/* apptasks — vòng đời FreeRTOS: khởi tạo 3 task (Input/Game/Render) + queue/mutex/semaphore,
 * LED + safe-stop. Lớp tích hợp — chạm HAL/CMSIS-RTOS. Quyết định: research §12.
 *
 * LƯU Ý TÊN FILE: đặt là apptasks.c/.h (KHÔNG phải tasks.c) để tránh trùng object với
 * Middlewares/.../FreeRTOS/Source/tasks.c — Makefile sinh object theo notdir nên 2 file cùng
 * tên "tasks.c" sẽ đè nhau (build/tasks.o), gây thiếu symbol FreeRTOS. Module vẫn gọi là "tasks".
 *
 * Skeleton Phase 1 — hiện thực ở Phase 2 (T016, T020–T025). */

void tasks_start(void);   /* gọi từ freertos.c vùng USER CODE */

#endif /* APPTASKS_H */
