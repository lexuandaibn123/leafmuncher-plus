# Phase 1 — Data Model: LeafMuncher+

Mô hình dữ liệu cho logic thuần (`game`/`levels`) + state chia sẻ giữa task. Tất cả struct dưới đây
**không chứa con trỏ HAL** và thao tác được trên PC (NT II). Kiểu minh hoạ; tên hằng số xem
[research.md](research.md).

---

## 1. Kiểu nền tảng

```c
typedef enum { DIR_UP, DIR_DOWN, DIR_LEFT, DIR_RIGHT } Dir;

typedef struct { int8_t c, r; } Cell;        // toạ độ lưới: c∈[0,19], r∈[0,12]

typedef enum {
  ST_MENU, ST_PLAYING, ST_PAUSED,
  ST_GAME_OVER, ST_LEVEL_COMPLETE, ST_WIN
} GameMode;

typedef enum {
  LEAF_NONE, LEAF_NORMAL, LEAF_GOLD, LEAF_POISON, LEAF_POWERUP
} LeafType;

typedef enum { PU_NONE, PU_SPEED, PU_SLOW, PU_GHOST, PU_PHASE } PowerType;

typedef enum { IN_NONE, IN_DIR, IN_SELECT, IN_PAUSE } InputKind;
typedef struct { InputKind kind; Dir dir; } InputEvent;   // dir chỉ dùng khi kind==IN_DIR
```

## 2. Entity

### 2.1 Worm (Sâu) — spec "Sâu"
Chuỗi đốt có thứ tự đầu→đuôi, lưu **ring buffer** ô + occupancy grid để va chạm O(1).

| Trường | Kiểu | Ý nghĩa |
|---|---|---|
| `body[260]` | `Cell` | ring buffer các đốt |
| `head_idx` | `uint16_t` | chỉ số đốt đầu trong `body` |
| `len` | `uint16_t` | độ dài hiện tại (`LEN_MIN..260`) |
| `dir` | `Dir` | **committed_dir** — hướng đã đi ở bước trước |
| `next_dir` | `Dir` | hướng chờ áp dụng ở bước kế (đã lọc 180°) |
| `grow_pending` | `uint8_t` | số đốt còn phải mọc thêm |

**Bất biến**: `LEN_MIN ≤ len ≤ 260`; không có 2 đốt trùng ô; `next_dir != opposite(dir)`.

### 2.2 Leaf / Mồi
Sân có **một mảng ô-bận** + tối đa: 1 lá thường, 1 lá vàng, 1 lá độc, 1 power-up cùng lúc.

| Trường | Kiểu | Ý nghĩa |
|---|---|---|
| `pos` | `Cell` | vị trí lưới |
| `type` | `LeafType` | thường / vàng / độc / power-up |
| `pu_type` | `PowerType` | loại hiệu ứng (chỉ khi `type==LEAF_POWERUP`) |
| `life_ms` | `int32_t` | thời gian còn lại trên sân; `<0` = vô hạn (lá thường, lá độc) |

**Bất biến (FR-013)**: `pos` luôn là ô trống — không đè thân sâu / chướng ngại / lá khác.

### 2.3 Power-up đang hiệu lực
Mảng cố định 4 phần tử (mỗi loại 1 slot) → stack độc lập (research §8).

| Trường | Kiểu | Ý nghĩa |
|---|---|---|
| `active[4]` | `int32_t` | thời gian còn lại theo `PowerType` (index = enum); `0` = tắt |

**Quy tắc**: ăn power-up loại `k` → `active[k] = PU_EFFECT_MS` (refresh). Mỗi tick trừ `step_ms`. Về `0`
thì tắt. GHOST không hạ về 0 khi đầu còn chồng thân (grace).

### 2.4 Level (Màn chơi)
Dữ liệu `const` trong `levels.c` (research §9).

| Trường | Kiểu | Ý nghĩa |
|---|---|---|
| `obstacles` | `uint8_t[13][20]` | bitmap: 1 = chướng ngại |
| `target_leaves` | `uint16_t` | số lá thường cần ăn để qua màn |
| `step_ms` | `uint16_t` | chu kỳ tick cơ bản của màn |

### 2.5 Game Session (Phiên chơi) — state gốc
Struct trung tâm mà `game_step` thao tác; cũng là snapshot cho render.

