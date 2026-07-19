# 🐛 LeafMuncher+ (Sâu Ăn Lá+)

Game **"sâu ăn lá"** lấy cảm hứng từ rắn săn mồi Nokia, làm bản mở rộng **"Plus"** chạy trên
kit **STM32F429I-DISC1** + **joystick analog**. Đồ án môn học Hệ thống nhúng.

> Điều khiển con sâu trên màn TFT màu, ăn lá để dài ra và ghi điểm, né tường/chướng ngại và
> chính thân mình — với nhiều loại lá, power-up và các màn chơi nâng cao.

---

## ✨ Tính năng

- 🎮 Điều khiển bằng **joystick analog** (mượt, có vùng chết, chặn quay đầu 180°).
- 🌿 **Nhiều loại lá:** lá thường (+điểm, dài ra), lá vàng (điểm cao, hiếm, tự biến mất), lá độc (gây hại).
- ⚡ **Power-up có thời hạn:** tăng tốc · làm chậm · bất tử (xuyên thân) · xuyên tường.
- 🧱 **Chướng ngại vật & nhiều level**, tốc độ tăng dần.
- 🖥️ Đồ hoạ **không nhấp nháy** (framebuffer SDRAM + DMA2D + double-buffer).
- 🧭 Menu / Pause / Game Over điều hướng bằng joystick.
- 💾 LƯU TRỮ (Flash): Tự động lưu điểm cao nhất ở chế độ vô tận và tính năng lưu/tiếp tục ván chơi đang dở.

## 🔧 Phần cứng

| Thành phần | Chi tiết |
|---|---|
| Bo | STM32F429I-DISC1 (STM32F429ZI, Cortex-M4 180MHz, SDRAM 8MB) |
| Màn hình | TFT 320×240 ILI9341 tích hợp (LTDC) |
| Điều khiển | Joystick analog 2 trục + nút (gắn ngoài) |

**Đấu nối joystick:** `VRx→PA5`, `VRy→PC3`, `SW→PB7`, `VCC→3V3`, `GND→GND`.

## 🛠️ Công nghệ sử dụng

- STM32CubeMX (cấu hình phần cứng)
- **FreeRTOS** (CMSIS-RTOS v2) quản lý đa luồng.
- Giao tiếp ngoại vi thông qua thư viện HAL.
- Build system: **arm-none-eabi-gcc + make**

## 🚀 Hướng dẫn biên dịch & Nạp code

Dự án có sẵn script `build.sh` bọc các lệnh cơ bản:

```bash
./build.sh          # biên dịch -> build/leafmuncher-plus.elf
./build.sh flash    # nạp xuống bo qua ST-LINK rồi reset
./build.sh clean    # xoá build/
```

Script sẽ tự động tìm kiếm đường dẫn của `arm-none-eabi-gcc` nằm trong STM32CubeIDE.
Chi tiết xem thêm tại: [docs/setup/02-build-flash.md](docs/setup/02-build-flash.md).

## 📁 Cấu trúc thư mục

```
leafmuncher-plus.ioc   # file cấu hình STM32CubeMX
build.sh               # script build tự động
Core/                  # mã nguồn chính (Driver, Logic game, Đồ họa)
docs/                  # tài liệu hướng dẫn và báo cáo
```

Các module game tự phát triển (nằm trong `Core/Src` và `Core/Inc`):
- `gfx`: Xử lý vẽ đồ họa (layer LCD, màu sắc, hình khối)
- `input`: Đọc tín hiệu ADC từ Joystick và xử lý bộ lọc (debounce).
- `game`: Chứa toàn bộ logic thuần của game (vị trí, va chạm, tính điểm).
- `render`: Chuyển đổi trạng thái game thành các lệnh vẽ đồ họa.
- `store`: Đọc/Ghi dữ liệu điểm và trạng thái vào bộ nhớ Flash.
- `apptasks`: Quản lý hệ thống task của RTOS.

## 📌 Trạng thái đồ án
- ✅ Đã hoàn thiện toàn bộ tính năng và báo cáo.
