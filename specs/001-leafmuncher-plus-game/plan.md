# Implementation Plan: LeafMuncher+ (Sâu Ăn Lá+)

**Branch**: `main` (feature `001-leafmuncher-plus-game`) | **Date**: 2026-06-19 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/001-leafmuncher-plus-game/spec.md`

## Summary

Game nhúng "Sâu Ăn Lá+" (snake mở rộng) trên **STM32F429I-DISC1** + joystick analog. Cách tiếp cận kỹ
thuật: **tách logic thuần khỏi phần cứng** — `game`/`levels`/`rng` là C thuần, unit-test trên PC; lớp
driver (`gfx`/`input`) chạm HAL; `render` ánh xạ state→gfx; `tasks` chạy 3 task FreeRTOS (Input/Game/
Render) đồng bộ qua queue + mutex + semaphore. Đồ hoạ **tự code trên framebuffer SDRAM + DMA2D +
double-buffer**, swap đồng bộ VSYNC (không thư viện GUI). Phát triển theo mốc **M1→M7**, ưu tiên M3
(rắn cổ điển) trước.

## Technical Context

**Language/Version**: C11 (firmware, HAL/CMSIS) + C (host unit test). KHÔNG dùng C++.

**Primary Dependencies**: STM32 HAL F4 + CMSIS, FreeRTOS qua CMSIS-RTOS v2 (heap_4), peripheral
LTDC/FMC(SDRAM)/DMA2D/ADC1+DMA/SPI5(panel ILI9341)/TIM6(HAL timebase)/TIM7(đồng hồ ms + heartbeat)/GPIO.
Build: `arm-none-eabi-gcc` + `make` (bundled CubeIDE). Host test: `gcc` thường (không link HAL).

**Storage**: N/A trong v1 (không lưu high-score — stretch goal). Framebuffer đặt trong SDRAM 8MB; state
game trong SRAM nội.

**Testing**: Unit-test host (gcc + assert) cho logic thuần (`game`/`levels`/`rng`) — bao phủ SC-006.
Phần cứng: smoke test trên bo + checklist thủ công theo từng mốc.

**Target Platform**: STM32F429ZI (Cortex-M4 180MHz, 2MB Flash, 256KB SRAM, 8MB SDRAM), bare-metal +
FreeRTOS, TFT 320×240 ILI9341 qua LTDC.

**Project Type**: Firmware nhúng — single project (cấu trúc CubeMX), kèm thư mục `test/` host.

**Performance Goals**: tick game ổn định không trượt deadline (`vTaskDelayUntil`); LTDC quét ~60Hz không
nhấp nháy/không xé hình (swap tại VSYNC); độ trễ input→đổi hướng ≤ 1 bước (SC-002).

**Constraints**: logic thuần KHÔNG gọi HAL/CMSIS/FreeRTOS (NT II); code tự viết chỉ gọi từ vùng USER
CODE (NT III); 2 framebuffer 320×240×2 = 300KB nằm gọn trong SDRAM; `./build.sh` 0 error là cổng bắt
buộc (NT VII).

**Scale/Scope**: 6 trạng thái (MENU/PLAYING/PAUSED/GAME_OVER/LEVEL_COMPLETE/WIN), 5 level, ~7 module,
lưới 20×13 = 260 ô, 4 loại power-up, 1 người chơi.

## Constitution Check

*GATE: phải pass trước Phase 0; kiểm lại sau Phase 1.*

| # | Nguyên tắc | Cách plan tuân thủ | Trạng thái |
|---|---|---|---|
| I | Tech stack cố định | Giữ CubeMX/Makefile/arm-gcc/FreeRTOS CMSIS_V2/DMA2D; không thêm dependency, không GUI lib | ✅ PASS |
| II | Tách logic khỏi phần cứng | `game.c`/`levels.c`/`rng.c` chỉ thao tác struct/mảng, nhận input qua tham số; host-test bằng gcc | ✅ PASS |
| III | Bảo toàn code khi regenerate | Module game ở file riêng; `main.c`/`freertos.c` chỉ *gọi* từ `/* USER CODE BEGIN/END */` | ✅ PASS |
| IV | Cô lập module | 7 module 1-trách-nhiệm: gfx · input · game · levels · render · rng · tasks | ✅ PASS |
| V | Đồ hoạ không nhấp nháy | framebuffer SDRAM + DMA2D fill/blit + double-buffer, swap tại ngắt LTDC (VSYNC) | ✅ PASS |
| VI | Luôn có bản chạy được | Lộ trình M1→M7, mỗi mốc nạp được + demo được; ưu tiên M3 | ✅ PASS |
| VII | Build sạch bắt buộc | Định nghĩa Done = `./build.sh` 0 error + mốc chạy được; tuyên bố kèm bằng chứng | ✅ PASS |

**Kết luận:** 0 vi phạm → Complexity Tracking để trống.

## Project Structure

### Documentation (this feature)

```text
specs/001-leafmuncher-plus-game/
├── plan.md              # File này (/speckit-plan)
├── research.md          # Phase 0 — chốt hằng số & quyết định thiết kế
├── data-model.md        # Phase 1 — entity, state machine, layout bộ nhớ
├── quickstart.md        # Phase 1 — kịch bản kiểm chứng theo mốc
├── contracts/           # Phase 1 — hợp đồng interface module
│   ├── game-core.md     #   API logic thuần (game_step) + bất biến
│   ├── render-gfx.md    #   API gfx + hợp đồng render
│   ├── input.md         #   ánh xạ joystick→hướng + sự kiện nút
│   └── levels.md        #   định dạng dữ liệu level
└── tasks.md             # Phase 2 (/speckit-tasks — CHƯA tạo ở bước này)
```

### Source Code (repository root)

```text
leafmuncher-plus.ioc        # nguồn cấu hình CubeMX (sửa peripheral ở đây rồi Generate)
Makefile                    # do CubeMX sinh
build.sh                    # wrapper build/flash

