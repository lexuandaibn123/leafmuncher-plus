# Đấu nối Joystick → STM32F429I-DISC1

Hướng dẫn nối **module joystick analog 2 trục + nút nhấn** (loại phổ biến KY-023 / PS2-style)
vào kit **STM32F429I-DISC1**. Đây là **phần cứng ngoài duy nhất** của dự án — mọi thứ khác
(màn TFT, SDRAM, Flash, LED, nút B1, ST-LINK) đã có sẵn trên bo.

Chân đã chốt trong [`constitution.md §2`](../../.specify/memory/constitution.md) — **không đổi**.

## Bảng đấu dây

| Chân joystick | → | Chân STM32 | Chức năng | Cấu hình peripheral |
|---|---|---|---|---|
| **VCC** (hoặc `+5V`/`5V`) | → | **3V3** | Nguồn | — (dùng 3.3V, **không** cấp 5V) |
| **GND** | → | **GND** | Mát chung | — |
| **VRx** (trục ngang) | → | **PA5** | Trục X analog | `ADC1_IN5` |
| **VRy** (trục dọc) | → | **PC3** | Trục Y analog | `ADC1_IN13` |
| **SW** (nút nhấn cần) | → | **PB7** | Nút bấm joystick | `GPIO_Input` + **Pull-up** |

> 💡 Trên bo DISC1, mỗi chân được **in nhãn cổng** (PA5, PC3, PB7, 3V, GND) ngay trên silkscreen
> ở hai hàng header mở rộng (P1/P2) — cứ dò đúng nhãn để cắm, không cần tra số chân vật lý.

## Sơ đồ

```text
   MODULE JOYSTICK                         STM32F429I-DISC1
  ┌───────────────┐                       ┌────────────────────┐
  │           VCC ●───────────────────────● 3V3                 │
  │           GND ●───────────────────────● GND                 │
  │           VRx ●───────────────────────● PA5  (ADC1_IN5)     │
  │           VRy ●───────────────────────● PC3  (ADC1_IN13)    │
  │           SW  ●───────────────────────● PB7  (GPIO in + PU) │
  │  ╔═════╗      │                       │                     │
  │  ║  ↑  ║ cần  │     5 dây Dupont       │  [TFT 2.4" sẵn]     │
  │  ║←─o─→║      │     cái–cái            │  [B1=PA0  PAUSE]    │
  │  ║  ↓  ║      │                       │  [LD3 xanh PG13]    │
  │  ╚═════╝      │                       │  [LD4 đỏ  PG14]     │
  └───────────────┘                       └────────────────────┘
```

## Vị trí header vật lý (P1 / P2)

Bo DISC1 có **2 hàng header mở rộng**: **P1** và **P2** (mỗi hàng 32×2). Theo sơ đồ UM1670:

| Tín hiệu | Chân MCU | Header | Pin số |
|---|---|---|---|
| VRx | PA5 | **P2** | 21 |
| VRy | PC3 | **P2** | 15 |
| 3V3 | — | **P2** | 1–2 (và vài vị trí khác) |
| GND | — | **P2** | có sẵn |
| **SW** | **PB7** | **P1** | **24** |

> ⚠️ **PA5/PC3 ra header P2, còn PB7 ra header P1** — đây là do routing của bo (ST chọn cả 3 làm
> *Free I/O* nhưng không xếp cùng một header), **không** phải lỗi đấu dây.
>
> **Thực tế chỉ 1 dây vắt sang P1:** module joystick dùng **chung 1 VCC + 1 GND**, nên VCC/GND/VRx/VRy
> (4 dây) đều cắm ở **P2**; chỉ **dây tín hiệu SW** đi sang **P1 (PB7, pin 24)**. Đây là cấu hình đã
> chốt — gọn và ít rủi ro (xem constitution §2). *(Muốn cả 5 dây trên 1 header phải đổi 2 trục sang
> ADC3/PF3·PF5 trên P1 — không làm vì phải rework toàn bộ ADC.)*

## Vật tư

- 1 × module joystick analog 2 trục có nút (KY-023 hoặc tương đương).
- 5 × dây **Dupont cái–cái** (header bo là chân đực).
- (tuỳ chọn) breadboard để cố định joystick.
- Cáp **USB mini-B** cấp nguồn + nạp qua ST-LINK (thường kèm kit).

## Lưu ý kỹ thuật

1. **Cấp 3V3, KHÔNG cấp 5V.** ADC của STM32 đo 0–3.3V; cấp 5V có thể làm tín hiệu VRx/VRy vượt
   ngưỡng và sai/hỏng chân. Joystick chạy tốt ở 3.3V (chỉ là cầu phân áp + nút).
2. **Nút SW dùng Pull-up nội** (`GPIO_PULLUP`): khi **không nhấn** đọc mức **cao (1)**, khi **nhấn**
   nối GND → mức **thấp (0)**. Code debounce ≥30ms, bắt **cạnh nhấn** (1→0) → sự kiện `IN_SELECT`.
3. **Hiệu chỉnh tâm (center calibration):** lúc khởi động, joystick ở giữa cho ra ~điểm giữa thang
   ADC (chưa chắc đúng 2048). Firmware lấy trung bình 16 mẫu/trục làm "tâm", áp **deadzone** quanh tâm
   để cần ở giữa = đứng yên (xem `input` — T013).
4. **Trục trội (dominant axis):** ánh xạ về 4 hướng theo trục lệch nhiều hơn so với tâm; có hysteresis
   để không rung hướng ở vùng chéo.
5. **Chân cấm dùng** (bo đã chiếm cho LCD/SDRAM/gyro): `PA7, PC4, PC1, PC2, PA1, PA2`. Đừng đấu joystick
   vào các chân này.

## Kiểm tra nhanh sau khi đấu

- [ ] Cắm USB → nạp firmware (`./build.sh flash`).
- [ ] Gạt cần 4 hướng → ô vuông/đầu sâu di chuyển đúng hướng tương ứng.
- [ ] Thả cần về giữa → đối tượng đứng yên (deadzone hoạt động).
- [ ] Nhấn nút cần → có phản hồi (chọn menu / xác nhận).
- [ ] Rút 1 dây tín hiệu khi đang chạy → bo **không treo/không reset** (tín hiệu về mức giữa → đi thẳng).
