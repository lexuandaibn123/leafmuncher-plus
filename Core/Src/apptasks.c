#include "apptasks.h"
#include "main.h"        /* LD3/LD4 pin defines, HAL_GPIO */
#include "tim.h"         /* htim7 (timebase + seed RNG) */
#include "cmsis_os.h"    /* CMSIS-RTOS v2 */
#include "game.h"
#include "input.h"
#include "render.h"
#include "gfx.h"

/* apptasks — lớp tích hợp HAL + vòng đời FreeRTOS.
 *   T016 — LED helper + safe-stop
 *   T017 — đồng hồ ms thực (TIM7) + heartbeat LED xanh ~1Hz
 *   T020–T025 — 3 task (Input/Game/Render) + queue/mutex/semaphore (research §12) */

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

/* ── T020: đối tượng đồng bộ + state chia sẻ (research §12) ────────────────── */

static GameState        s_game;        /* state "live" — chỉ GameTask ghi */
static GameState        s_snapshot;    /* bản chia sẻ cho RenderTask (khoá mutex) */
static osMessageQueueId_t s_input_q;   /* InputEvent: sâu 4, ghi đè cũ khi đầy */
static osMutexId_t        s_snap_mutex;/* bảo vệ s_snapshot */
static osSemaphoreId_t    s_frame_ready;/* GameTask báo "có khung mới" cho RenderTask */

static osThreadId_t s_input_th, s_game_th, s_render_th;

#define INPUT_PERIOD_MS 20u            /* 50Hz (INPUT_HZ) */

/* configTICK_RATE_HZ = 1000 → 1 tick = 1ms; quy đổi an toàn nếu sau này đổi. */
static uint32_t ms_to_ticks(uint32_t ms)
{
  return ms * osKernelGetTickFreq() / 1000u;
}

/* Đẩy vào queue, đầy thì bỏ phần tử cũ nhất rồi đẩy (ghi đè — research §12). */
static void input_q_put(InputEvent ev)
{
  if (osMessageQueuePut(s_input_q, &ev, 0u, 0u) != osOK) {
    InputEvent drop;
    (void)osMessageQueueGet(s_input_q, &drop, NULL, 0u);
    (void)osMessageQueuePut(s_input_q, &ev, 0u, 0u);
  }
}

/* Rút toàn bộ input chờ, giữ cái mới nhất (GameTask chỉ áp 1 lệnh/bước). */
static InputEvent input_q_latest(void)
{
  InputEvent ev = { IN_NONE, DIR_UP };
  InputEvent tmp;
  while (osMessageQueueGet(s_input_q, &tmp, NULL, 0u) == osOK) {
    ev = tmp;
  }
  return ev;
}

/* ── T021: InputTask @50Hz ─────────────────────────────────────────────────── */
static void InputTask(void *argument)
{
  (void)argument;
  uint32_t t = osKernelGetTickCount();
  const uint32_t period = ms_to_ticks(INPUT_PERIOD_MS);
  for (;;) {
    t += period;
    osDelayUntil(t);
    InputEvent ev = input_poll();
    if (ev.kind != IN_NONE) {
      input_q_put(ev);          /* chỉ đẩy sự kiện có nghĩa (tránh ngập IN_NONE) */
    }
  }
}

/* ── T022: GameTask — nhịp step_ms ổn định ─────────────────────────────────── */
static void GameTask(void *argument)
{
  (void)argument;
  uint32_t t = osKernelGetTickCount();
  for (;;) {
    t += ms_to_ticks(game_step_ms(&s_game));
    osDelayUntil(t);

    InputEvent in = input_q_latest();
    (void)game_step(&s_game, in, game_step_ms(&s_game));

    osMutexAcquire(s_snap_mutex, osWaitForever);
    s_snapshot = s_game;                 /* khoá tối thiểu: chỉ copy */
    osMutexRelease(s_snap_mutex);

    osSemaphoreRelease(s_frame_ready);   /* báo RenderTask */
  }
}

/* ── T023: RenderTask — chờ frame-ready, copy snapshot, vẽ ─────────────────── */
static void RenderTask(void *argument)
{
  (void)argument;
  GameState local;

  osMutexAcquire(s_snap_mutex, osWaitForever);
  local = s_snapshot;
  osMutexRelease(s_snap_mutex);
  render_force_full(&local);             /* khung đầu tiên */
  gfx_present();

  for (;;) {
    osSemaphoreAcquire(s_frame_ready, osWaitForever);
    osMutexAcquire(s_snap_mutex, osWaitForever);
    local = s_snapshot;                  /* copy nhanh dưới mutex */
    osMutexRelease(s_snap_mutex);
    render_frame(&local, EV_MOVED);
    gfx_present();
  }
}

/* ── T025 (logic): tạo đồng bộ + seed RNG + 3 task. Gọi từ freertos.c. ─────── */

static const osThreadAttr_t s_input_attr  = { .name = "input",  .stack_size = 1024, .priority = osPriorityAboveNormal };
static const osThreadAttr_t s_game_attr   = { .name = "game",   .stack_size = 2048, .priority = osPriorityNormal };
static const osThreadAttr_t s_render_attr = { .name = "render", .stack_size = 4096, .priority = osPriorityBelowNormal };

void tasks_start(void)
{
  s_input_q     = osMessageQueueNew(4u, sizeof(InputEvent), NULL);
  s_snap_mutex  = osMutexNew(NULL);
  s_frame_ready = osSemaphoreNew(1u, 0u, NULL);   /* nhị phân: có/không khung mới */
  if (s_input_q == NULL || s_snap_mutex == NULL || s_frame_ready == NULL) {
    safe_stop();
  }

  /* Seed RNG = bộ đếm TIM7 (thời điểm boot) XOR entropy LSB ADC (research §13). */
  uint32_t seed = input_entropy() ^ (uint32_t)__HAL_TIM_GET_COUNTER(&htim7);
  game_init(&s_game, seed);
  game_start(&s_game);          /* M2 demo: vào PLAYING ngay (MENU/SELECT ở T057/US4) */
  s_snapshot = s_game;          /* khung khởi tạo (chưa có task nào chạy) */

  s_input_th  = osThreadNew(InputTask,  NULL, &s_input_attr);
  s_game_th   = osThreadNew(GameTask,   NULL, &s_game_attr);
  s_render_th = osThreadNew(RenderTask, NULL, &s_render_attr);
  if (s_input_th == NULL || s_game_th == NULL || s_render_th == NULL) {
    safe_stop();
  }
}
