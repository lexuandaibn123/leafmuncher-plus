---
description: "Task list for LeafMuncher+ implementation"
---

# Tasks: LeafMuncher+ (Sâu Ăn Lá+)

**Input**: Design documents from `specs/001-leafmuncher-plus-game/`

**Prerequisites**: [plan.md](plan.md), [spec.md](spec.md), [research.md](research.md),
[data-model.md](data-model.md), [contracts/](contracts/), [quickstart.md](quickstart.md)

**Tests**: Test logic thuần là **BẮT BUỘC** ở đây — SC-006 + Nguyên tắc II (constitution) yêu cầu kiểm
chứng tự động luật core trên host (gcc). Task test (host) có trong từng story tương ứng.

**Organization**: Task gom theo user story (US1–US7) để hiện thực & test độc lập. Phase Setup +
Foundational tương ứng mốc **M1–M2**; US1=M3, US2=M4, US3=M5–M6, US4=M7, US5/US6/US7=M8 (xem [plan.md](plan.md)).

## Format: `[ID] [P?] [Story] Description`

- **[P]**: chạy song song được (**khác file**, không phụ thuộc task chưa xong)
- **[Story]**: US1/US2/US3/US4 — chỉ cho task thuộc phase user story
- Đường dẫn file chính xác trong mô tả
- ⚖️ **Regen-safe (NT III)**: code game ở module riêng; chỉ *gọi* từ vùng `/* USER CODE BEGIN/END */`
  trong `main.c`/`freertos.c`/`stm32f4xx_it.c`. Đổi peripheral đi qua `.ioc` rồi Generate (không sửa tay
  file init).

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Tạo khung 7 module core + harness test host. Scaffold CubeMX đã có sẵn. (`theme`/`store`
tạo ở Phase 7 cùng tính năng của chúng → tổng 9 module.)

- [x] T001 [P] Tạo skeleton header (include guard, rỗng) cho 7 module trong `Core/Inc/`: `gfx.h`, `input.h`, `game.h`, `levels.h`, `render.h`, `rng.h`, `apptasks.h`
- [x] T002 [P] Tạo skeleton source (include header, stub rỗng) cho 7 module trong `Core/Src/`: `gfx.c`, `input.c`, `game.c`, `levels.c`, `render.c`, `rng.c`, `apptasks.c`

> ⚠️ **Tên file `apptasks.c/.h`** (KHÔNG đặt `tasks.c`): Makefile CubeMX sinh object theo `notdir`, nên
> một file tên `tasks.c` sẽ đè `build/tasks.o` của FreeRTOS (`Middlewares/.../Source/tasks.c`) → thiếu
> symbol `vTaskSwitchContext`/`pxCurrentTCB`… Module vẫn gọi là **`tasks`**; chỉ tên file đổi để tránh trùng.
- [x] T003 Khai báo kiểu & hằng số dùng chung từ [data-model.md](data-model.md) + [research.md](research.md) (Dir, Cell, GameMode, LeafType, PowerType, InputEvent, GameEvents, COLS/ROWS/CELL/HUD_H, STEP_MS[], LEN_*, ...) trong `Core/Inc/game.h` (phụ thuộc T001)
- [x] T004 [P] Tạo harness test host: `test/Makefile` (gcc, include `Core/Inc`), `test/test_game.c` với 1 assert tối thiểu; xác nhận `make -C test` chạy
- [x] T005 Thêm 7 source mới vào `C_SOURCES` của `Makefile` (CubeMX) để firmware biên dịch chúng; ghi chú phải thêm lại sau mỗi lần CubeMX Generate; chạy `./build.sh` 0 error
- [x] T006 [P] Tạo `docs/ui/ui-design.md` từ [research.md §15](research.md) (layout 320×240/HUD, bảng màu, font 8×16, tóm tắt API `gfx`)

---

## Phase 2: Foundational (Blocking Prerequisites) — Mốc M1 + M2

**Purpose**: Hạ tầng hiển thị + input + timer + engine RTOS + harness — phải xong trước MỌI user story.

**⚠️ CRITICAL**: Không story nào bắt đầu cho tới khi phase này xong.

### M1 — Bring-up phần cứng & gfx

