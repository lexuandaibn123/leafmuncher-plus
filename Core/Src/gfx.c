#include "gfx.h"
#include "game.h"     /* SCREEN_W (320), SCREEN_H (240) — hệ toạ độ landscape */
#include "fmc.h"      /* hsdram1 + HAL_SDRAM_* */
#include "dma2d.h"    /* hdma2d */
#include "ltdc.h"     /* hltdc */
#include "spi.h"      /* hspi5 — gửi lệnh init panel ILI9341 */
#include "main.h"     /* CSX_Pin/PC2, WRX_DCX_Pin/PD13 */

/* ───────────────────────── Cấu hình framebuffer ─────────────────────────
 * Panel ILI9341 native PORTRAIT: 240 (W) × 320 (H), RGB565 (2 byte/pixel).
 * 1 framebuffer = 240×320×2 = 153600 byte = 0x25800.
 * Hai buffer trong SDRAM (8MB @ 0xD0000000) cho double-buffer:
 *   FB0 = 0xD0000000, FB1 = 0xD0025800. */
#define PANEL_W   240u
#define PANEL_H   320u
#define FB_BYTES  (PANEL_W * PANEL_H * 2u)   /* 0x25800 */
#define FB0_ADDR  0xD0000000u
#define FB1_ADDR  (FB0_ADDR + FB_BYTES)      /* 0xD0025800 */

static uint32_t g_fb[2]   = { FB0_ADDR, FB1_ADDR };
static uint8_t  g_back    = 1;   /* LTDC quét FB0 lúc đầu → vẽ vào FB1 */

/* ───────────────────────── SDRAM init sequence (IS42S16400J) ─────────────
 * MX_FMC_Init() chỉ cấu hình controller; thiết bị SDRAM cần chuỗi lệnh khởi tạo
 * (RM0090 §37 / datasheet IS42S16400J) trước khi dùng vùng nhớ, nếu không → màn đen.
 * SDCLK = HCLK/2 = 72MHz/2 = 36MHz. */

/* Mode register: burst length 1, sequential, CAS=3, standard, single write-burst. */
#define SDRAM_MR_BURST_LEN_1     ((uint16_t)0x0000)
#define SDRAM_MR_BURST_SEQ       ((uint16_t)0x0000)
#define SDRAM_MR_CAS_LATENCY_3   ((uint16_t)0x0030)
#define SDRAM_MR_OP_STANDARD     ((uint16_t)0x0000)
#define SDRAM_MR_WB_SINGLE       ((uint16_t)0x0200)

/* Refresh count = (15.625µs × 36MHz) − 20 ≈ 542 (4096 hàng, chu kỳ 64ms). */
#define SDRAM_REFRESH_COUNT      542u

static void sdram_init_sequence(void) {
  FMC_SDRAM_CommandTypeDef cmd = {0};
  const uint32_t timeout = 0xFFFF;

  /* 1) Clock Configuration Enable */
  cmd.CommandMode            = FMC_SDRAM_CMD_CLK_ENABLE;
  cmd.CommandTarget          = FMC_SDRAM_CMD_TARGET_BANK2;
  cmd.AutoRefreshNumber      = 1;
  cmd.ModeRegisterDefinition = 0;
  HAL_SDRAM_SendCommand(&hsdram1, &cmd, timeout);

  /* 2) Delay ≥ 100µs (sau khi cấp clock). 1ms cho dư. */
  HAL_Delay(1);

  /* 3) Precharge All */
  cmd.CommandMode = FMC_SDRAM_CMD_PALL;
  HAL_SDRAM_SendCommand(&hsdram1, &cmd, timeout);

  /* 4) Auto-refresh ×8 */
  cmd.CommandMode       = FMC_SDRAM_CMD_AUTOREFRESH_MODE;
  cmd.AutoRefreshNumber = 8;
  HAL_SDRAM_SendCommand(&hsdram1, &cmd, timeout);

  /* 5) Load Mode Register */
  cmd.CommandMode            = FMC_SDRAM_CMD_LOAD_MODE;
  cmd.AutoRefreshNumber      = 1;
  cmd.ModeRegisterDefinition = SDRAM_MR_BURST_LEN_1 | SDRAM_MR_BURST_SEQ |
                               SDRAM_MR_CAS_LATENCY_3 | SDRAM_MR_OP_STANDARD |
                               SDRAM_MR_WB_SINGLE;
  HAL_SDRAM_SendCommand(&hsdram1, &cmd, timeout);

  /* 6) Đặt tốc độ auto-refresh */
  HAL_SDRAM_ProgramRefreshRate(&hsdram1, SDRAM_REFRESH_COUNT);
}

