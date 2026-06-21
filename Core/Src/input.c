#include "input.h"
#include "adc.h"
#include "main.h"

/* input — T013/T014/T015.
 * Joystick: ADC1 quét 2 kênh (rank1=VRx CH5/PA5, rank2=VRy CH13/PC3) → DMA circular vào s_adc[].
 * Nút: JOY_SW (PB7, pull-up, active-low) → IN_SELECT; B1 (PA0, active-high) → IN_PAUSE.
 * Toàn bộ logic ánh xạ/deadzone/hysteresis/debounce ở đây; game nhận InputEvent thuần (NT II). */

/* ===== Cấu hình ánh xạ (research §10,§11; DEADZONE/DEBOUNCE_MS trong game.h) ===== */
#define AXIS_HYST_NUM   13   /* trục trội phải ≥ 1.3× trục kia → 13/10 (tránh rung chéo) */
#define AXIS_HYST_DEN   10
#define CAL_SAMPLES     16   /* số mẫu/trục khi hiệu chỉnh center */
#define ADC_MID         2048 /* giữa thang 12-bit, dùng làm center dự phòng */

/* Đảo dấu trục nếu đấu dây ngược (xác minh ở demo T018, lật cờ nếu cần). */
#ifndef JOY_INVERT_X
#define JOY_INVERT_X 0
#endif
#ifndef JOY_INVERT_Y
#define JOY_INVERT_Y 1   /* xác minh on-board: VRy đấu ngược → cần lật để lên/xuống đúng */
#endif

/* ===== Trạng thái module ===== */
static volatile uint16_t s_adc[2];                 /* [0]=VRx, [1]=VRy (DMA cập nhật liên tục) */
static uint16_t          s_center_x = ADC_MID;
static uint16_t          s_center_y = ADC_MID;
static uint32_t          s_entropy  = 0u;          /* tích luỹ LSB nhiễu ADC (T015) */

/* Debounce nút theo cạnh-nhấn (press-edge), thời gian thực bằng HAL_GetTick(). */
typedef struct {
  GPIO_TypeDef *port;
  uint16_t      pin;
  uint8_t       pressed_level;  /* mức GPIO khi NHẤN (SET/RESET) */
  uint8_t       last_raw;       /* mức thô lần đọc trước */
  uint8_t       stable;         /* trạng thái đã debounce (1 = đang nhấn) */
  uint32_t      t_change;       /* mốc ms khi mức thô đổi gần nhất */
} Button;

static Button s_sw;   /* JOY_SW → IN_SELECT */
static Button s_b1;   /* B1     → IN_PAUSE  */

/* Hàng chờ cạnh-nhấn 1 ô để không mất sự kiện khi 2 nút cùng đổi 1 lần poll. */
static uint8_t s_pend_select = 0u;
static uint8_t s_pend_pause  = 0u;

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

/* Cập nhật debounce; trả 1 đúng tại sườn nhấn (chuyển nhả→nhấn), không lặp khi giữ. */
static uint8_t btn_press_edge(Button *b)
{
  uint8_t raw = (uint8_t)HAL_GPIO_ReadPin(b->port, b->pin);
  uint32_t now = HAL_GetTick();

  if (raw != b->last_raw) {        /* mức thô vừa đổi → khởi động đếm ổn định */
    b->last_raw = raw;
    b->t_change = now;
    return 0u;
  }
  if ((now - b->t_change) < (uint32_t)DEBOUNCE_MS) {
    return 0u;                     /* chưa ổn định đủ lâu */
  }

  uint8_t now_pressed = (raw == b->pressed_level) ? 1u : 0u;
  uint8_t edge = (now_pressed && !b->stable) ? 1u : 0u;
  b->stable = now_pressed;
  return edge;
}