- [x] T007 [P] Hiện thực `rng` thuần (xorshift32: `rng_seed`/`rng_next`/`rng_range`) trong `Core/Src/rng.c` + `Core/Inc/rng.h`
- [x] T008 [P] `gfx_init` (2 framebuffer SDRAM `0xD0000000`/`0xD0025800`, cấu hình LTDC layer RGB565), `gfx_clear`, `gfx_fill_rect` (DMA2D R2M), helper `gfx_rgb565` trong `Core/Src/gfx.c` (hợp đồng [contracts/render-gfx.md](contracts/render-gfx.md)) — *gfx_init kèm chuỗi init SDRAM (MX_FMC_Init thiếu); xoay 90° landscape→portrait trong phần mềm; gfx_present bản cơ bản (T011 nâng cấp)*
- [x] T009 **Panel ILI9341 bring-up qua SPI5**: gửi chuỗi lệnh init (interface/RGB mode, SLPOUT `0x11`, DISPON `0x29`, gamma/MADCTL) để panel vào chế độ RGB cho LTDC quét; gọi trong `gfx_init` **trước khi** bật LTDC, trong `Core/Src/gfx.c` dùng `hspi5` (phụ thuộc T008). ⚠️ Không có bước này màn sẽ đen ở M1. — ✅ **nghiệm thu trên bo: hiện màu đặc đúng**. Chốt 4 gotcha (research §21): SDRAM init seq, `0xB0=0xC2`, DMA2D R2M màu ARGB8888, LTDC clock ~6MHz.
- [x] T010 `gfx_blit` + `gfx_text` + bảng font 8×16 ASCII (`const` flash) trong `Core/Src/gfx.c` (phụ thuộc T008) — **nghiệm thu on-board** (chữ rõ, đúng chiều); font sinh từ `tools/genfont.c` → `Core/Src/font8x16.c`; glyph/blit vẽ bằng CPU có xoay 90° (DMA2D M2M không xoay được)
- [x] T011 `gfx_present` + swap double-buffer đồng bộ VSYNC qua ngắt line LTDC trong `Core/Src/gfx.c` (phụ thuộc T008) — **nghiệm thu on-board: HẾT XÉ hoàn toàn**; `HAL_LTDC_LineEventCallback` swap tại dòng 323 (vùng blank), present chờ tới VSYNC; ISR đi qua `LTDC_IRQHandler` sẵn có
- [x] T012 `gfx_blend_rect` (alpha blend CPU theo ô, có xoay) cho overlay mờ trong `Core/Src/gfx.c` (phụ thuộc T008) — **nghiệm thu on-board** (overlay PAUSED mờ đúng); blend hằng-màu nên dùng CPU (DMA2D M2M_BLEND cần buffer FG); overlay tĩnh nên vẽ 1 lần
- [x] T013 [P] `input_init` (start ADC1 DMA vào `uint16_t[2]`, hiệu chỉnh center 16 mẫu/trục) + ánh xạ joystick→hướng (deadzone, trục trội, hysteresis) → `InputEvent` trong `Core/Src/input.c` (hợp đồng [contracts/input.md](contracts/input.md)) — **nghiệm thu on-board** (4 hướng khớp); trục trội ≥1.3× (AXIS_HYST 13/10); `JOY_INVERT_Y=1` (VRy đấu ngược); **ADC sample-time 480 chu kỳ** chống overrun/bão ngắt (research §22)
- [x] T014 Đọc **1 nút** PB7 (JOY_SW) với debounce ≥30ms + cạnh-nhấn → `IN_SELECT` ("nút chính") trong `Core/Src/input.c` (phụ thuộc T013) — bỏ nút B1/PA0 (ý nghĩa pause/select do FSM quyết theo `mode`); debounce thời gian thực HAL_GetTick, hàng chờ cạnh-nhấn 1 ô để không mất sự kiện
- [x] T015 `input_entropy()` tích luỹ LSB nhiễu ADC cho seed RNG trong `Core/Src/input.c` (phụ thuộc T013) — gom LSB ADC ở input_init (16 mẫu) + mỗi input_poll
- [x] T016 [P] Helper LED (xanh PG13 / đỏ PG14) + safe-stop khi init HW lỗi (bật LED đỏ, dừng) trong `Core/Src/apptasks.c` — `led_green/led_red/safe_stop`; `timebase_start` gọi `safe_stop` nếu `HAL_TIM_Base_Start_IT` lỗi
- [x] T017 **Cấu hình hardware Timer (TIM7 basic, qua `.ioc` rồi Generate — TIM6 đã là HAL timebase)**: bật **update interrupt** làm **time-base mili-giây thực** + nhịp **heartbeat LED xanh ~1Hz**; ISR đặt ở `Core/Src/stm32f4xx_it.c` vùng USER CODE, logic ở `Core/Src/apptasks.c`. Cung cấp đồng hồ ms thực cho đếm ngược lá vàng/power-up (M5/M6). *(Thoả yêu cầu peripheral Timer + Interrupt của constitution §2, nghiệm thu được.)* — **nghiệm thu on-board**: heartbeat xanh ~1Hz; `app_millis()` đếm ms; `timebase_tick_isr` gọi từ `TIM7_IRQHandler`
- [x] T018 Demo M1: trong `Core/Src/main.c`/`freertos.c` vùng USER CODE, gọi `gfx`+`input` để 1 ô vuông di chuyển theo joystick **trên panel hiển thị** + LED heartbeat (TIM7) nhấp nháy; xác nhận `./build.sh` 0 error + chạy trên bo (checklist M1 [quickstart.md](quickstart.md)) (phụ thuộc T008–T017) — **nghiệm thu on-board**: ô chạy 4 hướng + mũi tên chỉ hướng (soi lệch cần khi bấm) + heartbeat + nút JOY_SW toggle pause (overlay mờ), không xé hình

