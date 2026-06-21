# Contract — Input (joystick + nút)

Module `input` (lớp chạm HAL: ADC1+DMA, GPIO). Biến tín hiệu thô thành `InputEvent` thuần cho logic.
Đảm bảo FR-002/003/015/016/020 và edge case joystick.

## API `input.h`

```c
void       input_init(void);                 // bắt đầu ADC DMA; hiệu chỉnh center (16 mẫu/trục)
InputEvent input_poll(void);                 // gọi @50Hz trong InputTask → sự kiện mới nhất
uint32_t   input_entropy(void);              // LSB nhiễu ADC tích luỹ — góp seed RNG (research §13)
```

`InputEvent` (xem [data-model.md](../data-model.md)): `kind ∈ {IN_NONE, IN_DIR, IN_SELECT}`.

## Ánh xạ joystick → hướng (FR-002)

- Đọc `buf[0]=VRx (PA5/ADC1_IN5)`, `buf[1]=VRy (PC3/ADC1_IN13)`, 12-bit (0..4095).
- `dx = VRx - center_x`, `dy = VRy - center_y` (center đo lúc `input_init`).
- **Deadzone**: nếu `|dx| < DEADZONE` và `|dy| < DEADZONE` → `IN_NONE` (đi thẳng — FR-020).
- **Trục trội + hysteresis**: chỉ phát `IN_DIR` khi trục trội vượt trục kia ≥ **1.3×** và vượt deadzone.
  - trục X trội: `dx>0 → DIR_RIGHT`, `dx<0 → DIR_LEFT`.
  - trục Y trội: dấu theo hiệu chỉnh (tránh đảo do đấu dây) → `DIR_UP`/`DIR_DOWN`.
- Lọc 180° **không** làm ở đây — `game` lọc theo `committed_dir` (tránh thủng bẫy gạt-2-lần).

## Nút (FR-015/016) — **một nút duy nhất**

| Nút | Chân | Sự kiện | Khi nào |
|---|---|---|---|
| JOY_SW | PB7 (pull-up, active-low) | `IN_SELECT` | nút chính — luôn phát `IN_SELECT` |

- **Một nút vật lý.** `input` luôn phát `IN_SELECT`; **ý nghĩa do FSM `game` quyết theo `mode`**
  (NT II — input không biết trạng thái):
  - `ST_PLAYING` → `IN_SELECT` = **Pause** (sang `ST_PAUSED`).
  - `ST_PAUSED` → `IN_SELECT` = **Resume** (về `ST_PLAYING`).
  - `ST_MENU` / `ST_GAME_OVER` / `ST_WIN` / `ST_LEVEL_COMPLETE` → `IN_SELECT` = **chọn/tiếp/chơi lại**.
- **Lý do bỏ nút B1:** bấm nút-trên-cần (JOY_SW) tách rời trục analog → không lệch cần; nút thứ 2 (B1/PA0)
  dễ gây va trục khi pause giữa lúc chơi và không thật sự cần khi FSM đã phân tách theo `mode`.
- **Debounce**: trạng thái nút phải ổn định ≥ `DEBOUNCE_MS (30ms)`.
- **Cạnh nhấn**: phát sự kiện 1 lần tại sườn nhấn (press-edge), không lặp khi giữ.

## Edge cases (spec)

- Joystick rút ra / hở → ADC trôi về ~giữa → rơi vào deadzone → `IN_NONE` → sâu đi thẳng, **không crash**.
- Nhiều input trong 1 tick game → InputTask đẩy vào queue, GameTask lấy **cái mới nhất**; chỉ 1 lệnh rẽ
  được áp mỗi bước.

## Lưu ý hiện thực — gotchas phần cứng (đã xác minh trên bo M1, xem [research.md §22](../research.md))

- **ADC sample-time để 480 chu kỳ** (đặt trong `input_init` qua `HAL_ADC_ConfigChannel`, regen-safe).
  Để mặc 3 chu kỳ với continuous+DMA circular → hoặc bão ngắt làm treo `HAL_Delay`, hoặc overrun làm ADC
  dừng (buffer đóng băng). **Giữ nguyên ngắt DMA2_Stream0** để có xử lý overrun.
- **Chiều trục tuỳ đấu dây**: bộ dây hiện tại cần `JOY_INVERT_Y=1`. 2 cờ `JOY_INVERT_X/Y` trong `input.c`.
- **Debounce thời gian thực** bằng `HAL_GetTick()` (≥`DEBOUNCE_MS`), cạnh-nhấn 1 lần; hàng chờ 1 ô để
  không mất sự kiện khi 2 nút đổi cùng lúc.

## Hợp đồng

- `input` KHÔNG chứa logic game; chỉ sinh `InputEvent`.
- `game` nhận `InputEvent` qua tham số → test được mà không cần ADC thật (NT II).
