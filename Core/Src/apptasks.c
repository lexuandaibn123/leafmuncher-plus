#include "apptasks.h"
#include "main.h"
#include "tim.h"
#include "cmsis_os.h"
#include "game.h"
#include "input.h"
#include "render.h"
#include "gfx.h"
#include "rng.h"
#include "store.h"

/* Các hàm hỗ trợ điều khiển LED báo hiệu */
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
  led_green(0);
  led_red(1);
  __disable_irq();
  for (;;) { }
}

static volatile uint32_t s_ms = 0u;

#define HEARTBEAT_HALF_MS 500u

void timebase_start(void)
{
  if (HAL_TIM_Base_Start_IT(&htim7) != HAL_OK)
  {
    safe_stop();
  }
}

uint32_t app_millis(void)
{
  return s_ms;
}

void timebase_tick_isr(void)
{
  uint32_t ms = s_ms + 1u;
  s_ms = ms;
  if ((ms % HEARTBEAT_HALF_MS) == 0u)
  {
    HAL_GPIO_TogglePin(LD3_GPIO_Port, LD3_Pin);
  }
}

/* Các biến chia sẻ giữa các luồng FreeRTOS */
static GameState        s_game;
static GameState        s_snapshot;
static osMessageQueueId_t s_input_q;
static osMutexId_t        s_snap_mutex;
static osSemaphoreId_t    s_frame_ready;

static osThreadId_t s_input_th, s_game_th, s_render_th;

#define INPUT_PERIOD_MS 20u

static uint32_t ms_to_ticks(uint32_t ms)
{
  return ms * osKernelGetTickFreq() / 1000u;
}

/* Lưu phím bấm vào queue, nếu đầy thì ghi đè sự kiện cũ nhất */
static void input_q_put(InputEvent ev)
{
  if (osMessageQueuePut(s_input_q, &ev, 0u, 0u) != osOK) {
    InputEvent drop;
    (void)osMessageQueueGet(s_input_q, &drop, NULL, 0u);
    (void)osMessageQueuePut(s_input_q, &ev, 0u, 0u);
  }
}

/* Rút toàn bộ input chờ, giữ lại cái mới nhất để xử lý */
static InputEvent input_q_latest(void)
{
  InputEvent ev = { IN_NONE, DIR_UP };
  InputEvent tmp;
  while (osMessageQueueGet(s_input_q, &tmp, NULL, 0u) == osOK) {
    ev = tmp;
  }
  return ev;
}

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
      input_q_put(ev);
    }
  }
}

static void sync_save_flags(void)
{
  s_game.has_save[MODE_LEVEL]   = store_has_save(MODE_LEVEL)   ? 1u : 0u;
  s_game.has_save[MODE_ENDLESS] = store_has_save(MODE_ENDLESS) ? 1u : 0u;
}

static void GameTask(void *argument)
{
  (void)argument;
  uint32_t t = osKernelGetTickCount();
  int s_nav_neutral = 1;
  
  for (;;) {
    t += ms_to_ticks(game_step_ms(&s_game));
    osDelayUntil(t);

    InputEvent in = input_q_latest();
    GameMode mode_before = s_game.mode;
    
    if (s_game.mode == ST_PLAYING) {
      (void)game_step(&s_game, in, game_step_ms(&s_game));
    } else {
      if (in.kind == IN_DIR) {
        if (s_nav_neutral) {
          game_input_ui(&s_game, in);
          s_nav_neutral = 0;
        }
      } else {
        if (in.kind == IN_SELECT) {
          if (s_game.mode == ST_MENU) {
            uint32_t seed = input_entropy() ^ (uint32_t)__HAL_TIM_GET_COUNTER(&htim7);
            rng_seed(&s_game.rng, seed);
          }
          game_input_ui(&s_game, in);
        }
        s_nav_neutral = 1;
      }
    }

    if (s_game.load_request) {
      s_game.load_request = 0u;
      PlayMode m = (PlayMode)s_game.play_mode;
      if (store_has_save(m)) {
        (void)store_load_game(m, &s_game);
        s_game.from_save = 1u;
      }
      sync_save_flags();
    } else if (s_game.save_request) {
      s_game.save_request = 0u;
      (void)store_save_game((PlayMode)s_game.play_mode, &s_game);
      s_game.mode = ST_MENU;
      s_game.menu_sel = 0u;
      sync_save_flags();
    } else if (mode_before == ST_MENU && s_game.mode == ST_PLAYING) {
      s_game.from_save = 0u;
    }

    if (mode_before != ST_GAME_OVER && mode_before != ST_WIN &&
        (s_game.mode == ST_GAME_OVER || s_game.mode == ST_WIN)) {
      bool need_commit = false;
      if (s_game.play_mode == MODE_ENDLESS) {
        uint32_t old_high = store_get()->endless_high;
        store_set_endless_high(s_game.score);
        if (store_get()->endless_high != old_high) {
          render_set_endless_best(store_get()->endless_high);
          need_commit = true;
        }
      }
      
      if (s_game.from_save && store_has_save(s_game.play_mode)) {
        store_clear_save(s_game.play_mode);
      } else if (need_commit) {
        (void)store_commit();
      }
      s_game.from_save = 0u;
      sync_save_flags();
    }

    osMutexAcquire(s_snap_mutex, osWaitForever);
    s_snapshot = s_game;
    osMutexRelease(s_snap_mutex);

    osSemaphoreRelease(s_frame_ready);
  }
}

static void RenderTask(void *argument)
{
  (void)argument;
  GameState local;

  osMutexAcquire(s_snap_mutex, osWaitForever);
  local = s_snapshot;
  osMutexRelease(s_snap_mutex);
  render_force_full(&local);
  gfx_present();

  for (;;) {
    osSemaphoreAcquire(s_frame_ready, osWaitForever);
    osMutexAcquire(s_snap_mutex, osWaitForever);
    local = s_snapshot;
    osMutexRelease(s_snap_mutex);
    render_frame(&local, EV_MOVED);
    gfx_present();
    led_red(local.mode == ST_GAME_OVER);
  }
}

static const osThreadAttr_t s_input_attr  = { .name = "input",  .stack_size = 1024, .priority = osPriorityAboveNormal };
static const osThreadAttr_t s_game_attr   = { .name = "game",   .stack_size = 2048, .priority = osPriorityNormal };
static const osThreadAttr_t s_render_attr = { .name = "render", .stack_size = 4096, .priority = osPriorityBelowNormal };

void tasks_start(void)
{
  s_input_q     = osMessageQueueNew(4u, sizeof(InputEvent), NULL);
  s_snap_mutex  = osMutexNew(NULL);
  s_frame_ready = osSemaphoreNew(1u, 0u, NULL);
  
  if (s_input_q == NULL || s_snap_mutex == NULL || s_frame_ready == NULL) {
    safe_stop();
  }

  uint32_t seed = input_entropy() ^ (uint32_t)__HAL_TIM_GET_COUNTER(&htim7);
  game_init(&s_game, seed);

  store_init();
  render_set_endless_best(store_get()->endless_high);
  sync_save_flags();

  s_snapshot = s_game;

  s_input_th  = osThreadNew(InputTask,  NULL, &s_input_attr);
  s_game_th   = osThreadNew(GameTask,   NULL, &s_game_attr);
  s_render_th = osThreadNew(RenderTask, NULL, &s_render_attr);
  if (s_input_th == NULL || s_game_th == NULL || s_render_th == NULL) {
    safe_stop();
  }
}
