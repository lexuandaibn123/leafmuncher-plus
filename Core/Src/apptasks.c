#include "apptasks.h"
#include "main.h"        /* LD3/LD4 pin defines, HAL_GPIO */
#include "tim.h"         /* htim7 (timebase + seed RNG) */
#include "cmsis_os.h"    /* CMSIS-RTOS v2 */
#include "game.h"
#include "input.h"
#include "render.h"
#include "gfx.h"
#include "rng.h"          /* T061 — re-seed RNG tại sườn nhấn Start */
#include "store.h"        /* T076 — cài đặt bền vững (theme + điểm cao Vô tận) qua Flash */

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

/* T082: đồng bộ cờ ô lưu vào state (game/render đọc để dựng MENU "Tiếp tục"). */
static void sync_save_flags(void)
{
  s_game.has_save[MODE_LEVEL]   = store_has_save(MODE_LEVEL)   ? 1u : 0u;
  s_game.has_save[MODE_ENDLESS] = store_has_save(MODE_ENDLESS) ? 1u : 0u;
}

/* ── T022: GameTask — nhịp step_ms ổn định ─────────────────────────────────── */
static void GameTask(void *argument)
{
  (void)argument;
  uint32_t t = osKernelGetTickCount();
  int s_nav_neutral = 1;          /* edge-detect joystick menu: 1 = cần ở trung tâm, cho gạt kế */
  for (;;) {
    t += ms_to_ticks(game_step_ms(&s_game));
    osDelayUntil(t);

    InputEvent in = input_q_latest();
    GameMode mode_before = s_game.mode;                      /* T076: bắt chuyển trạng thái để lưu store */
    if (s_game.mode == ST_PLAYING) {
      (void)game_step(&s_game, in, game_step_ms(&s_game));   /* bước chơi (US1); IN_SELECT → PAUSED */
    } else {
      /* Ngoài PLAYING: edge-detect joystick để 1 lần gạt = 1 bước menu (tránh giữ cần nhảy
       * nhiều mục). IN_DIR chỉ xử khi vừa từ trung tâm (s_nav_neutral); nhả cần (IN_NONE) reset. */
      if (in.kind == IN_DIR) {
        if (s_nav_neutral) {
          game_input_ui(&s_game, in);
          s_nav_neutral = 0;
        }
      } else {
        if (in.kind == IN_SELECT) {
          /* T061: re-seed RNG tại sườn nhấn Start (MENU+SELECT) → mỗi ván chuỗi lá khác nhau
           * (TIM7 counter XOR entropy LSB ADC, research §13). game_start giữ state rng đã seed. */
          if (s_game.mode == ST_MENU) {
            uint32_t seed = input_entropy() ^ (uint32_t)__HAL_TIM_GET_COUNTER(&htim7);
            rng_seed(&s_game.rng, seed);
          }
          game_input_ui(&s_game, in);   /* Start · resume · advance · replay (nút edge sẵn ở input) */
        }
        s_nav_neutral = 1;              /* về trung tâm / vừa bấm nút → cho phép gạt kế */
      }
    }

    /* T076/T082: ghi store ở các sườn chuyển trạng thái (erase Flash ~vài trăm ms — ngoài vòng
     * tick gắt). game.c chỉ ĐẶT CỜ (load/save_request); Flash thực thi ở đây (Nguyên tắc II). */
    if (s_game.load_request) {
      /* MENU "Tiếp tục": nạp ô lưu của chế độ đã chọn → state khôi phục (mode=ST_PLAYING). */
      s_game.load_request = 0u;
      PlayMode m = (PlayMode)s_game.play_mode;
      if (store_has_save(m)) {
        (void)store_load_game(m, &s_game);     /* ghi đè cả rng → resume xác định; ô lưu GIỮ */
        s_game.from_save = 1u;                 /* đánh dấu: kết thúc ván này thì xóa ô lưu (FR-031) */
      }
      sync_save_flags();
    } else if (s_game.save_request) {
      /* PAUSED "Lưu & Thoát": lưu snapshot (mang mode PLAYING) rồi về MENU. */
      s_game.save_request = 0u;                /* xóa TRƯỚC khi chụp → bản lưu có cờ = 0 */
      (void)store_save_game((PlayMode)s_game.play_mode, &s_game);
      s_game.mode = ST_MENU;
      s_game.menu_sel = 0u;
      sync_save_flags();
    } else if (mode_before == ST_MENU && s_game.mode == ST_PLAYING) {
      /* Rời menu vào ván MỚI (Start, KHÔNG phải Tiếp tục): không đến từ ô lưu. */
      s_game.from_save = 0u;
      /* Nếu theme đã đổi so với Flash thì lưu lại theme. */
      if (store_get()->theme_id != s_game.theme_id) {
        store_set_theme((ThemeId)s_game.theme_id);
        (void)store_commit();
      }
    }

    /* T082: kết thúc ván (GAME_OVER/WIN) → cập nhật điểm cao Vô tận + xóa ô lưu, GHI FLASH 1 LẦN. */
    if (mode_before != ST_GAME_OVER && mode_before != ST_WIN &&
        (s_game.mode == ST_GAME_OVER || s_game.mode == ST_WIN)) {
      bool need_commit = false;
      if (s_game.play_mode == MODE_ENDLESS) {
        uint32_t old_high = store_get()->endless_high;
        store_set_endless_high(s_game.score);              /* chỉ cập nhật cache (chưa ghi) */
        if (store_get()->endless_high != old_high) {
          render_set_endless_best(store_get()->endless_high);
          need_commit = true;
        }
      }
      /* Xóa ô lưu CHỈ khi ván này khôi phục từ nó (FR-031) — tránh xóa nhầm ô lưu cũ khi
       * người chơi bắt đầu ván MỚI cùng chế độ rồi thua. */
      if (s_game.from_save && store_has_save(s_game.play_mode)) {
        store_clear_save(s_game.play_mode);   /* ghi cả sector (gồm điểm cao mới đã vào cache) */
      } else if (need_commit) {
        (void)store_commit();                 /* không xóa ô lưu → vẫn ghi điểm cao */
      }
      s_game.from_save = 0u;
      sync_save_flags();
    }

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
    led_red(local.mode == ST_GAME_OVER); /* T037: LED đỏ báo Game Over (xanh = heartbeat) */
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

  /* Seed RNG ban đầu = bộ đếm TIM7 (boot) XOR entropy LSB ADC (research §13); ván thật
   * re-seed lại tại sườn nhấn Start trong GameTask (T061). */
  uint32_t seed = input_entropy() ^ (uint32_t)__HAL_TIM_GET_COUNTER(&htim7);
  game_init(&s_game, seed);     /* T061: boot dừng ở MENU (FR-014), KHÔNG vào PLAYING ngay */

  /* T076: nạp cài đặt bền vững từ Flash → theme đã chọn cho render + điểm cao Vô tận cho HUD.
   * Flash trống/hỏng → store_init nạp mặc định (theme=FOREST, high=0), không crash (FR-027). */
  store_init();
  s_game.theme_id = (uint8_t)store_get()->theme_id;
  render_set_endless_best(store_get()->endless_high);
  sync_save_flags();           /* T082: MENU hiển thị "Tiếp tục" nếu Flash có ô lưu hợp lệ */

  s_snapshot = s_game;          /* khung khởi tạo (chưa có task nào chạy) */

  s_input_th  = osThreadNew(InputTask,  NULL, &s_input_attr);
  s_game_th   = osThreadNew(GameTask,   NULL, &s_game_attr);
  s_render_th = osThreadNew(RenderTask, NULL, &s_render_attr);
  if (s_input_th == NULL || s_game_th == NULL || s_render_th == NULL) {
    safe_stop();
  }
}