### M2 — Khung engine FreeRTOS

- [x] T019 Định nghĩa `GameState` (worm ring buffer, `occupied[13][20]`, lá, power, score, level) + `game_init`/`game_start` + query đọc-chỉ `game_cell_content`/`game_step_ms` (toàn bộ API thuần của `game.h`), trong `Core/Src/game.c` + `Core/Inc/game.h` (data-model §2.5; hợp đồng [contracts/game-core.md](contracts/game-core.md)) (phụ thuộc T003 — dùng kiểu/hằng do T003 khai báo trong cùng `game.h`) — **host test pass** (`test_game.c`: init MENU/start PLAYING, sâu LEN_START giữa sân, occupied=thân, query lá/oob, tính xác định cùng seed); `STEP_MS[]`/`TARGET_LEAVES[]` định nghĩa trong `levels.c` (lát cắt T041)
- [ ] T020 Tạo đối tượng đồng bộ: queue input (sâu 4, overwrite), mutex snapshot state, semaphore/notification "frame ready" trong `Core/Src/apptasks.c` (research §12)
- [ ] T021 `InputTask` @50Hz đọc `input_poll` → đẩy `InputEvent` vào queue trong `Core/Src/apptasks.c` (phụ thuộc T014, T020)
- [ ] T022 `GameTask` `vTaskDelayUntil(step_ms)`, lấy input mới nhất, gọi `game_step`, ghi snapshot dưới mutex, báo render trong `Core/Src/apptasks.c` (phụ thuộc T019, T020)
- [ ] T023 `RenderTask` chờ "frame ready", copy snapshot dưới mutex tối thiểu, gọi `render`, `gfx_present()` trong `Core/Src/apptasks.c` (phụ thuộc T011, T020)
- [ ] T024 `render_force_full` + `render_frame` dispatch theo `mode`; vẽ lưới tĩnh + khung HUD trong `Core/Src/render.c` (hợp đồng [contracts/render-gfx.md](contracts/render-gfx.md))
- [ ] T025 Khởi tạo 3 task từ `Core/Src/freertos.c` vùng USER CODE (gọi `tasks_start()`); **seed RNG = (bộ đếm TIM7/DWT tại input đầu tiên) XOR `input_entropy()`** rồi truyền vào `game_init` (research §13); bỏ/thay `defaultTask` (phụ thuộc T015, T017, T021–T024)
- [x] T026 [P] Mở rộng test host: `test/test_rng.c` (tính xác định của xorshift32) trong `test/`
- [ ] T027 Demo M2: "đầu sâu" 1 ô di chuyển trên lưới ở nhịp tick cố định qua 3 task; `./build.sh` 0 error + on-board (checklist M2) (phụ thuộc T019–T025)

**Checkpoint**: Nền tảng sẵn sàng — user story có thể bắt đầu.

---

## Phase 3: User Story 1 - Sâu ăn lá cổ điển (Priority: P1) 🎯 MVP — Mốc M3

**Goal**: Chơi trọn một ván snake cổ điển: di chuyển, ăn lá dài ra +10 điểm, thua khi va tường/thân, hiện
điểm cuối.

**Independent Test**: Boot → vào PLAYING; gạt joystick lái sâu ăn lá (+10, dài ra); va tường/thân →
GAME_OVER hiện điểm. (Menu đầy đủ thuộc US4 — ở M3 boot thẳng vào PLAYING.)

### Tests for User Story 1 (host, BẮT BUỘC) ⚠️

- [ ] T028 [US1] Test move/grow/eat: ăn lá → `len+1`, `score+10`, sinh lá mới ở ô trống; trong `test/test_game.c`
- [ ] T029 [US1] Test va chạm: tường + thân **trừ ô đuôi vừa nhả** (đi vào ô đuôi cũ KHÔNG chết); deadzone đi thẳng; trong `test/test_game.c`
- [ ] T030 [US1] Test chặn 180° gồm **gạt 2 lần trong 1 tick** (theo `committed_dir`); trong `test/test_game.c`
- [ ] T031 [US1] Test sinh lá không đè thân/chướng ngại; trong `test/test_game.c` *(nhánh sân-đầy→thắng-màn cần ST_LEVEL_COMPLETE/ST_WIN nên test ở US2/T040, không ở đây)*

