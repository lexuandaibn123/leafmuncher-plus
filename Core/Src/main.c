/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "cmsis_os.h"
#include "adc.h"
#include "crc.h"
#include "dma.h"
#include "dma2d.h"
#include "i2c.h"
#include "ltdc.h"
#include "spi.h"
#include "tim.h"
#include "usart.h"
#include "usb_host.h"
#include "gpio.h"
#include "fmc.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "gfx.h"      /* SMOKE TEST — gỡ ở T018 */
#include "input.h"    /* SMOKE TEST T013–T015 — gỡ ở T018 */
#include "apptasks.h" /* T016/T017 — LED helper, safe-stop, đồng hồ ms + heartbeat */
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void MX_FREERTOS_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/* SMOKE TEST (gỡ ở T018): mũi tên tam giác đặc chỉ hướng `d`, tâm (cx,cy).
 * Dùng để soi mắt: bấm nút JOY_SW có làm LỆCH CẦN (đổi hướng ngoài ý muốn) không —
 * nếu hướng nhảy ngay lúc bấm thì cú bấm đã chạm trục analog. */
static void draw_arrow(int cx, int cy, Dir d, uint16_t color)
{
  const int L = 16;   /* chiều dài mũi tên (đáy → đỉnh) */
  const int B = 14;   /* bề rộng đáy */
  for (int i = 0; i <= L; i++) {
    int half = (B / 2) * (L - i) / L;   /* bề rộng thu dần về đỉnh (i=L) */
    switch (d) {
      case DIR_RIGHT: gfx_fill_rect(cx - L/2 + i, cy - half, 1, 2*half + 1, color); break;
      case DIR_LEFT:  gfx_fill_rect(cx + L/2 - i, cy - half, 1, 2*half + 1, color); break;
      case DIR_DOWN:  gfx_fill_rect(cx - half, cy - L/2 + i, 2*half + 1, 1, color); break;
      case DIR_UP:    gfx_fill_rect(cx - half, cy + L/2 - i, 2*half + 1, 1, color); break;
    }
  }
}

