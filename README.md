# 🐛 LeafMuncher+ (Sâu Ăn Lá+)

Game **"sâu ăn lá"** lấy cảm hứng từ rắn săn mồi Nokia, làm bản mở rộng **"Plus"** chạy trên
kit **STM32F429I-DISC1** + **joystick analog**. Đồ án môn học Hệ thống nhúng.

> Điều khiển con sâu trên màn TFT màu, ăn lá để dài ra và ghi điểm, né tường/chướng ngại và
> chính thân mình — với nhiều loại lá, power-up và level kiểu "Plus".

---

## ✨ Tính năng

- 🎮 Điều khiển bằng **joystick analog** (mượt, có vùng chết, chặn quay đầu 180°).
- 🌿 **Nhiều loại lá:** lá thường (+điểm, dài ra), lá vàng (điểm cao, hiếm, tự biến mất), lá độc (gây hại).
- ⚡ **Power-up có thời hạn:** tăng tốc · làm chậm · bất tử (xuyên thân) · xuyên tường.
- 🧱 **Chướng ngại vật & nhiều level**, tốc độ tăng dần.
- 🖥️ Đồ hoạ **không nhấp nháy** (framebuffer SDRAM + DMA2D + double-buffer).
- 🧭 Menu / Pause / Game Over điều hướng bằng joystick.

## 🔧 Phần cứng

| Thành phần | Chi tiết |
|---|---|
| Bo | STM32F429I-DISC1 (STM32F429ZI, Cortex-M4 180MHz, SDRAM 8MB) |
| Màn hình | TFT 320×240 ILI9341 tích hợp (LTDC) |
| Điều khiển | Joystick analog 2 trục + nút (gắn ngoài) |

**Đấu nối joystick:** `VRx→PA5`, `VRy→PC3`, `SW→PB7`, `VCC→3V3`, `GND→GND`.

## 🛠️ Tech stack

STM32CubeMX (cấu hình → xuất **Makefile**) · **FreeRTOS** (CMSIS-RTOS v2) ·
HAL · build bằng **arm-none-eabi-gcc + make** (bundled trong STM32CubeIDE) · nạp qua ST-LINK.

## 🚀 Build & nạp

```bash
./build.sh          # biên dịch -> build/leafmuncher-plus.elf
./build.sh flash    # nạp xuống bo qua ST-LINK rồi reset
./build.sh clean    # xoá build/
```

Không cần cài thêm toolchain — `build.sh` tự dùng arm-gcc/make/STM32_Programmer_CLI có sẵn
trong STM32CubeIDE. Chi tiết: [docs/setup/02-build-flash.md](docs/setup/02-build-flash.md).
Tạo lại base project từ CubeMX: [docs/setup/01-cubemx-codegen.md](docs/setup/01-cubemx-codegen.md).

## 📁 Cấu trúc

```
leafmuncher-plus.ioc   # cấu hình CubeMX (sửa peripheral ở đây rồi Generate)
build.sh               # wrapper build/flash
Core/                  # code (HAL init do CubeMX quản lý + module game tự viết)
docs/setup/            # hướng dẫn CubeMX & build
specs/                 # đặc tả tính năng (Spec Kit)
.specify/              # constitution + templates (Spec Kit)
AGENTS.md              # hướng dẫn cho AI coding agent
```

Module game: `gfx` (vẽ) · `input` (joystick) · `game` (logic) · `render` · `rng` · `tasks` (FreeRTOS).

## 📐 Quy trình phát triển

Dự án dùng **Spec-Driven Development** với [Spec Kit](https://github.com/github/spec-kit):
`constitution → specify → plan → tasks → implement`. Lộ trình tăng dần **M1→M7** (luôn có bản chạy được),
ưu tiên hoàn thiện "rắn cổ điển" trước rồi thêm chướng ngại/lá/power-up.

- 📜 Luật dự án: [`.specify/memory/constitution.md`](.specify/memory/constitution.md)
- 📋 Spec: [`specs/001-leafmuncher-plus-game/spec.md`](specs/001-leafmuncher-plus-game/spec.md)

## 📌 Trạng thái

- ✅ Base project (CubeMX + Makefile + FreeRTOS), build sạch
- ✅ Constitution v1.0.0 + Spec hoàn chỉnh
- ⏭️ Tiếp theo: `/speckit-plan` → `/speckit-tasks` → hiện thực M1