### Implementation for User Story 1

- [ ] T032 [US1] Hiện thực di chuyển sâu + commit hướng (lọc 180° theo `committed_dir`, buffer đúng 1 lệnh rẽ) trong `Core/Src/game.c`
- [ ] T033 [US1] Ăn lá thường (grow, score+10, cập nhật `occupied`) + sinh lá ở ô trống (duyệt ô trống/chọn ô thứ k qua `rng`) trong `Core/Src/game.c` (phụ thuộc T032)
- [ ] T034 [US1] Phát hiện va chạm (tường + thân-trừ-ô-đuôi) → `ST_GAME_OVER` + `EV_GAME_OVER` trong `Core/Src/game.c` (phụ thuộc T032)
- [ ] T035 [US1] `game_step` máy trạng thái PLAYING (áp input, deadzone đi thẳng, sinh sự kiện) + chuyển sang GAME_OVER trong `Core/Src/game.c` (phụ thuộc T032–T034)
- [ ] T036 [US1] Render PLAYING dirty-rect: ô đầu mới, ô đuôi cũ, ô lá, vùng điểm HUD trong `Core/Src/render.c` (phụ thuộc T024)
- [ ] T037 [US1] Render màn GAME_OVER (điểm cuối) + bật LED đỏ; LED xanh khi PLAYING trong `Core/Src/render.c` + `Core/Src/apptasks.c` (phụ thuộc T036)
- [ ] T038 [US1] Boot thẳng vào PLAYING (tạm thời, sẽ thay bằng MENU ở US4/T061) trong `Core/Src/game.c`/`freertos.c` USER CODE; chạy `make -C test` xanh + `./build.sh` + on-board M3 (Acceptance US1, SC-001/002/006)

**Checkpoint**: US1 hoạt động & test độc lập — đây là MVP demo được.

---

## Phase 4: User Story 2 - Chướng ngại & nhiều màn (Priority: P2) — Mốc M4

**Goal**: Mỗi màn có chướng ngại + mục tiêu; đạt mục tiêu qua màn, tốc độ tăng; thắng màn cuối; sân đầy
cũng coi như thắng màn.

**Independent Test**: Đụng chướng ngại → GAME_OVER; ăn đủ `target` lá → LEVEL_COMPLETE → màn kế nhanh hơn;
đạt mục tiêu màn cuối → WIN.

### Tests for User Story 2 (host) ⚠️

- [ ] T039 [P] [US2] Test bất biến level (target>0, `step_ms` giảm dần, ô khởi đầu trống, đủ ô trống) trong `test/test_levels.c`
- [ ] T040 [US2] Test va chạm chướng ngại → GAME_OVER; level-complete; tăng tốc; WIN màn cuối; **sân đầy (hết ô trống sinh lá) → thắng màn** (LEVEL_COMPLETE/WIN); trong `test/test_game.c`

### Implementation for User Story 2

- [ ] T041 [P] [US2] Module `levels`: 5 bitmap chướng ngại 20×13 + `level_get`/`level_is_last` trong `Core/Src/levels.c` + `Core/Inc/levels.h` (hợp đồng [contracts/levels.md](contracts/levels.md))
- [ ] T042 [US2] Nạp chướng ngại của level vào `occupied` khi `game_start`/lên màn trong `Core/Src/game.c` (phụ thuộc T041)
- [ ] T043 [US2] Va chạm chướng ngại → GAME_OVER trong `game_step` (`Core/Src/game.c`) (phụ thuộc T042)
- [ ] T044 [US2] Đạt `target_leaves` → `ST_LEVEL_COMPLETE`; **chỉ sang màn kế khi nhận `IN_SELECT` (FR-021 — KHÔNG tự động)**, lúc đó reset sâu, giữ score, nạp `step_ms` màn mới; `ST_WIN` ở màn cuối; **+ nhánh sân-đầy: khi sinh lá mà không còn ô trống → phát `EV_LEVEL_DONE`/`EV_WIN` (LEVEL_COMPLETE nếu còn màn, WIN nếu màn cuối)** trong `Core/Src/game.c` (phụ thuộc T042)
- [ ] T045 [US2] Render chướng ngại + màn LEVEL_COMPLETE + WIN trong `Core/Src/render.c` (phụ thuộc T036)
- [ ] T046 [US2] `make -C test` xanh + `./build.sh` + on-board M4 (Acceptance US2)

**Checkpoint**: US1 + US2 đều chạy độc lập.

---

