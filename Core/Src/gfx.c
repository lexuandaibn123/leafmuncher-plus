#include "gfx.h"
#include "game.h"     /* SCREEN_W (320), SCREEN_H (240) — hệ toạ độ landscape */
#include "fmc.h"      /* hsdram1 + HAL_SDRAM_* */
#include "dma2d.h"    /* hdma2d */
#include "ltdc.h"     /* hltdc */
#include "spi.h"      /* hspi5 — gửi lệnh init panel ILI9341 */
#include "main.h"     /* CSX_Pin/PC2, WRX_DCX_Pin/PD13 */
#include "font8x16.h" /* FONT8X16[96][16] — glyph ASCII 0x20..0x7F (T010) */

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

/* T011 — hoán đổi buffer đồng bộ VSYNC qua ngắt line LTDC (hết xé hình).
 * Active display = dòng 4..323 (xem ltdc.c); chốt swap ngay sau dòng active cuối
 * (AccumulatedActiveH=323) → ISR chạy trong vùng blank dọc, reload an toàn. */
#define LTDC_SWAP_LINE  323u
static volatile uint32_t g_swap_addr = 0;   /* địa chỉ buffer chờ đưa ra quét */
static volatile uint8_t  g_swap_req  = 0;   /* 1 = đang chờ ISR áp dụng tại VSYNC */

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

/* Chuỗi init KHỚP CHÍNH XÁC thư viện MaJerle (known-good cho F429-DISCO LTDC/RGB).
 * Then chốt: 0xB0=0xC2 (RGB Interface Signal Control) đưa panel vào chế độ RGB —
 * thiếu nó panel không chốt đúng luồng RGB từ LTDC → sọc ngang. */
static void panel_init(void) {
  HAL_GPIO_WritePin(CSX_GPIO_Port, CSX_Pin, GPIO_PIN_SET);   /* CS nghỉ */

  lcd_cmd_n(0xCA, (const uint8_t[]){0xC3,0x08,0x50}, 3);
  lcd_cmd_n(0xCF, (const uint8_t[]){0x00,0xC1,0x30}, 3);            /* Power control B */
  lcd_cmd_n(0xED, (const uint8_t[]){0x64,0x03,0x12,0x81}, 4);       /* Power on seq */
  lcd_cmd_n(0xE8, (const uint8_t[]){0x85,0x00,0x78}, 3);            /* Driver timing A */
  lcd_cmd_n(0xCB, (const uint8_t[]){0x39,0x2C,0x00,0x34,0x02}, 5);  /* Power control A */
  lcd_cmd_n(0xF7, (const uint8_t[]){0x20}, 1);                      /* Pump ratio */
  lcd_cmd_n(0xEA, (const uint8_t[]){0x00,0x00}, 2);                 /* Driver timing B */
  lcd_cmd_n(0xB1, (const uint8_t[]){0x00,0x1B}, 2);                 /* Frame rate */
  lcd_cmd_n(0xB6, (const uint8_t[]){0x0A,0xA2}, 2);                 /* Display function ctrl */
  lcd_cmd_n(0xC0, (const uint8_t[]){0x10}, 1);                      /* Power control 1 */
  lcd_cmd_n(0xC1, (const uint8_t[]){0x10}, 1);                      /* Power control 2 */
  lcd_cmd_n(0xC5, (const uint8_t[]){0x45,0x15}, 2);                 /* VCOM 1 */
  lcd_cmd_n(0xC7, (const uint8_t[]){0x90}, 1);                      /* VCOM 2 */
  lcd_cmd_n(0x36, (const uint8_t[]){0xC8}, 1);                      /* MADCTL (đúng MaJerle, BGR) */
  lcd_cmd_n(0xF2, (const uint8_t[]){0x00}, 1);                      /* 3Gamma disable */
  lcd_cmd_n(0xB0, (const uint8_t[]){0xC2}, 1);                      /* ★ RGB Interface Signal Control */
  lcd_cmd_n(0xB6, (const uint8_t[]){0x0A,0xA7,0x27,0x04}, 4);       /* Display function ctrl (RGB) */
  lcd_cmd_n(0x2A, (const uint8_t[]){0x00,0x00,0x00,0xEF}, 4);       /* Column 0..239 */
  lcd_cmd_n(0x2B, (const uint8_t[]){0x00,0x00,0x01,0x3F}, 4);       /* Page 0..319 */
  lcd_cmd_n(0xF6, (const uint8_t[]){0x01,0x00,0x06}, 3);            /* Interface control (RGB) */
  lcd_cmd(0x2C);                                                    /* Memory write (mở luồng RGB) */
  lcd_cmd_n(0x26, (const uint8_t[]){0x01}, 1);                      /* Gamma curve */
  lcd_cmd_n(0xE0, (const uint8_t[]){0x0F,0x29,0x24,0x0C,0x0E,0x09,0x4E,0x78,
                                    0x3C,0x09,0x13,0x05,0x17,0x11,0x00}, 15); /* +Gamma */
  lcd_cmd_n(0xE1, (const uint8_t[]){0x00,0x16,0x1B,0x04,0x11,0x07,0x31,0x33,
                                    0x42,0x05,0x0C,0x0A,0x28,0x2F,0x0F}, 15); /* -Gamma */

  lcd_cmd(0x11);           /* SLPOUT — thoát ngủ */
  HAL_Delay(120);
  lcd_cmd(0x29);           /* DISPON — bật hiển thị */
  HAL_Delay(10);
}