/* ───────────────────────── Panel ILI9341 bring-up qua SPI5 (T009) ────────
 * LTDC chỉ quét tín hiệu RGB song song ra panel; panel phải được đưa vào chế độ
 * RGB-interface qua SPI5 trước, nếu KHÔNG → màn ĐEN. Chuỗi lệnh theo BSP
 * STM32F429I-DISCO (ili9341.c). CSX=PC2 (active-low), WRX_DCX=PD13 (0=lệnh,1=data). */

static void lcd_cmd(uint8_t cmd) {
  HAL_GPIO_WritePin(WRX_DCX_GPIO_Port, WRX_DCX_Pin, GPIO_PIN_RESET);  /* DCX=0 → lệnh */
  HAL_GPIO_WritePin(CSX_GPIO_Port, CSX_Pin, GPIO_PIN_RESET);
  HAL_SPI_Transmit(&hspi5, &cmd, 1, 100);
  HAL_GPIO_WritePin(CSX_GPIO_Port, CSX_Pin, GPIO_PIN_SET);
}

static void lcd_data(uint8_t d) {
  HAL_GPIO_WritePin(WRX_DCX_GPIO_Port, WRX_DCX_Pin, GPIO_PIN_SET);    /* DCX=1 → data */
  HAL_GPIO_WritePin(CSX_GPIO_Port, CSX_Pin, GPIO_PIN_RESET);
  HAL_SPI_Transmit(&hspi5, &d, 1, 100);
  HAL_GPIO_WritePin(CSX_GPIO_Port, CSX_Pin, GPIO_PIN_SET);
}

/* Gửi 1 lệnh kèm n byte tham số (n có thể = 0). */
static void lcd_cmd_n(uint8_t cmd, const uint8_t *args, uint8_t n) {
  lcd_cmd(cmd);
  for (uint8_t i = 0; i < n; i++) lcd_data(args[i]);
}

static void panel_init(void) {
  HAL_GPIO_WritePin(CSX_GPIO_Port, CSX_Pin, GPIO_PIN_SET);   /* CS nghỉ */

  lcd_cmd(0x01);            /* SWRESET — reset mềm */
  HAL_Delay(50);

  lcd_cmd_n(0xCB, (const uint8_t[]){0x39,0x2C,0x00,0x34,0x02}, 5);  /* Power control A */
  lcd_cmd_n(0xCF, (const uint8_t[]){0x00,0xC1,0x30}, 3);            /* Power control B */
  lcd_cmd_n(0xE8, (const uint8_t[]){0x85,0x00,0x78}, 3);            /* Driver timing A */
  lcd_cmd_n(0xEA, (const uint8_t[]){0x00,0x00}, 2);                 /* Driver timing B */
  lcd_cmd_n(0xED, (const uint8_t[]){0x64,0x03,0x12,0x81}, 4);       /* Power on seq */
  lcd_cmd_n(0xF7, (const uint8_t[]){0x20}, 1);                      /* Pump ratio */
  lcd_cmd_n(0xC0, (const uint8_t[]){0x23}, 1);                      /* Power control 1 */
  lcd_cmd_n(0xC1, (const uint8_t[]){0x10}, 1);                      /* Power control 2 */
  lcd_cmd_n(0xC5, (const uint8_t[]){0x3E,0x28}, 2);                 /* VCOM 1 */
  lcd_cmd_n(0xC7, (const uint8_t[]){0x86}, 1);                      /* VCOM 2 */
  lcd_cmd_n(0x36, (const uint8_t[]){0x48}, 1);                      /* MADCTL (BGR, mặc định portrait) */
  lcd_cmd_n(0x3A, (const uint8_t[]){0x55}, 1);                      /* COLMOD = RGB565 16-bit */
  lcd_cmd_n(0xB1, (const uint8_t[]){0x00,0x18}, 2);                 /* Frame rate */
  lcd_cmd_n(0xB6, (const uint8_t[]){0x08,0x82,0x27}, 3);            /* Display function ctrl */
  lcd_cmd_n(0xF2, (const uint8_t[]){0x00}, 1);                      /* 3Gamma disable */
  lcd_cmd_n(0x26, (const uint8_t[]){0x01}, 1);                      /* Gamma curve */
  lcd_cmd_n(0xE0, (const uint8_t[]){0x0F,0x31,0x2B,0x0C,0x0E,0x08,0x4E,0xF1,
                                    0x37,0x07,0x10,0x03,0x0E,0x09,0x00}, 15); /* +Gamma */
  lcd_cmd_n(0xE1, (const uint8_t[]){0x00,0x0E,0x14,0x03,0x11,0x07,0x31,0xC1,
                                    0x48,0x08,0x0F,0x0C,0x31,0x36,0x0F}, 15); /* -Gamma */
  /* Interface control: nhận dữ liệu ảnh từ RGB interface (LTDC) thay vì GRAM nội. */
  lcd_cmd_n(0xF6, (const uint8_t[]){0x01,0x00,0x06}, 3);

  lcd_cmd(0x11);           /* SLPOUT — thoát ngủ */
  HAL_Delay(120);
  lcd_cmd(0x29);           /* DISPON — bật hiển thị */
  HAL_Delay(10);
}