## Phase 5: User Story 3 - Đa dạng lá & power-up (Priority: P3) — Mốc M5–M6

**Goal**: Lá vàng (+50, hết hạn tự mất), lá độc (co/trừ điểm), 4 power-up có thời hạn (tăng tốc/làm chậm/
bất tử/xuyên tường) với đồng hồ đếm ngược + stack.

**Independent Test**: Cho từng loại lá xuất hiện → ăn lá vàng +50 (hết hạn tự mất); lá độc co sâu (không
Game Over); power-up bật hiệu ứng có đồng hồ, tự tắt.

### Tests for User Story 3 (host) ⚠️

- [ ] T047 [US3] Test lá vàng (+50, hết hạn `EV_LEAF_EXPIRED`) + lá độc (co 2 đốt, sàn `LEN_MIN`, phạt điểm khi ở sàn) trong `test/test_game.c`
- [ ] T048 [US3] Test power-up: bật/trừ theo `dt_ms`/tắt/refresh cùng loại/stack khác loại; PHASE wrap; GHOST bỏ qua va thân + grace; trong `test/test_game.c`

### Implementation for User Story 3 (M5 — lá đa dạng)

- [ ] T049 [US3] Lá vàng: sinh (15%/lần ăn nếu chưa có), ăn +50, đồng hồ `GOLD_LIFE_MS` (ms thực từ TIM7) hết hạn tự mất trong `Core/Src/game.c` (phụ thuộc T033)
- [ ] T050 [US3] Lá độc: sinh (lvl≥2, 20%), ăn → co 2 đốt (sàn `LEN_MIN`; nếu ở sàn thì −20 điểm), không Game Over trong `Core/Src/game.c` (phụ thuộc T033)
- [ ] T051 [US3] Render lá vàng (nhấp nháy) + lá độc trong `Core/Src/render.c` (phụ thuộc T036)

### Implementation for User Story 3 (M6 — power-up)

- [ ] T052 [US3] Power-up: sinh (lvl≥3, 12%), ăn → set `power.active[type]=PU_EFFECT_MS`, đồng hồ trừ theo ms thực (TIM7), đổi `step_ms` (SPEED/SLOW factor) trong `Core/Src/game.c` (phụ thuộc T033)
- [ ] T053 [US3] GHOST (bỏ qua va thân + grace khi đầu còn chồng thân) + PHASE (wrap biên sang cạnh đối diện) trong `Core/Src/game.c` (phụ thuộc T034, T052)
- [ ] T054 [US3] Render ô power-up + HUD icon + thanh/đồng hồ đếm ngược trong `Core/Src/render.c` (phụ thuộc T036)
- [ ] T055 [US3] `make -C test` xanh + `./build.sh` + on-board M5+M6 (Acceptance US3)

**Checkpoint**: US1 + US2 + US3 đều chạy độc lập.

---

## Phase 6: User Story 4 - Menu & tạm dừng (Priority: P3) — Mốc M7

**Goal**: MENU điều hướng joystick + nút; PAUSED bằng nút chính JOY_SW; GAME_OVER chơi lại từ màn 1, điểm 0.

**Independent Test**: MENU đẩy joystick chọn Start → PLAYING; nhấn JOY_SW → PAUSED → nhấn tiếp tiếp tục;
GAME_OVER chọn chơi lại → ván mới điểm 0.

### Tests for User Story 4 (host) ⚠️

- [ ] T056 [US4] Test chuyển trạng thái UI: MENU select→start; pause toggle; replay reset (level 0, score 0) trong `test/test_game.c`

### Implementation for User Story 4

- [ ] T057 [US4] `game_input_ui`: điều hướng MENU (lên/xuống đổi `menu_sel`, SELECT = start) trong `Core/Src/game.c`
- [ ] T058 [US4] Pause toggle (nút chính JOY_SW = `IN_SELECT`, FSM phân tách theo `mode`) PLAYING↔PAUSED, dừng cập nhật game khi PAUSED trong `Core/Src/game.c` + `Core/Src/apptasks.c` (phụ thuộc T022)
- [ ] T059 [US4] GAME_OVER/WIN → chơi lại (SELECT → MENU rồi start, reset điểm 0) trong `Core/Src/game.c`
- [ ] T060 [US4] Render MENU + overlay PAUSED (`gfx_blend_rect` mờ + hộp) trong `Core/Src/render.c` (phụ thuộc T036, T012)
- [ ] T061 [US4] Đặt MENU làm điểm vào lúc boot (thay T038 boot-thẳng-PLAYING); **chốt bộ đếm TIM7/DWT tại sườn nhấn Start XOR `input_entropy()` → re-seed mỗi ván qua `game_init`** (research §13) trong `Core/Src/freertos.c`/`game.c` USER CODE (phụ thuộc T025, T057)
- [ ] T062 [US4] `make -C test` xanh + `./build.sh` + on-board M7 (Acceptance US4, vòng lặp đầy đủ)

