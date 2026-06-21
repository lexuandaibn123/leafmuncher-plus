# Hướng dẫn tạo base project bằng STM32CubeMX

> Tài liệu này ghi lại **chính xác các bước cấu hình + sinh code** cho LeafMuncher+
> trên **STM32F429I-DISC1**, xuất project kiểu **Makefile**.
> Làm theo đúng thứ tự để tái tạo `.ioc` từ đầu khi cần.

- **Bo:** STM32F429I-DISC1 (STM32F429ZI)
- **Toolchain xuất ra:** Makefile (build bằng arm-gcc + make bundled trong CubeIDE)
- **RTOS:** FreeRTOS (CMSIS-RTOS v2)
- **Đồ hoạ:** LTDC + SDRAM (framebuffer) + DMA2D

---

## Bước 1 — Tạo project từ board

1. Mở **STM32CubeMX** → **File ▸ New Project** → tab **Board Selector**.
2. Gõ `STM32F429I-DISC1` → chọn bo → **Start Project**.
3. Hộp thoại *"Initialize all peripherals with their default Mode?"* → **Yes**.
   → Tự cấu hình **LTDC, FMC/SDRAM, SPI5↔ILI9341, I2C touch, LED PG13/PG14, nút B1 PA0, clock 180 MHz**.

> Bo mặc định cũng bật **USB Host (OTG_HS)** — không dùng cho game, có thể tắt
> (Connectivity ▸ USB_OTG_HS = Disable, và Middleware ▸ USB_HOST = Disable) cho nhẹ. Không bắt buộc.

## Bước 2 — Bật FreeRTOS

1. **Pinout & Configuration ▸ Middleware and Software Packs ▸ FREERTOS**.
2. **Interface = CMSIS_V2**.
3. CubeMX cảnh báo HAL timebase trùng SysTick → vào **System Core ▸ SYS ▸ Timebase Source = TIM6**.

## Bước 3 — ADC cho joystick (2 trục) + DMA

Mục tiêu: đọc liên tục 2 trục joystick, sạch và không chặn (hợp FreeRTOS).

1. Trong **Pinout view** (sơ đồ chip): click **PA5** → chọn **`ADC1_IN5`**; click **PC3** → chọn **`ADC1_IN13`**.
2. Vào **Analog ▸ ADC1 ▸ Parameter Settings**:
   - **Scan Conversion Mode = Enabled**
   - **Number Of Conversion = 2**
   - **Rank 1 → Channel 5** (PA5 = VRx)
   - **Rank 2 → Channel 13** (PC3 = VRy)
   - *(Không dùng Injected channels.)*
3. ⚠️ **Thêm DMA TRƯỚC** (nếu không, mục "DMA Continuous Requests" sẽ bị xám):
   tab **ADC1 ▸ DMA Settings** → **Add** → DMA Request = **`ADC1`** → **Mode = Circular**,
   **Data Width: Peripheral = Half Word, Memory = Half Word** (đọc vào buffer `uint16_t[2]`).
4. **Quay lại Parameter Settings** → giờ mới bật được:
   - **Continuous Conversion Mode = Enabled**
   - **DMA Continuous Requests = Enabled**

> Trong code chỉ cần gọi `HAL_ADC_Start_DMA(&hadc1, buf, 2)` một lần;
> `buf[0]` = VRx, `buf[1]` = VRy luôn cập nhật.

## Bước 4 — Nút joystick (PB7) — phải là INPUT

> ⚠️ Lỗi hay gặp: PB7 bị để mặc định **Output** → không đọc được nút.

1. Trong **Pinout view**: click **PB7** → chọn **`GPIO_Input`** (KHÔNG để Output).
2. **System Core ▸ GPIO** → click dòng **PB7**:
   - **GPIO Pull-up/Pull-down = Pull-up**
   - **User Label = `JOY_SW`**
3. (Tuỳ chọn, gọn code) đặt **User Label**: PA5 = `JOY_VRX`, PC3 = `JOY_VRY`.

> Cách kiểm tra đúng: mở `Core/Src/gpio.c`, dòng cấu hình `JOY_SW_Pin` phải là
> `GPIO_InitStruct.Mode = GPIO_MODE_INPUT;` (không phải `GPIO_MODE_OUTPUT_PP`).

## Bước 5 — Chân đã chốt

| Tín hiệu joystick | Chân STM32 | Cấu hình | Ghi chú |
|---|---|---|---|
| VRx (trục X) | **PA5** | `ADC1_IN5` (Rank 1) | label `JOY_VRX` |
| VRy (trục Y) | **PC3** | `ADC1_IN13` (Rank 2) | label `JOY_VRY` |
| SW (nút) | **PB7** | `GPIO_Input` + Pull-up | label `JOY_SW` |
| VCC | 3V3 | — | |
| GND | GND | — | |