| Trường | Kiểu | Ý nghĩa |
|---|---|---|
| `mode` | `GameMode` | trạng thái máy trạng thái |
| `worm` | `Worm` | con sâu |
| `leaf_normal`, `leaf_gold`, `leaf_poison`, `leaf_pu` | `Leaf` | các lá hiện có (`type==LEAF_NONE` nếu trống) |
| `power` | power-up đang hiệu lực (§2.3) | |
| `occupied[13][20]` | `uint8_t` | ô bận: thân/chướng ngại/lá (tra O(1) cho va chạm & sinh lá) |
| `level_idx` | `uint8_t` | màn hiện tại `0..4` |
| `leaves_eaten` | `uint16_t` | số lá thường đã ăn trong **màn** (đối chiếu `target_leaves`) |
| `score` | `uint32_t` | điểm tích luỹ (clamp ≥ 0) |
| `step_ms` | `uint16_t` | chu kỳ tick hiệu dụng (sau hệ số power-up) |
| `menu_sel` | `uint8_t` | lựa chọn đang sáng ở MENU |
| `rng` | `uint32_t` | state PRNG (xorshift32) |

## 3. Máy trạng thái (FR-014)

```text
        ┌────────────────────────── (chọn "Chơi lại") ─────────────────────────┐
        │                                                                       │
     ┌──▼───┐  SELECT(Start)   ┌─────────┐  nút PAUSE        ┌────────┐         │
     │ MENU │ ───────────────► │ PLAYING │ ◄──────────────► │ PAUSED │         │
     └──────┘                  └────┬────┘   nút RESUME       └────────┘         │
        ▲                           │                                            │
        │                    ┌──────┴───────┬───────────────┐                    │
        │           va chạm  │  đạt target  │  đạt target    │                   │
        │          (chết)    │  (màn < cuối)│  (màn cuối)    │                   │
        │                    ▼              ▼                ▼                    │
        │              ┌───────────┐  ┌──────────────┐  ┌──────┐                 │
        └───SELECT─────│ GAME_OVER │  │LEVEL_COMPLETE│  │ WIN  │──SELECT─────────┘
                       └───────────┘  └──────┬───────┘  └──────┘  (về MENU)
                                             │ SELECT / tự động
                                             ▼
                                       PLAYING (màn kế, step_ms nhỏ hơn, sâu reset)
```

**Chuyển hợp lệ**:
- `MENU → PLAYING`: SELECT trên "Start" → reset session về level 0, score 0, sâu dài `LEN_START`.
- `PLAYING → PAUSED → PLAYING`: nút user (PA0) toggle; PAUSED dừng cập nhật game, vẫn giữ màn hình.
- `PLAYING → GAME_OVER`: đầu va biên/thân/chướng ngại (không có power-up phù hợp).
- `PLAYING → LEVEL_COMPLETE`: `leaves_eaten ≥ target_leaves` và còn màn sau → SELECT/tự động sang
  `PLAYING` màn kế (reset sâu, giữ score, `step_ms` màn mới).
- `PLAYING → WIN`: đạt target ở màn cuối.
- `GAME_OVER → MENU` / `WIN → MENU`: SELECT (FR-017 chơi lại = về MENU rồi Start, score 0).
- Edge: sân đầy (không còn ô trống sinh lá) → coi như đạt mục tiêu màn → LEVEL_COMPLETE/WIN.

## 4. Tín hiệu phần cứng (ngoài logic thuần)

| Tín hiệu | Trạng thái game | Cơ chế |
|---|---|---|
| LED xanh (PG13) | PLAYING / hoạt động bình thường (FR-019) | GPIO set ở `tasks`/`render` |
| LED đỏ (PG14) | GAME_OVER / lỗi khởi tạo HW | GPIO set; lỗi init HW → bật đỏ + dừng an toàn |

> Các trường con trỏ HAL, handle DMA2D/LTDC, framebuffer **không** nằm trong các struct trên — chúng ở
> lớp `gfx`/`tasks`. Ranh giới này giữ `game`/`levels`/`rng` thuần để host-test (NT II).

## 5. Layout bộ nhớ

| Vùng | Nơi | Kích thước |
|---|---|---|
| Framebuffer A/B | SDRAM `0xD0000000` / `0xD0025800` | 2 × 153.6 KB |
| `GameState` (gồm `worm.body`, `occupied`) | SRAM nội | ~2 KB |
| Font 8×16 + bảng màu + level bitmap | Flash (`const`) | < 4 KB |
| FreeRTOS heap_4 | SRAM nội | 32 KB (`configTOTAL_HEAP_SIZE`) |