**Checkpoint**: Toàn bộ 4 user story chạy độc lập — game hoàn chỉnh.

---

> **Lưu ý đánh số:** ID **T063–T068 không dùng** (khoảng trống do mở rộng US5/6/7) — không phải task bị thiếu.

## Phase 7: User Story 5/6/7 - Chế độ chơi, Theme, Lưu & Tiếp tục ván (Priority: P2/P3) — Mốc M8

**Goal**: 2 chế độ (Màn/Vô tận) chọn ở menu; 2 theme (Rừng/Sa mạc) đổi ở menu; lưu **điểm cao Vô tận +
theme** vào Flash; **Pause nâng cao** (Tiếp tục/Lưu&Thoát/Thoát) + **lưu/tiếp tục ván** mỗi mode 1 ô (US7).
Logic giữ thuần — theme/store ở lớp ngoài.

**Independent Test**: Menu chọn Vô tận → sân trống, nhanh dần, không WIN, chết → điểm cao; đổi theme →
màu/nền/chướng ngại đổi; Pause → Lưu & Thoát → tắt/bật nguồn → Tiếp tục đúng trạng thái; điểm cao + theme
còn nguyên.

### Tests for User Story 5/6 (host) ⚠️

- [ ] T069 [US5] Test Endless: `step_ms` giảm theo số lá ăn (ramp `ENDLESS_STEP_DEC`/`ENDLESS_RAMP_EVERY`), clamp `STEP_MS_MIN`, **không** LEVEL_COMPLETE/WIN; điểm cao cập nhật khi `score` vượt; chế độ Màn không đổi hành vi; trong `test/test_game.c`

### Implementation for User Story 5/6

- [ ] T070 [P] [US6] Module `theme`: bảng `Theme` `const` cho `THEME_FOREST`/`THEME_DESERT` (bảng màu + sprite chướng ngại 16×16) + `theme_get`/`theme_count`/`theme_next` trong `Core/Src/theme.c` + `Core/Inc/theme.h` (hợp đồng [contracts/theme.md](contracts/theme.md))
- [ ] T071 [P] [US5] Module `store`: `PersistData` + `store_init`/`store_get`/`store_set_*`/`store_commit`; ghi **1 sector Flash riêng** (erase+program) qua `HAL_FLASH_*`, magic/version/crc, fallback mặc định khi trống/hỏng trong `Core/Src/store.c` + `Core/Inc/store.h` (hợp đồng [contracts/store.md](contracts/store.md))
- [ ] T072 [US5] Thêm `play_mode` vào `GameState` + `game_start(play_mode,…)`; nhánh **ENDLESS** trong `game_step`: không nạp chướng ngại, không `target`/WIN, `step_ms` giảm theo `leaves_eaten` (research §18), lá đặc biệt + power-up mở khoá từ đầu; chế độ Màn giữ nguyên trong `Core/Src/game.c` (phụ thuộc T019, T044)
- [ ] T073 [US6] `render` đọc theme hiện hành: vẽ nền/màu/sprite chướng ngại theo `Theme` (thay màu hardcode); chữ HUD/đối tượng dùng bảng màu theme trong `Core/Src/render.c` (phụ thuộc T036, T070)
- [ ] T074 [US5] Render HUD + GAME_OVER chế độ Vô tận (điểm ván + điểm cao) trong `Core/Src/render.c` (phụ thuộc T073)
- [ ] T075 [US5/US6] Mở rộng MENU: chọn **chế độ** (Màn/Vô tận) + **đổi theme** (`theme_next`); `menu_sel` đa mục trong `Core/Src/game.c` (phụ thuộc T057)
- [ ] T076 [US5/US6] Tích hợp `store` trong `Core/Src/apptasks.c`/`freertos.c` USER CODE: `store_init` lúc boot → nạp `theme_id` cho render; khi đổi theme rời menu → `store_set_theme`+`store_commit`; ở GAME_OVER Vô tận nếu `score` > `endless_high` → `store_set_endless_high`+`store_commit` (phụ thuộc T070, T071, T075)
- [ ] T077 Thêm `theme.c`, `store.c` vào `C_SOURCES` (Makefile) + **dành riêng sector 12 Bank 2 (`0x08100000`, 16 KB) cho store** trong `STM32F429XX_FLASH.ld` (MEMORY riêng / `NOLOAD`, tránh trùng vùng code Bank 1); ghi chú thêm lại sau mỗi CubeMX Generate; `./build.sh` 0 error
- [ ] T078 Demo M8: menu chọn mode + theme → chơi Vô tận lập điểm cao → tắt/bật nguồn giữ điểm cao & theme; `make -C test` xanh + on-board (Acceptance US5/US6, FR-022..027)