Chân đã bị bo dùng — **tránh**: PA7 (`ACP_RST`), PC4 (`OTG_FS`), PC1 (gyro NCS), PC2 (LCD CSX), PA1/PA2 (gyro INT).
Dự phòng nếu kẹt: VRy → **PC5 (IN15)**; SW → **PB4** hoặc **PC13**.

## Bước 6 — Bật TIM7 (đồng hồ ms thực + nhịp heartbeat) — mở khoá T017

`.ioc` mặc định chỉ có **TIM1 + TIM6** (TIM6 là HAL timebase). T017 cần **TIM7** làm đồng hồ
mili-giây thực cho đếm ngược lá vàng/power-up + nhịp **heartbeat LED ~1Hz**. Đây là peripheral
**Timer + Interrupt** để chấm điểm theo constitution §2.

1. Panel trái → **Timers ▸ TIM7**.
2. Tích **Activated** (TIM7 là basic timer nên chỉ có 1 ô).
3. Tab **Parameter Settings ▸ Counter Settings**:
   - **Prescaler (PSC) = `7199`**
   - **Counter Period (ARR) = `9`**
   > TIM7 clock = APB1 timer clock. Ở project này HCLK **72 MHz** → APB1 timer 72 MHz.
   > 72 MHz / (7199+1) = 10 kHz; / (9+1) = **1000 Hz = ngắt mỗi 1 ms** (đồng hồ ms).
   > ⚠️ Nếu nâng HCLK lên 180 MHz sau này thì tính lại PSC (vd APB1 timer 90 MHz → PSC=8999).
4. Tab **NVIC Settings** (vẫn trong TIM7) → tích **TIM7 global interrupt** ✅.
5. **Project Manager ▸ Code Generator** → giữ *Generate peripheral initialization as a pair of
   .c/.h files* (đã đặt từ trước).
6. **Generate Code**.

> Sinh ra: `MX_TIM7_Init()` (PSC=7199, ARR=9) + `TIM7_IRQHandler()` (trong `stm32f4xx_it.c`) +
> NVIC bật. Logic ms-clock/heartbeat đặt ở vùng USER CODE (`apptasks.c` + ISR) — đó là **T017**,
> KHÔNG sửa file init sinh ra. Sau khi gen nhớ chạy lại **T005** (thêm 7 source vào `C_SOURCES`).

## Bước 7 — Project Manager → xuất Makefile

1. Tab **Project Manager ▸ Project**:
   - **Project Name:** `leafmuncher-plus`
   - **Project Location:** `d:\` (sinh đúng vào `d:\leafmuncher-plus`)
   - **Toolchain / IDE:** **Makefile**
2. Tab **Code Generator**:
   - ✅ *Generate peripheral initialization as a pair of .c/.h files per peripheral*
   - ✅ *Keep User Code when re-generating* (giữ code trong vùng `USER CODE BEGIN/END`)
3. Bấm **GENERATE CODE**.

> Khi gen lại: chỉ code nằm giữa `/* USER CODE BEGIN x */` và `/* USER CODE END x */`
> được giữ. Code game nên đặt trong các module riêng (`Core/Src/gfx.c`, `game.c`, ...)
> và chỉ *gọi* chúng từ vùng USER CODE — sẽ không bị mất khi regenerate.

## Bước 8 — Build kiểm tra

Từ thư mục gốc project:

```bash
./build.sh          # biên dịch (xem build/leafmuncher-plus.elf)
./build.sh flash    # nạp xuống bo qua ST-LINK
./build.sh clean    # xoá thư mục build/
```

(Chi tiết script ở [docs/setup/02-build-flash.md](02-build-flash.md).)

---

## Peripheral được sinh ra (đối chiếu nhanh)

| Peripheral | File | Dùng cho |
|---|---|---|
| LTDC | `ltdc.c/.h` | Quét màn TFT từ framebuffer |
| FMC (SDRAM) | `fmc.c/.h` | Vùng nhớ framebuffer |
| DMA2D | `dma2d.c/.h` | Tô/blit tăng tốc (Chrom-ART) |
| ADC1 (+DMA) | `adc.c/.h` | Đọc 2 trục joystick |
| GPIO | `gpio.c/.h` | Nút JOY_SW, nút B1, LED |
| SPI5 | `spi.c/.h` | Lệnh điều khiển ILI9341 / gyro |
| TIM6 | `tim.c/.h` | HAL timebase (FreeRTOS giữ SysTick) |
| TIM7 (+IRQ) | `tim.c/.h`, `stm32f4xx_it.c` | Đồng hồ ms thực + heartbeat LED (T017) |
| FreeRTOS | `freertos.c`, `Middlewares/.../FreeRTOS` | Đa task game |