void input_init(void)
{
  /* Kéo dài sample-time 2 kênh lên 480 chu kỳ (regen-safe, đè cấu hình .ioc).
   * ADC 36MHz: 480+12 cyc ≈ 13.7µs/lần × 2 kênh ≈ 27µs/scan → DMA dư sức phục vụ
   * (hết overrun làm ADC dừng) và ngắt DMA chỉ ~36kHz (không treo TIM6 timebase). */
  ADC_ChannelConfTypeDef ch = {0};
  ch.SamplingTime = ADC_SAMPLETIME_480CYCLES;
  ch.Channel = ADC_CHANNEL_5;  ch.Rank = 1; HAL_ADC_ConfigChannel(&hadc1, &ch);
  ch.Channel = ADC_CHANNEL_13; ch.Rank = 2; HAL_ADC_ConfigChannel(&hadc1, &ch);

  /* Khởi động quét liên tục 2 kênh vào s_adc[] qua DMA (circular, giữ ngắt). */
  HAL_ADC_Start_DMA(&hadc1, (uint32_t *)s_adc, 2);
  HAL_Delay(2);   /* chờ vài scan đầu để buffer có giá trị thực trước khi hiệu chỉnh */

  /* Hiệu chỉnh center: trung bình CAL_SAMPLES mẫu/trục; đồng thời góp entropy. */
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

  btn_init(&s_sw, JOY_SW_GPIO_Port, JOY_SW_Pin, GPIO_PIN_RESET); /* active-low (pull-up) */
  btn_init(&s_b1, B1_GPIO_Port,     B1_Pin,     GPIO_PIN_SET);   /* active-high */
}

InputEvent input_poll(void)
{
  InputEvent ev = { IN_NONE, DIR_UP };

  /* Góp thêm entropy từ LSB ADC mỗi lần poll. */
  s_entropy = (s_entropy << 1) ^ ((uint32_t)(s_adc[0] ^ s_adc[1]) & 0x01u);

  /* Cập nhật debounce 2 nút mỗi poll (đừng bỏ lỡ cạnh); gom vào hàng chờ. */
  if (btn_press_edge(&s_sw)) s_pend_select = 1u;
  if (btn_press_edge(&s_b1)) s_pend_pause  = 1u;

  /* Ưu tiên nút (sự kiện rời rạc) hơn hướng; rút 1 sự kiện/poll. */
  if (s_pend_select) { s_pend_select = 0u; ev.kind = IN_SELECT; return ev; }
  if (s_pend_pause)  { s_pend_pause  = 0u; ev.kind = IN_PAUSE;  return ev; }

  /* Ánh xạ joystick → hướng (deadzone + trục trội ≥1.3× + hysteresis). */
  int dx = (int)s_adc[0] - (int)s_center_x;
  int dy = (int)s_adc[1] - (int)s_center_y;
  int adx = iabs(dx);
  int ady = iabs(dy);

  if (adx < DEADZONE && ady < DEADZONE) {
    return ev;  /* trong deadzone → IN_NONE (đi thẳng, FR-020) */
  }

  if (adx * AXIS_HYST_DEN >= ady * AXIS_HYST_NUM) {        /* X trội rõ */
    if (adx >= DEADZONE) {
      ev.kind = IN_DIR;
#if JOY_INVERT_X
      ev.dir = (dx > 0) ? DIR_LEFT : DIR_RIGHT;
#else
      ev.dir = (dx > 0) ? DIR_RIGHT : DIR_LEFT;
#endif
    }
  } else if (ady * AXIS_HYST_DEN >= adx * AXIS_HYST_NUM) { /* Y trội rõ */
    if (ady >= DEADZONE) {
      ev.kind = IN_DIR;
#if JOY_INVERT_Y
      ev.dir = (dy > 0) ? DIR_DOWN : DIR_UP;
#else
      ev.dir = (dy > 0) ? DIR_UP : DIR_DOWN;
#endif
    }
  }
  /* Vùng nhập nhằng (không trục nào trội ≥1.3×) → giữ IN_NONE (chống rung chéo). */

  return ev;
}

uint32_t input_entropy(void)
{
  return s_entropy;
}