static char dir_char(Dir d)
{
  return (d == DIR_RIGHT) ? 'R' : (d == DIR_LEFT) ? 'L' : (d == DIR_UP) ? 'U' : 'D';
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_ADC1_Init();
  MX_CRC_Init();
  MX_DMA2D_Init();
  MX_FMC_Init();
  MX_I2C3_Init();
  MX_LTDC_Init();
  MX_SPI5_Init();
  MX_TIM1_Init();
  MX_USART1_UART_Init();
  MX_TIM7_Init();
  /* USER CODE BEGIN 2 */
  /* SMOKE TEST INPUT (T013–T015, gỡ ở T018): ô vuông di chuyển theo joystick trên panel.
   * Kiểm: ADC DMA + hiệu chỉnh center + deadzone + chiều trục (JOY_INVERT_X/Y) + nút.
   * - Gạt cần → ô chạy theo hướng (RIGHT/LEFT/UP/DOWN).
   * - JOY_SW (PB7) → IN_SELECT: 1 nút duy nhất. Trong demo (đang "chơi") = toggle pause
   *   + LED đỏ (PG14). Trong game thật, ý nghĩa do FSM quyết theo mode. */
  HAL_GPIO_WritePin(GPIOG, LD3_Pin | LD4_Pin, GPIO_PIN_RESET);

  const uint16_t BG    = gfx_rgb565(8, 8, 28);     /* nền xanh đêm   */
  const uint16_t SQ    = gfx_rgb565(60, 220, 90);  /* ô xanh lá      */
  const uint16_t SQ_P  = gfx_rgb565(220, 90, 60);  /* ô cam khi pause */
  const uint16_t WHITE = gfx_rgb565(235, 235, 235);
  const uint16_t CYAN  = gfx_rgb565(40, 200, 220);
  const uint16_t GOLD  = gfx_rgb565(255, 215, 0);
  const uint16_t BOX   = gfx_rgb565(30, 30, 60);   /* nền hộp PAUSED */
  const int STEP = 16;     /* bước di chuyển = 1 ô lưới (CELL) */
  const int SZ   = 20;     /* cạnh ô vuông vẽ */
  int x = (SCREEN_W - SZ) / 2;
  int y = (SCREEN_H - SZ) / 2;

  gfx_init();

  /* MỐC 1: vẽ 1 khung trước input_init (ô đỏ) — nếu thấy ô này thì gfx OK. */
  gfx_clear(BG);
  gfx_fill_rect(x, y, SZ, SZ, gfx_rgb565(230, 40, 40));
  gfx_present();
  HAL_GPIO_WritePin(GPIOG, LD3_Pin, GPIO_PIN_SET);   /* LED xanh: sắp gọi input_init */

  input_init();

  HAL_GPIO_WritePin(GPIOG, LD4_Pin, GPIO_PIN_SET);   /* LED đỏ: input_init đã xong */

  /* T017 — bật đồng hồ ms thực (TIM7) + heartbeat LED xanh ~1Hz.
   * Sau đây LED xanh (PG13) do heartbeat làm chủ; nhịp đập = TIM7 chạy đúng. */
  timebase_start();

  Dir cur = DIR_RIGHT;
  int moving = 0;          /* chỉ đi khi đang gạt cần (IN_DIR gần nhất) */
  int paused = 0;
  int pause_drawn = 0;     /* overlay pause đã vẽ (vẽ 1 lần/lần vào pause) */
  uint32_t tick = 0;

  while (1) {
    InputEvent ev = input_poll();
    if (ev.kind == IN_DIR)  { cur = ev.dir; moving = 1; }
    else if (ev.kind == IN_NONE) { moving = 0; }
    else if (ev.kind == IN_SELECT) { /* 1 nút: trong demo "đang chơi" → toggle pause */
                                     paused = !paused; pause_drawn = 0;
                                     HAL_GPIO_WritePin(GPIOG, LD4_Pin, paused ? GPIO_PIN_SET : GPIO_PIN_RESET); }

    if (paused) {
      /* Overlay tĩnh: blend toàn màn nặng (CPU) → CHỈ vẽ 1 lần để input vẫn nhạy. */
      if (!pause_drawn) {
        gfx_clear(BG);
        gfx_text(6, 4,  "LEAFMUNCHER+", WHITE, BG);
        gfx_text(6, 22, "T010 TEXT / T011 NO-TEAR / T012 BLEND", CYAN, BG);
        gfx_fill_rect(x, y, SZ, SZ, SQ_P);
        draw_arrow(x + SZ / 2, y + SZ / 2, cur, gfx_rgb565(40, 10, 10));  /* mũi tên hướng */
        gfx_blend_rect(0, 0, SCREEN_W, SCREEN_H, gfx_rgb565(0, 0, 0), 150);  /* T012 */
        gfx_fill_rect(118, 104, 84, 32, BOX);
        gfx_text(128, 112, "PAUSED", GOLD, BOX);
        char hud[6] = "DIR ?"; hud[4] = dir_char(cur);
        gfx_text(244, 4, hud, GOLD, BG);   /* hướng chốt tại lúc bấm pause */
        gfx_present();
        pause_drawn = 1;
      }
    } else {
      /* Di chuyển ~120ms/bước khi đang gạt cần. */
      if (moving && (tick % 6u == 0u)) {
        switch (cur) {
          case DIR_RIGHT: x += STEP; break;
          case DIR_LEFT:  x -= STEP; break;
          case DIR_UP:    y -= STEP; break;
          case DIR_DOWN:  y += STEP; break;
        }
        if (x < 0) x = 0; if (x > SCREEN_W - SZ) x = SCREEN_W - SZ;
        if (y < 0) y = 0; if (y > SCREEN_H - SZ) y = SCREEN_H - SZ;
      }
      gfx_clear(BG);
      gfx_text(6, 4,  "LEAFMUNCHER+", WHITE, BG);
      gfx_text(6, 22, "T010 TEXT / T011 NO-TEAR / T012 BLEND", CYAN, BG);
      gfx_fill_rect(x, y, SZ, SZ, SQ);
      draw_arrow(x + SZ / 2, y + SZ / 2, cur, gfx_rgb565(10, 30, 10));  /* mũi tên hướng */
      char hud[6] = "DIR ?"; hud[4] = dir_char(cur);
      gfx_text(244, 4, hud, GOLD, BG);   /* đọc hướng hiện tại — soi lệch khi bấm */
      gfx_present();
    }

    HAL_Delay(20);   /* ~50Hz poll — nhạy cho cả nút lẫn cần */
    tick++;
  }
  /* USER CODE END 2 */

  /* Init scheduler */
  osKernelInitialize();  /* Call init function for freertos objects (in cmsis_os2.c) */
  MX_FREERTOS_Init();

  /* Start scheduler */
  osKernelStart();

  /* We should never get here as control is now taken by the scheduler */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 72;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 3;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM6 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM6)
  {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

  /* USER CODE END Callback 1 */
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