### Implementation for User Story 7 (Pause nâng cao + Lưu/Tiếp tục ván)

- [ ] T079 [US7] Test host: round-trip `GameState` (copy byte → khôi phục → `game_step` cho kết quả y hệt); save→clear→`has_save`=false (mô phỏng bằng struct copy, không Flash) trong `test/test_game.c`
- [ ] T080 [US7] `store`: thêm `store_save_game`/`store_load_game`/`store_has_save`/`store_clear_save` — **2 ô lưu** theo `PlayMode`, version+crc, sai → coi như không có (hợp đồng [contracts/store.md](contracts/store.md)) trong `Core/Src/store.c` (phụ thuộc T071)
- [ ] T081 [US7] PAUSED → **menu 3 mục** (Tiếp tục / Lưu & Thoát / Thoát) + điều hướng joystick trong `Core/Src/game.c` (phụ thuộc T058)
- [ ] T082 [US7] MENU "Tiếp tục [chế độ]" khi `store_has_save` → `store_load_game` → PLAYING; "Lưu & Thoát" → `store_save_game`+về MENU; xóa ô lưu khi GAME_OVER/WIN (`store_clear_save`) trong `Core/Src/game.c` + `Core/Src/apptasks.c` (phụ thuộc T076, T080, T081)
- [ ] T083 [US7] Render menu PAUSED 3 mục + chỉ báo "Tiếp tục" ở MENU trong `Core/Src/render.c` (phụ thuộc T060)
- [ ] T084 [US7] Demo: chơi → Pause → Lưu & Thoát → tắt/bật nguồn → Tiếp tục đúng trạng thái; kết thúc → save tự mất; cả 2 mode độc lập; `make -C test` xanh + on-board (Acceptance US7, FR-028..032)

**Checkpoint**: Game đầy đủ 7 user story — 2 chế độ, 2 theme, lưu bền vững, pause + lưu/tiếp tục ván.

---

## Phase 8: Polish & Cross-Cutting Concerns

**Purpose**: Hoàn thiện đa-story.

- [ ] T085 [P] Hoàn thiện `docs/ui/ui-design.md` với bảng màu/font thực tế đã dùng + 2 theme
- [ ] T086 Rà soát dirty-rect & xác nhận **không nhấp nháy/không xé hình** bằng mắt (SC-003) trong `Core/Src/render.c`/`gfx.c`
- [ ] T087 Rà soát warning từ `./build.sh`, xử lý (NT VII — không tích luỹ warning)
- [ ] T088 [P] Cập nhật trạng thái mốc M1→M8 trong [AGENTS.md](../../AGENTS.md) §7
- [ ] T089 Đối chiếu **9 peripheral** (GPIO, ADC+DMA, **Timer**, Interrupt, LTDC, FMC/SDRAM, DMA2D, **Flash**, FreeRTOS — constitution §2) đều nghiệm thu được trong [quickstart.md](quickstart.md); bổ sung mục thiếu
- [ ] T090 Chạy trọn [quickstart.md](quickstart.md) end-to-end (M1→M8) làm nghiệm thu cuối

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: không phụ thuộc — bắt đầu ngay.
- **Foundational (Phase 2, M1–M2)**: phụ thuộc Setup — **CHẶN mọi user story**.
- **User Stories (Phase 3–6)**: đều phụ thuộc Foundational. Theo độ ưu tiên P1→P2→P3 (US1→US2→US3→US4)
  bám lộ trình M3→M7.
- **US5/US6/US7 (Phase 7, M8)**: sau Foundational; dùng `game_step`/MENU của US1+US4 và nhánh level của US2;
  US7 (pause+save/resume) dựa trên pause US4 + `store`.
- **Polish (Phase 8)**: sau khi các story mong muốn đã xong.

### User Story Dependencies

- **US1 (P1, M3)**: sau Foundational. Không phụ thuộc story khác → MVP.
- **US2 (P2, M4)**: sau Foundational. Dùng `occupied`/`game_step` của US1; chứa nhánh sân-đầy→thắng-màn
  (cần ST_LEVEL_COMPLETE/ST_WIN của US2). Test độc lập được.