Core/
├── Inc/                    # header — CubeMX (main.h, adc.h, ltdc.h, dma2d.h, fmc.h…)
│   ├── gfx.h               #   [tự viết] primitive vẽ framebuffer
│   ├── input.h             #   [tự viết] đọc joystick + nút → InputEvent
│   ├── game.h              #   [tự viết] logic thuần (state + game_step)
│   ├── levels.h            #   [tự viết] dữ liệu level (const)
│   ├── render.h            #   [tự viết] state → gfx
│   ├── rng.h               #   [tự viết] PRNG thuần
│   └── tasks.h             #   [tự viết] khởi tạo 3 task + đối tượng đồng bộ
├── Src/                    # CubeMX (main.c, freertos.c, adc.c, ltdc.c, dma2d.c, fmc.c…)
│   ├── gfx.c               #   [tự viết] DMA2D fill/blit/blend, swap VSYNC
│   ├── input.c             #   [tự viết] hiệu chỉnh center + deadzone + debounce
│   ├── game.c              #   [tự viết] THUẦN — move/grow/collide/spawn
│   ├── levels.c            #   [tự viết] THUẦN — bảng level
│   ├── render.c            #   [tự viết] vẽ từng trạng thái (dirty-rect)
│   ├── rng.c               #   [tự viết] THUẦN — xorshift32
│   └── tasks.c             #   [tự viết] InputTask/GameTask/RenderTask + queue/mutex/sem
└── (assets)               #   font 8×16 + bảng màu (const trong flash)

Drivers/ Middlewares/       # HAL + CMSIS + FreeRTOS (KHÔNG sửa)

test/                       # [tự viết] unit-test host (gcc, không HAL)
├── Makefile                #   build & chạy test trên PC
├── test_game.c             #   luật core: grow/collide/180°/spawn/floor
├── test_levels.c           #   target & layout level
└── stubs/                  #   (nếu cần) thay HAL ở mức biên

docs/
├── setup/                  # 01-cubemx-codegen.md, 02-build-flash.md (đã có)
└── ui/                     # [tự viết] ui-design.md — layout, palette, font, API gfx
```

**Structure Decision**: Giữ nguyên cấu trúc CubeMX hiện có (`Core/Inc`, `Core/Src`, `Drivers`,
`Middlewares`). Module game tự viết đặt **cạnh** file CubeMX trong `Core/` (cùng Makefile biên dịch),
mỗi module 1 cặp `.c/.h` (NT IV) và **không** chứa logic trong file CubeMX (NT III). Thư mục `test/`
tách riêng, biên dịch bằng gcc host để chạy logic thuần (NT II) — không nằm trong build firmware.

## Complexity Tracking

> Không có vi phạm Constitution Check → bảng để trống.

| Violation | Why Needed | Simpler Alternative Rejected Because |
|-----------|------------|-------------------------------------|
| (none)    | —          | —                                   |