/* ⚠️ DMA2D R2M: thanh ghi màu OCOLR phải ở định dạng ARGB8888 (DMA2D tự chuyển
 * sang RGB565 khi ghi ra framebuffer). Truyền thẳng RGB565 → lệch kênh màu. */
static uint32_t argb_from_565(uint16_t c) {
  uint32_t r = (uint32_t)((c >> 11) & 0x1Fu);
  uint32_t g = (uint32_t)((c >> 5)  & 0x3Fu);
  uint32_t b = (uint32_t)( c        & 0x1Fu);
  r = (r << 3) | (r >> 2);   /* 5→8 bit */
  g = (g << 2) | (g >> 4);   /* 6→8 bit */
  b = (b << 3) | (b >> 2);   /* 5→8 bit */
  return 0xFF000000u | (r << 16) | (g << 8) | b;
}

/* ───────────────────────── DMA2D Register-to-Memory fill ─────────────────
 * Tô 1 khối w×h màu RGB565 vào địa chỉ dst (trong back buffer). */
static void dma2d_fill(uint32_t dst, uint16_t color, uint32_t w, uint32_t h, uint32_t line_off) {
  hdma2d.Instance          = DMA2D;
  hdma2d.Init.Mode         = DMA2D_R2M;
  hdma2d.Init.ColorMode    = DMA2D_OUTPUT_RGB565;
  hdma2d.Init.OutputOffset = line_off;
  HAL_DMA2D_Init(&hdma2d);
  HAL_DMA2D_Start(&hdma2d, argb_from_565(color), dst, w, h);
  HAL_DMA2D_PollForTransfer(&hdma2d, 100);
}

/* ───────────────────────── API ──────────────────────────────────────── */

uint16_t gfx_rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return (uint16_t)(((r & 0xF8u) << 8) | ((g & 0xFCu) << 3) | (b >> 3));
}

/* Hạ LTDC pixel clock 25MHz (mặc định CubeMX) → ~6.25MHz cho đúng dải ILI9341 RGB.
 * Regen-safe: đặt ở đây thay vì sửa ltdc.c (bị CubeMX ghi đè). research §21. */
static void ltdc_set_pixel_clock(void) {
  RCC_PeriphCLKInitTypeDef pclk = {0};
  pclk.PeriphClockSelection = RCC_PERIPHCLK_LTDC;
  pclk.PLLSAI.PLLSAIN = 50;
  pclk.PLLSAI.PLLSAIR = 4;
  pclk.PLLSAIDivR     = RCC_PLLSAIDIVR_4;   /* 2MHz×50 /4 /4 = 6.25MHz */
  __HAL_LTDC_DISABLE(&hltdc);
  HAL_RCCEx_PeriphCLKConfig(&pclk);
  __HAL_LTDC_ENABLE(&hltdc);
}

