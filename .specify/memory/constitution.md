<!--
Sync Impact Report
==================
Version change: (template) → 1.0.0 → 1.1.0
Bump rationale:
  - 1.0.0: Lần phê chuẩn đầu tiên — thay toàn bộ placeholder bằng 7 nguyên tắc cụ thể.
  - 1.1.0 (MINOR, 2026-06-21): thêm module `theme`/`store` (NT IV) + peripheral Flash (§2)
    cho tính năng 2 chế độ chơi, theme và lưu bền vững (US5/US6).
Modified principles: (none — khởi tạo mới)
Added sections:
  - 7 Core Principles (I..VII)
  - Ràng Buộc Kỹ Thuật & Phần Cứng (Section 2)
  - Quy Trình Phát Triển (Section 3)
  - Governance
Removed sections: (none)
Templates requiring updates:
  - .specify/templates/plan-template.md      ✅ tương thích (Constitution Check đọc động, không cần sửa)
  - .specify/templates/spec-template.md       ✅ tương thích
  - .specify/templates/tasks-template.md      ✅ tương thích
Follow-up TODOs: (none)
-->

# LeafMuncher+ Constitution

Đồ án môn học Hệ thống nhúng — game "Sâu Ăn Lá+" trên kit **STM32F429I-DISC1** + joystick.
Spec gốc: `specs/001-leafmuncher-plus-game/spec.md`.

## Core Principles

### I. Tech Stack Cố Định (Fixed Toolchain)
Bộ công cụ là **bất biến** trong suốt dự án:
- Cấu hình peripheral và sinh code init bằng **STM32CubeMX**, xuất project kiểu **Makefile**.
- Build bằng **arm-none-eabi-gcc + make** (bundled trong STM32CubeIDE); nạp qua ST-LINK
  (`STM32_Programmer_CLI`/OpenOCD). Lệnh chuẩn: `./build.sh`, `./build.sh flash`.
- RTOS: **FreeRTOS qua CMSIS-RTOS v2**.

KHÔNG đổi sang IDE/toolchain/RTOS khác. Mọi thay đổi stack PHẢI sửa constitution trước.
*Lý do:* đồng bộ với yêu cầu trường, tái lập được, và cho phép Claude Code build/kiểm lỗi trực tiếp.

### II. Tách Logic Khỏi Phần Cứng (Hardware-Independent Logic)
Logic game (`game.c`, `levels.c`, `rng.c` và tương đương) **KHÔNG được gọi HAL/CMSIS/FreeRTOS API**.
Chúng chỉ thao tác trên state thuần (struct, mảng) và nhận input qua tham số.
Phần cứng (ADC, LTDC, DMA2D, GPIO) chỉ được chạm tới ở lớp driver (`gfx.c`, `input.c`) và `tasks.c`.

*Lý do:* logic thuần PHẢI **unit-test được trên PC** (host build) — đây là cách kiểm thử chính
cho phần khó test trên bo. Vi phạm nguyên tắc này làm mất khả năng test.

### III. Bảo Toàn Code Khi Regenerate (Regeneration-Safe Code)
Code do người viết PHẢI nằm trong **module riêng** (`Core/Src/*.c` ngoài các file CubeMX quản lý),
và chỉ được **gọi** từ trong vùng `/* USER CODE BEGIN x */ ... /* USER CODE END x */`.
KHÔNG sửa logic vào thẳng các file CubeMX sinh (`main.c` init, `gpio.c`, `adc.c`, `ltdc.c`…) ngoài vùng USER CODE.

*Lý do:* chạy lại CubeMX (đổi chân/peripheral) sẽ ghi đè mọi thứ ngoài vùng USER CODE.
Mọi thay đổi cấu hình peripheral đi qua `.ioc` rồi Generate, không sửa tay code init.

### IV. Cô Lập Module (Single-Responsibility Modules)
Mỗi module có **một trách nhiệm** và một interface gọn (header khai báo tối thiểu cái cần lộ):
`gfx` (vẽ framebuffer) · `input` (đọc joystick) · `game` (logic) · `render` (state→gfx) ·
`rng` (số ngẫu nhiên) · `theme` (bảng màu/sprite cosmetic) · `store` (lưu bền vững Flash) ·
`tasks` (vòng đời FreeRTOS). Người đọc PHẢI hiểu module làm gì mà không cần đọc nội tạng.
Khi một file phình to / ôm nhiều việc → tách nhỏ.

