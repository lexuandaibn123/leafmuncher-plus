# AGENTS.md — LeafMuncher+ (Sâu Ăn Lá+)

Hướng dẫn cho AI coding agent làm việc trong repo này. Đây là **nguồn chuẩn duy nhất**;
`CLAUDE.md`/`GEMINI.md` chỉ trỏ về đây.

> ⚖️ Khi có xung đột: **chỉ thị trực tiếp của người dùng** > **`.specify/memory/constitution.md`** >
> file này > mặc định của agent. Constitution là luật dự án — đọc trước khi sửa kiến trúc.

---

## 1. Dự án là gì

Game nhúng **"Sâu Ăn Lá+"** (mở rộng từ rắn săn mồi Nokia) chạy trên kit
**STM32F429I-DISC1** + **joystick analog**. Là **đồ án môn học Hệ thống nhúng** — cần demo chạy
được và thể hiện rõ peripherals.

- **Spec đầy đủ:** `specs/001-leafmuncher-plus-game/spec.md`
- **Luật dự án:** `.specify/memory/constitution.md`
- **Hướng dẫn phần cứng/build:** `docs/setup/`

## 2. Phần cứng

- **MCU/Bo:** STM32F429ZI trên STM32F429I-DISC1 (Cortex-M4 180MHz, 2MB Flash, 256KB RAM, SDRAM 8MB).
- **Màn hình:** TFT 320×240 ILI9341 tích hợp, qua **LTDC** + SDRAM framebuffer.
- **Joystick (chân đã chốt — KHÔNG đổi nếu không sửa constitution):**

  | Tín hiệu | Chân | Cấu hình |
  |---|---|---|
  | VRx | **PA5** | ADC1_IN5 (Rank 1) |
  | VRy | **PC3** | ADC1_IN13 (Rank 2) |
  | SW (nút) | **PB7** | GPIO_Input + Pull-up |

  Chân **cấm dùng** (bo đã chiếm): PA7, PC4, PC1, PC2, PA1, PA2.
- **LED:** xanh = OK, đỏ = lỗi/game over.

## 3. Tech stack (CỐ ĐỊNH)

- Cấu hình peripheral + sinh code init: **STM32CubeMX** → xuất project kiểu **Makefile**.
- Build: **arm-none-eabi-gcc + make** (bundled trong STM32CubeIDE, `build.sh` tự dò đường dẫn).
- Nạp: **STM32_Programmer_CLI** / OpenOCD qua ST-LINK tích hợp.
- RTOS: **FreeRTOS qua CMSIS-RTOS v2** (heap_4).
- Đồ hoạ: **framebuffer SDRAM + DMA2D + double-buffer**.

KHÔNG đổi IDE/toolchain/RTOS. Đổi peripheral phải qua `.ioc` rồi Generate, không sửa tay code init.

## 4. Lệnh

```bash
./build.sh          # biên dịch -> build/leafmuncher-plus.elf (.hex/.bin)
./build.sh flash    # nạp xuống bo qua ST-LINK rồi reset
./build.sh clean    # xoá build/
```

> Agent có thể chạy `./build.sh` để tự kiểm lỗi biên dịch. Nạp/chạy thật trên bo do người dùng làm.

## 5. Cấu trúc repo

```
leafmuncher-plus.ioc      # nguồn cấu hình CubeMX (sửa peripheral ở đây rồi Generate)
Makefile                  # do CubeMX sinh
build.sh                  # wrapper build/flash dùng toolchain bundled
Core/Inc, Core/Src        # code (file CubeMX quản lý + module game tự viết)
Drivers/                  # HAL + CMSIS (không sửa)
Middlewares/              # FreeRTOS, USB host (không sửa)
docs/setup/               # 01-cubemx-codegen.md, 02-build-flash.md
specs/001-.../            # spec.md, checklists/ (Spec Kit)
.specify/                 # constitution, templates, scripts, feature.json
```

Module game (tự viết, đặt trong `Core/Src` & `Core/Inc`):
`gfx` (vẽ framebuffer) · `input` (joystick) · `game` (logic) · `render` (state→gfx) ·
`rng` (ngẫu nhiên) · `tasks` (FreeRTOS).

## 6. Quy tắc code (rút từ constitution — đọc bản đầy đủ ở `.specify/memory/constitution.md`)

1. **Tech stack cố định** — xem mục 3.
2. **Tách logic khỏi phần cứng** — `game.c`/`levels.c`/`rng.c` KHÔNG gọi HAL/CMSIS/FreeRTOS; phải unit-test được trên PC.
3. **Bảo toàn code khi regenerate** — code tự viết đặt trong module riêng, chỉ *gọi* từ vùng `/* USER CODE BEGIN/END */`. KHÔNG sửa ngoài vùng đó trong file CubeMX sinh.
4. **Cô lập module** — mỗi file một trách nhiệm, header lộ tối thiểu.
5. **Đồ hoạ không nhấp nháy** — framebuffer + DMA2D + double-buffer; không vẽ lên buffer đang quét.
6. **Luôn có bản chạy được** — phát triển theo M1→M7, mỗi mốc demo được; ưu tiên M3 (rắn cổ điển) trước.
7. **Build sạch là bắt buộc** — `./build.sh` 0 error mới coi là xong; tuyên bố "đã sửa/chạy" phải kèm bằng chứng.

## 7. Quy trình (Spec-Driven Development — Spec Kit)

`/speckit-constitution` → `/speckit-specify` → *(tuỳ chọn `/speckit-clarify`)* → `/speckit-plan`
→ `/speckit-tasks` → `/speckit-implement`.

Trạng thái hiện tại: ✅ constitution v1.0.0, ✅ spec (`specs/001-leafmuncher-plus-game`).
Bước kế: `/speckit-plan`.

## 8. Bẫy thường gặp

- CubeMX: muốn bật **DMA Continuous Requests** thì phải **Add DMA cho ADC1 trước** (nếu không sẽ bị xám).
- Đổi chân/peripheral trong CubeMX rồi Generate sẽ **ghi đè** mọi thứ ngoài vùng USER CODE → đừng để logic ở đó.
- Bo F429I-DISC1 **không có** audio codec (khác F407-Disco); âm thanh là stretch goal.
- Joystick rút ra → ADC về mức giữa → coi như không bẻ lái (đừng để crash).