- **US3 (P3, M5–M6)**: sau Foundational. Dùng cơ chế sinh lá của US1 + đồng hồ ms thực (TIM7) của M1.
- **US4 (P3, M7)**: sau Foundational. Bọc luồng MENU/PAUSE quanh PLAYING của US1; re-seed RNG tại Start.
- **US5 (P2, M8)**: thêm `play_mode` + nhánh ENDLESS vào `game_step` (sau US1/US2); `store` (Flash) độc lập.
- **US6 (P3, M8)**: `theme` cosmetic; `render` đọc theme (sau US1 render T036); MENU đổi theme (sau US4).
- **US7 (P3, M8)**: pause-menu mở rộng US4 (T058/T060) + `store` (T071/T080); MENU "Tiếp tục" sau US5 mode-select.

### Within Each User Story

- Test host viết trước/song song và phải FAIL trước khi hiện thực (luật core).
- `game.c` (logic) trước `render.c` (hiển thị); module thuần trước phần phụ thuộc HW.
- Mỗi story xong + build sạch trước khi sang story kế.

### Parallel Opportunities

- Setup: T001, T002, T004, T006 song song (khác file). T003 sau T001; T005 sau T002.
- Foundational: các luồng file độc lập song song — `rng.c` (T007), `gfx.c` (T008→T009–T012),
  `input.c` (T013→T014/T015), `apptasks.c` LED (T016). T017 (TIM, `.ioc`+`apptasks.c`/`it.c`) song song. Hợp
  lưu ở T018 (demo M1). Lưu ý T019 ghi `game.h`/`game.c` — chạy **sau** T003 (cùng `game.h`).
- Trong từng story: task `[P]` ở file khác (vd `levels.c` T041, `test_levels.c` T039) chạy song song.

---

## Parallel Example: Foundational (M1)

```bash
# Các luồng module độc lập (khác file) khởi động cùng lúc:
Task: "T007 rng.c — xorshift32"
Task: "T008 gfx.c — framebuffer + fill_rect + rgb565"
Task: "T013 input.c — ADC DMA + ánh xạ joystick"
Task: "T016 apptasks.c — LED + safe-stop"
Task: "T017 TIM7 (.ioc) — time-base ms + heartbeat"
# Sau khi xong (gồm T009 panel SPI5, T010–T012 gfx) → T018 demo M1 hợp lưu.
```

---

## Implementation Strategy

### MVP First (US1 = M3)

1. Phase 1 Setup → 2. Phase 2 Foundational (M1+M2, CHẶN tất cả) → 3. Phase 3 US1 (M3).
4. **STOP & VALIDATE**: chơi trọn 1 ván snake (quickstart M3) → demo MVP.

### Incremental Delivery (bám M1→M8)

Setup+Foundational → US1 (demo MVP) → US2 → US3 → US4 → US5/US6 (modes/theme/lưu). Mỗi mốc một bản nạp
được, không phá mốc trước (SC-005, NT VI).

---

## Traceability

### Mốc ↔ phase ↔ FR

| Mốc | Phase | FR chính |
|---|---|---|
| M1 | Foundational (T007–T018) | FR-018,019 (nền hiển thị/tín hiệu) |
| M2 | Foundational (T019–T027) | kiến trúc RTOS, SC-004 (tick ổn định) |
| M3 | US1 (T028–T038) | FR-001..006, 020 |
| M4 | US2 (T039–T046) | FR-007,008,009; SC-004 (tăng tốc qua màn) |
| M5–M6 | US3 (T047–T055) | FR-010,011,012,013 |
| M7 | US4 (T056–T062) | FR-014,015,016,017 |
| M8 | US5/US6/US7 (T069–T084) | FR-022..032 (modes, theme, lưu Flash, pause+save/resume) |
| — | Polish (T085–T090) | SC-003,005 + NT VII |

### Peripheral bắt buộc (constitution §2) ↔ task nghiệm thu

| Peripheral | Task thể hiện |
|---|---|
| GPIO | T014 (nút), T016 (LED) |
| ADC (+DMA) | T013 (joystick) |
| **Timer (TIM7)** | **T017** (time-base ms + heartbeat) |
| Interrupt | T011 (LTDC line), T017 (TIM update) |
| LTDC | T008, T011 |
| FMC/SDRAM | T008 (framebuffer) |
| DMA2D | T008–T012 |
| **Flash** | **T071** (store: lưu điểm cao + theme bền vững) |
| FreeRTOS | T020–T025 |

---

## Notes

- `[P]` = khác file, không phụ thuộc task chưa xong.
- Luật core (game/levels/rng) **phải test host** trước khi coi là xong (SC-006, NT II).
- Mọi code game đặt module riêng, chỉ gọi từ vùng USER CODE (NT III); thêm peripheral (TIM7) đi qua
  `.ioc` rồi Generate.
- Định nghĩa Done: `./build.sh` 0 error **và** mốc tương ứng chạy/quan sát được trên bo (NT VII).
- Commit sau mỗi task hoặc nhóm hợp lý; dừng ở checkpoint để nghiệm thu story độc lập.