void gfx_init(void) {
  ltdc_set_pixel_clock();  /* ~6.25MHz — panel ILI9341 chốt kịp (research §21) */
  /* Bật FMC SDRAM Read Burst (RBURST) để tăng hiệu suất đọc của LTDC. */
  FMC_Bank5_6->SDCR[0] |= FMC_SDCR1_RBURST;

  panel_init();            /* T009: đưa ILI9341 vào chế độ RGB (0xB0=0xC2) */
  sdram_init_sequence();   /* SDRAM sẵn sàng trước khi ghi framebuffer */
  /* Xoá cả 2 buffer về đen để không hiện rác SDRAM lúc boot. */
  g_back = 1;
  dma2d_fill(g_fb[0], 0x0000, PANEL_W, PANEL_H, 0);
  dma2d_fill(g_fb[1], 0x0000, PANEL_W, PANEL_H, 0);

  /* T011: kích hoạt ngắt line LTDC để hoán đổi buffer đúng cửa sổ VSYNC. */
  HAL_LTDC_ProgramLineEvent(&hltdc, LTDC_SWAP_LINE);
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

/* T011 — gọi từ HAL_LTDC_IRQHandler (LTDC_IRQHandler ở stm32f4xx_it.c) tại dòng
 * LTDC_SWAP_LINE, tức trong vùng blank dọc. Áp địa chỉ buffer mới bằng reload TỨC THỜI
 * (an toàn vì không đang quét vùng hiển thị) rồi tái vũ trang ngắt cho khung kế. */
void HAL_LTDC_LineEventCallback(LTDC_HandleTypeDef *h) {
  if (g_swap_req) {
    HAL_LTDC_SetAddress(h, g_swap_addr, 0);   /* set FBStartAddress + reload tức thời */
    g_swap_req = 0u;
  }
  HAL_LTDC_ProgramLineEvent(h, LTDC_SWAP_LINE);
}

void gfx_present(void) {
  /* Yêu cầu đưa back buffer (vừa vẽ xong) ra quét; ISR line LTDC áp tại VSYNC.
   * Chờ tới khi swap hoàn tất → buffer cũ mới an toàn để vẽ khung sau (hết xé). */
  g_swap_addr = g_fb[g_back];
  g_swap_req  = 1u;

  uint32_t guard = 0;
  while (g_swap_req && ++guard < 2000000u) { /* spin tới VSYNC (kèm chốt an toàn) */ }

  g_back ^= 1u;   /* buffer vừa hiển thị trở thành back buffer mới */
}

/* ───────────────────────── T010/T012 — vẽ pixel có xoay 90° ───────────────
 * DMA2D M2M không xoay được; glyph/blit/blend làm bằng CPU, map landscape→portrait
 * giống gfx_fill_rect (px=ly, py=SCREEN_W−1−lx). Văn bản/overlay nhỏ → chi phí thấp. */

static inline void fb_put_px(int lx, int ly, uint16_t c) {
  if (lx < 0 || lx >= SCREEN_W || ly < 0 || ly >= SCREEN_H) return;
  uint32_t px = (uint32_t)ly;
  uint32_t py = (uint32_t)(SCREEN_W - 1 - lx);
  *(volatile uint16_t *)(g_fb[g_back] + (py * PANEL_W + px) * 2u) = c;
}

static inline uint16_t fb_get_px(int lx, int ly) {
  uint32_t px = (uint32_t)ly;
  uint32_t py = (uint32_t)(SCREEN_W - 1 - lx);
  return *(volatile uint16_t *)(g_fb[g_back] + (py * PANEL_W + px) * 2u);
}

/* Trộn alpha 2 màu RGB565 (a=0 → bg, a=255 → fg). */
static uint16_t blend565(uint16_t bg, uint16_t fg, uint8_t a) {
  uint32_t ia = 255u - a;
  uint32_t br = (bg >> 11) & 0x1Fu, bg6 = (bg >> 5) & 0x3Fu, bb = bg & 0x1Fu;
  uint32_t fr = (fg >> 11) & 0x1Fu, fg6 = (fg >> 5) & 0x3Fu, fb = fg & 0x1Fu;
  uint32_t r = (fr * a + br * ia) / 255u;
  uint32_t g = (fg6 * a + bg6 * ia) / 255u;
  uint32_t b = (fb * a + bb * ia) / 255u;
  return (uint16_t)((r << 11) | (g << 5) | b);
}

void gfx_blit(const uint16_t *src, int x, int y, int w, int h) {
  if (!src || w <= 0 || h <= 0) return;
  for (int i = 0; i < h; i++)
    for (int j = 0; j < w; j++)
      fb_put_px(x + j, y + i, src[i * w + j]);
}

void gfx_blend_rect(int x, int y, int w, int h, uint16_t c, uint8_t a) {
  if (w <= 0 || h <= 0) return;
  if (a == 255u) { gfx_fill_rect(x, y, w, h, c); return; }
  for (int i = 0; i < h; i++) {
    int ly = y + i;
    if (ly < 0 || ly >= SCREEN_H) continue;
    for (int j = 0; j < w; j++) {
      int lx = x + j;
      if (lx < 0 || lx >= SCREEN_W) continue;
      fb_put_px(lx, ly, blend565(fb_get_px(lx, ly), c, a));
    }
  }
}

/* Vẽ chuỗi font 8×16, phóng to nguyên lần `scale` (scale=1 = cỡ mặc định, hành vi T010).
 * Mỗi pixel font → khối scale×scale; nền `bg` vẫn vẽ (đặc) như cũ. */
void gfx_text(int x, int y, const char *s, uint16_t fg, uint16_t bg, int scale) {
  if (!s) return;
  if (scale < 1) scale = 1;
  for (int cx = x; *s; s++, cx += FONT_W * scale) {
    unsigned char ch = (unsigned char)*s;
    if (ch >= 'a' && ch <= 'z') ch = (unsigned char)(ch - 'a' + 'A');  /* gộp về hoa */
    if (ch < 0x20u || ch > 0x7Fu) ch = 0x20u;                          /* ngoài bảng → trống */
    const uint8_t *glyph = FONT8X16[ch - 0x20u];
    for (int r = 0; r < FONT_H; r++) {
      uint8_t row = glyph[r];
      for (int col = 0; col < FONT_W; col++) {
        uint16_t c = (row & (0x80u >> col)) ? fg : bg;
        for (int dy = 0; dy < scale; dy++)
          for (int dx = 0; dx < scale; dx++)
            fb_put_px(cx + col * scale + dx, y + r * scale + dy, c);
      }
    }
  }
}
