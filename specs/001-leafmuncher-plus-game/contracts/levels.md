# Contract — Levels (dữ liệu màn chơi)

Module `levels` (thuần, `const` trong flash). Cung cấp bố cục chướng ngại + mục tiêu + tốc độ cho `game`.
Đảm bảo FR-007/008/009.

## API `levels.h`

```c
#define LEVEL_COUNT 5

typedef struct {
  const uint8_t (*obstacles)[20];  // trỏ tới mảng [13][20], 1 = chướng ngại
  uint16_t target_leaves;          // số lá thường cần ăn để qua màn
  uint16_t step_ms;                // chu kỳ tick cơ bản của màn
} Level;

const Level* level_get(uint8_t idx);   // idx 0..LEVEL_COUNT-1; NULL nếu ngoài phạm vi
uint8_t      level_is_last(uint8_t idx);
```

## Bảng level (research §9) *(tunable)*

| idx | Tên | `target_leaves` | `step_ms` | Layout chướng ngại |
|---|---|---|---|---|
| 0 | Khởi động | 6 | 180 | trống |
| 1 | Hai thanh | 8 | 155 | 2 thanh ngang ngắn |
| 2 | Chữ thập | 10 | 130 | dấu cộng giữa sân |
| 3 | Bốn góc | 12 | 110 | 4 khối góc |
| 4 | Mê cung | 14 | 95 | mê cung thưa |

## Bất biến

1. Mỗi bitmap đúng **13 hàng × 20 cột**; giá trị ∈ {0,1}.
2. Phải chừa đủ ô trống cho sâu khởi đầu (3 đốt ở giữa, hướng phải) + ít nhất 1 ô sinh lá → **ô giữa sân
   và lân cận luôn trống**.
3. `step_ms` giảm dần theo `idx` (FR-008), không nhỏ hơn `STEP_MS_MIN`.
4. `level_get(idx>=LEVEL_COUNT)` trả `NULL`; `game` coi như đã WIN khi vượt màn cuối.

## Test (host)

- `target_leaves` > 0 và `step_ms` đơn điệu giảm.
- Ô khởi đầu sâu + lân cận = trống ở mọi level.
- Tổng ô trống ≥ `LEN_START + 1` ở mọi level.
