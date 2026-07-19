#include "input.h"
#include "adc.h"
#include "main.h"

#define AXIS_HYST_NUM   13
#define AXIS_HYST_DEN   10
#define CAL_SAMPLES     16
#define ADC_MID         2048

#ifndef JOY_INVERT_X
#define JOY_INVERT_X 0
#endif
#ifndef JOY_INVERT_Y
#define JOY_INVERT_Y 1
#endif

static volatile uint16_t s_adc[2];
static uint16_t          s_center_x = ADC_MID;
static uint16_t          s_center_y = ADC_MID;
static uint32_t          s_entropy  = 0u;

typedef struct {
  GPIO_TypeDef *port;
  uint16_t      pin;
  uint8_t       pressed_level;
  uint8_t       last_raw;
  uint8_t       stable;
  uint32_t      t_change;
} Button;

static Button s_sw;
static uint8_t s_pend_select = 0u;

static int iabs(int v) { return v < 0 ? -v : v; }

static void btn_init(Button *b, GPIO_TypeDef *port, uint16_t pin, uint8_t pressed_level)
{
  b->port = port;
  b->pin = pin;
  b->pressed_level = pressed_level;
  b->last_raw = (uint8_t)HAL_GPIO_ReadPin(port, pin);
  b->stable = 0u;
  b->t_change = HAL_GetTick();
}

static uint8_t btn_press_edge(Button *b)
{
  uint8_t raw = (uint8_t)HAL_GPIO_ReadPin(b->port, b->pin);
  uint32_t now = HAL_GetTick();

  if (raw != b->last_raw) {
    b->last_raw = raw;
    b->t_change = now;
    return 0u;
  }
  if ((now - b->t_change) < (uint32_t)DEBOUNCE_MS) {
    return 0u;
  }

  uint8_t now_pressed = (raw == b->pressed_level) ? 1u : 0u;
  uint8_t edge = (now_pressed && !b->stable) ? 1u : 0u;
  b->stable = now_pressed;
  return edge;
}

void input_init(void)
{
  ADC_ChannelConfTypeDef ch = {0};
  ch.SamplingTime = ADC_SAMPLETIME_480CYCLES;
  ch.Channel = ADC_CHANNEL_5;  ch.Rank = 1; HAL_ADC_ConfigChannel(&hadc1, &ch);
  ch.Channel = ADC_CHANNEL_13; ch.Rank = 2; HAL_ADC_ConfigChannel(&hadc1, &ch);

  HAL_ADC_Start_DMA(&hadc1, (uint32_t *)s_adc, 2);
  HAL_Delay(2);

  uint32_t sx = 0u, sy = 0u;
  for (int i = 0; i < CAL_SAMPLES; i++) {
    uint16_t vx = s_adc[0];
    uint16_t vy = s_adc[1];
    sx += vx;
    sy += vy;
    s_entropy = (s_entropy << 1) ^ ((uint32_t)(vx ^ vy) & 0x0Fu);
    HAL_Delay(2);
  }
  s_center_x = (uint16_t)(sx / CAL_SAMPLES);
  s_center_y = (uint16_t)(sy / CAL_SAMPLES);

  btn_init(&s_sw, JOY_SW_GPIO_Port, JOY_SW_Pin, GPIO_PIN_RESET);
}

InputEvent input_poll(void)
{
  InputEvent ev = { IN_NONE, DIR_UP };

  s_entropy = (s_entropy << 1) ^ ((uint32_t)(s_adc[0] ^ s_adc[1]) & 0x01u);

  if (btn_press_edge(&s_sw)) s_pend_select = 1u;

  if (s_pend_select) { s_pend_select = 0u; ev.kind = IN_SELECT; return ev; }

  int dx = (int)s_adc[0] - (int)s_center_x;
  int dy = (int)s_adc[1] - (int)s_center_y;
  int adx = iabs(dx);
  int ady = iabs(dy);

  if (adx < DEADZONE && ady < DEADZONE) {
    return ev;
  }

  if (adx * AXIS_HYST_DEN >= ady * AXIS_HYST_NUM) {
    if (adx >= DEADZONE) {
      ev.kind = IN_DIR;
#if JOY_INVERT_X
      ev.dir = (dx > 0) ? DIR_LEFT : DIR_RIGHT;
#else
      ev.dir = (dx > 0) ? DIR_RIGHT : DIR_LEFT;
#endif
    }
  } else if (ady * AXIS_HYST_DEN >= adx * AXIS_HYST_NUM) {
    if (ady >= DEADZONE) {
      ev.kind = IN_DIR;
#if JOY_INVERT_Y
      ev.dir = (dy > 0) ? DIR_DOWN : DIR_UP;
#else
      ev.dir = (dy > 0) ? DIR_UP : DIR_DOWN;
#endif
    }
  }

  return ev;
}

uint32_t input_entropy(void)
{
  return s_entropy;
}