### V. Đồ Hoạ Không Nhấp Nháy (Flicker-Free Rendering)
Vẽ qua **framebuffer trong SDRAM**, tăng tốc bằng **DMA2D**, và dùng **double-buffer** (swap khi vẽ xong).
KHÔNG vẽ trực tiếp lên buffer đang quét. Lưới game = ô vuông → ưu tiên fill-rect bằng DMA2D.

*Lý do:* trải nghiệm mượt, không xé hình; đồng thời thể hiện kiến thức LTDC/FMC/DMA2D của đồ án.

### VI. Luôn Có Bản Chạy Được (Always-Shippable Increments)
Phát triển theo lộ trình tăng dần **M1→M7** (xem spec). Sau mỗi mốc PHẢI có một bản **nạp được và demo được**.
Ưu tiên hoàn thiện "rắn cổ điển chạy được" (M3) trước khi thêm power-up/level.
KHÔNG để code ở trạng thái không build/không chạy qua đêm.

### VII. Build Sạch Là Bắt Buộc (Clean Build Gate — NON-NEGOTIABLE)
Một thay đổi chỉ được coi là "xong" khi **`./build.sh` biên dịch sạch (0 error)**.
Cảnh báo (warning) PHẢI được xem xét, không tích luỹ vô tội vạ.
Tuyên bố "đã chạy/đã sửa" PHẢI kèm bằng chứng (output build, hoặc quan sát trên bo).

## Ràng Buộc Kỹ Thuật & Phần Cứng

- **Bo:** STM32F429I-DISC1 (STM32F429ZI, Cortex-M4 180MHz, 2MB Flash, 256KB RAM, SDRAM 8MB,
  TFT 320×240 ILI9341 qua LTDC).
- **Chân joystick (đã chốt):** VRx=**PA5** (ADC1_IN5), VRy=**PC3** (ADC1_IN13), SW=**PB7** (GPIO_Input + Pull-up).
  Chân cấm dùng (bo đã chiếm): PA7, PC4, PC1, PC2, PA1, PA2.
- **Peripheral cần thể hiện (yêu cầu đồ án):** GPIO, ADC(+DMA), Timer, Interrupt, LTDC, FMC/SDRAM, DMA2D, Flash (lưu bền vững), FreeRTOS.
- **Kiến trúc runtime:** 3 task FreeRTOS — Input (đọc ADC/nút), Game (logic, `vTaskDelayUntil`), Render (vẽ);
  đồng bộ qua **queue + mutex + semaphore**.
- **Lưới game:** ô 16×16px, sân chơi 20×13, HUD trên cùng 32px.

## Quy Trình Phát Triển

- **Spec-Driven Development (Spec Kit):** constitution → `/speckit-specify` → `/speckit-plan` →
  `/speckit-tasks` → `/speckit-implement`. Artifact gốc đặt trong `specs/` và `docs/`.
- **Cấu hình peripheral:** luôn qua CubeMX `.ioc` rồi Generate; ghi lại bước trong
  `docs/setup/01-cubemx-codegen.md`.
- **Kiểm thử:** logic thuần → unit-test host (Nguyên tắc II); phần cứng → smoke test trên bo + checklist thủ công.
- **Định nghĩa "Done":** build sạch (`./build.sh`) **và** mốc tương ứng chạy được trên bo/quan sát được.

## Governance

Constitution này **đứng trên** mọi quy ước khác của dự án. Khi có xung đột, constitution thắng,
trừ khi người dùng ra chỉ thị trực tiếp.

- **Sửa đổi:** ghi lý do, cập nhật phiên bản theo SemVer, đồng bộ các template liên quan.
  - MAJOR: gỡ/định nghĩa lại nguyên tắc theo hướng không tương thích.
  - MINOR: thêm nguyên tắc/mục mới hoặc mở rộng đáng kể.
  - PATCH: làm rõ câu chữ, sửa lỗi không đổi ngữ nghĩa.
- **Tuân thủ:** mỗi lần review/PR phải đối chiếu 7 nguyên tắc; độ phức tạp phát sinh phải có lý do.
- **Hướng dẫn runtime:** xem `docs/setup/` và spec trong `specs/`.

**Version**: 1.1.0 | **Ratified**: 2026-06-19 | **Last Amended**: 2026-06-21