/* ───────────────────────── DMA2D Register-to-Memory fill ─────────────────
 * Tô 1 khối w×h màu RGB565 vào địa chỉ dst (trong back buffer). */
static void dma2d_fill(uint32_t dst, uint16_t color, uint32_t w, uint32_t h, uint32_t line_off) {
  hdma2d.Instance          = DMA2D;
  hdma2d.Init.Mode         = DMA2D_R2M;
  hdma2d.Init.ColorMode    = DMA2D_OUTPUT_RGB565;
  hdma2d.Init.OutputOffset = line_off;
  HAL_DMA2D_Init(&hdma2d);
  HAL_DMA2D_Start(&hdma2d, (uint32_t)color, dst, w, h);
  HAL_DMA2D_PollForTransfer(&hdma2d, 100);
}

/* ───────────────────────── API ──────────────────────────────────────── */

uint16_t gfx_rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return (uint16_t)(((r & 0xF8u) << 8) | ((g & 0xFCu) << 3) | (b >> 3));
}

void gfx_init(void) {
  panel_init();            /* T009: đưa ILI9341 vào chế độ RGB (không có → màn đen) */
  sdram_init_sequence();   /* SDRAM sẵn sàng trước khi ghi framebuffer */
  /* Xoá cả 2 buffer về đen để không hiện rác SDRAM lúc boot. */
  g_back = 1;
  dma2d_fill(g_fb[0], 0x0000, PANEL_W, PANEL_H, 0);
  dma2d_fill(g_fb[1], 0x0000, PANEL_W, PANEL_H, 0);
}

void gfx_clear(uint16_t color) {
  dma2d_fill(g_fb[g_back], color, PANEL_W, PANEL_H, 0);
}

void gfx_fill_rect(int x, int y, int w, int h, uint16_t c) {
  /* Clip về khung landscape [0,SCREEN_W) × [0,SCREEN_H). */
  if (w <= 0 || h <= 0) return;
  if (x < 0) { w += x; x = 0; }
  if (y < 0) { h += y; y = 0; }
  if (x >= SCREEN_W || y >= SCREEN_H) return;
  if (x + w > SCREEN_W) w = SCREEN_W - x;
  if (y + h > SCREEN_H) h = SCREEN_H - y;
  if (w <= 0 || h <= 0) return;

  /* Xoay 90° landscape→portrait: px = ly, py = (SCREEN_W-1) - lx.
   * Hình chữ nhật landscape (x,y,w,h) → chữ nhật portrait:
   *   PX0 = y, PY0 = SCREEN_W - x - w, rộng = h, cao = w. */
  uint32_t px0 = (uint32_t)y;
  uint32_t py0 = (uint32_t)(SCREEN_W - x - w);
  uint32_t rw  = (uint32_t)h;   /* bề rộng theo trục x vật lý */
  uint32_t rh  = (uint32_t)w;   /* chiều cao theo trục y vật lý */

  uint32_t dst = g_fb[g_back] + (py0 * PANEL_W + px0) * 2u;
  dma2d_fill(dst, c, rw, rh, PANEL_W - rw);
}

void gfx_present(void) {
  /* T008 bản cơ bản: đổi địa chỉ layer rồi reload tại khoảng vblank dọc (giảm xé hình).
   * ⚠️ T011 sẽ nâng cấp: đặt cờ swap, áp dụng trong ngắt line LTDC (đồng bộ VSYNC chính xác). */
  HAL_LTDC_SetAddress_NoReload(&hltdc, g_fb[g_back], 0);
  HAL_LTDC_Reload(&hltdc, LTDC_RELOAD_VERTICAL_BLANKING);
  g_back ^= 1u;   /* buffer vừa hiển thị trở thành back buffer mới */
}
