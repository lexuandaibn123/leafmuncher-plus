# Contract — Game Core (logic thuần)

Module `game` (+ `levels`, `rng`). **KHÔNG gọi HAL/CMSIS/FreeRTOS** (NT II). Đây là hợp đồng mà unit-test
host và lớp `tasks` dựa vào. Kiểu xem [data-model.md](../data-model.md).

## API (`game.h`)

```c
// Khởi tạo phiên mới: về MENU, sâu LEN_START, score 0, level 0, seed RNG.
void game_init(GameState* gs, uint32_t seed);

// Bắt đầu ván từ MENU (gọi khi SELECT "Start"): nạp level 0, reset sâu/score/lá.
void game_start(GameState* gs);

// Một bước logic. `in` = input mới nhất kể từ bước trước (IN_NONE nếu không có).
// `dt_ms` = step_ms hiệu dụng đã trôi (để hạ đồng hồ lá/power-up).
// Trả về bitmask sự kiện đã xảy ra trong bước (để render/HW phản ứng: âm/LED/dirty-rect).
GameEvents game_step(GameState* gs, InputEvent in, uint16_t dt_ms);

// Áp input ở MENU/PAUSED/GAME_OVER/WIN (điều hướng, không phải bước chơi).
void game_input_ui(GameState* gs, InputEvent in);

// Truy vấn cho render (chỉ đọc).
LeafType game_cell_content(const GameState* gs, Cell c); // gì đang ở ô (thân/lá/chướng ngại/trống)
uint16_t game_step_ms(const GameState* gs);              // chu kỳ tick hiệu dụng hiện tại
```

```c
typedef uint16_t GameEvents;            // bitmask
#define EV_MOVED        (1u<<0)
#define EV_ATE_NORMAL   (1u<<1)
#define EV_ATE_GOLD     (1u<<2)
#define EV_ATE_POISON   (1u<<3)
#define EV_ATE_POWERUP  (1u<<4)
#define EV_LEAF_EXPIRED (1u<<5)         // lá vàng/power-up hết hạn, biến mất
#define EV_GAME_OVER    (1u<<6)
#define EV_LEVEL_DONE   (1u<<7)
#define EV_WIN          (1u<<8)
#define EV_DIR_BLOCKED  (1u<<9)         // input 180° bị bỏ qua
```

## Bất biến (hậu điều kiện của `game_step`, ST_PLAYING)

1. `LEN_MIN ≤ worm.len ≤ 260`.
2. Không có 2 đốt sâu trùng ô; `occupied` đồng nhất với thân + chướng ngại + lá.
3. `worm.next_dir != opposite(worm.dir)` — không bao giờ quay đầu 180° (FR-003).
4. Mọi lá có `type != LEAF_NONE` đứng trên ô trống (không đè thân/chướng ngại/lá khác) (FR-013).
5. `score ≥ 0`.
6. Đồng hồ: `0 ≤ leaf_gold.life_ms`, `0 ≤ power.active[k]`; về 0 thì hiệu lực/lá tắt (trừ grace GHOST).

## Quy tắc hành vi (ánh xạ FR & test SC-006)

| Tình huống | Kết quả | FR |
|---|---|---|
| Đầu vào ô lá thường | `len+1`, `score+10`, sinh lá thường mới ở ô trống ngẫu nhiên | FR-004 |
| Đầu vào ô lá vàng | `score+50`, `len+1`, lá vàng biến mất | FR-010 |
| Lá vàng quá `GOLD_LIFE_MS` chưa ăn | biến mất (`EV_LEAF_EXPIRED`) | FR-010 |
| Đầu vào ô lá độc | co 2 đốt (sàn `LEN_MIN`; nếu ở sàn thì `−20` điểm), KHÔNG Game Over | FR-011 |
| Đầu vào ô power-up | `power.active[type]=PU_EFFECT_MS`; cùng loại = refresh | FR-012 |
| Đầu chạm biên | GAME_OVER, trừ khi PHASE bật → wrap cạnh đối diện | FR-005, FR-012 |
| Đầu chạm thân (trừ ô đuôi sẽ nhả) | GAME_OVER, trừ khi GHOST bật | FR-005, FR-012 |
| Đầu chạm chướng ngại | GAME_OVER (PHASE/GHOST không xuyên chướng ngại ở v1) | FR-005 |
| Input 180° so với `committed_dir` | bỏ qua, giữ hướng (`EV_DIR_BLOCKED`) | FR-003 |
| Joystick deadzone (`IN_NONE`) | đi thẳng theo `dir` | FR-020 |
| `leaves_eaten ≥ target` & còn màn | LEVEL_COMPLETE rồi sang màn kế, `step_ms` nhỏ hơn | FR-007,008 |
| `leaves_eaten ≥ target` & màn cuối | WIN | FR-009 |
| Sân không còn ô trống để sinh lá | coi như thắng màn | edge spec |

## Tính xác định (cho test)

- Mọi ngẫu nhiên đi qua `rng` (state trong `GameState.rng`). Cùng seed + cùng chuỗi input + cùng `dt_ms`
  ⇒ cùng kết quả → test tái lập được.
- `game_step` không đọc đồng hồ hệ thống; thời gian vào qua `dt_ms`.
