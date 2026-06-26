#ifndef GAME_H
#define GAME_H

/* game — logic thuần (state + game_step). KHÔNG gọi HAL/CMSIS/FreeRTOS (Nguyên tắc II).
 * Header chia sẻ: kiểu nền tảng + hằng số + API logic. Hợp đồng: contracts/game-core.md.
 * Hằng số lấy từ research.md (Tổng hợp hằng số); kiểu từ data-model.md §1–§2. */

#include <stdint.h>
#include <stdbool.h>

/* ===================== Hình học lưới (research §1) ===================== */
#define SCREEN_W    320
#define SCREEN_H    240
#define HUD_H       32
#define CELL        16
#define COLS        20
#define ROWS        13
#define GRID_CELLS  (COLS * ROWS)   /* 260 ô chơi */

/* ===================== Sâu & độ dài (research §4) ===================== */
#define LEN_START   3
#define LEN_MIN     3
#define WORM_CAP    GRID_CELLS      /* sức chứa ring buffer = 260 */

/* ===================== Tốc độ / tick (research §3) ===================== */
#define LEVELS      5
#define STEP_MS_MIN 70
#define STEP_MS_MAX 400
#define SPEED_FACTOR 0.6f
#define SLOW_FACTOR  1.7f
#define PU_EFFECT_MS 6000

/* Bảng tick & mục tiêu mỗi level — ĐỊNH NGHĨA trong levels.c (T041). */
extern const uint16_t STEP_MS[LEVELS];        /* {180,155,130,110,95} */
extern const uint16_t TARGET_LEAVES[LEVELS];  /* {6,8,10,12,14} */

/* ===================== Điểm & lá (research §5,§6) ===================== */
#define SCORE_LEAF        10
#define SCORE_GOLD        50
#define POISON_SHRINK     2
#define POISON_PENALTY    20
#define GOLD_CHANCE_PCT   40
#define GOLD_LIFE_MS      8000
#define POISON_CHANCE_PCT 20       /* mở khoá từ level >= 2 */
#define PU_CHANCE_PCT     12       /* mở khoá từ level >= 3 */
#define PU_LIFE_MS        10000

/* ===================== Endless (research §18) ===================== */
#define ENDLESS_STEP0      180
#define ENDLESS_STEP_DEC   3
#define ENDLESS_RAMP_EVERY 3

/* ===================== Input (research §10,§11) ===================== */
#define DEADZONE     500
#define INPUT_HZ     50
#define DEBOUNCE_MS  30

/* ===================== Kiểu nền tảng (data-model §1) ===================== */
typedef enum { DIR_UP, DIR_DOWN, DIR_LEFT, DIR_RIGHT } Dir;

typedef struct { int8_t c, r; } Cell;   /* c∈[0,COLS), r∈[0,ROWS) */

typedef enum {
  ST_MENU, ST_PLAYING, ST_PAUSED,
  ST_GAME_OVER, ST_LEVEL_COMPLETE, ST_WIN
} GameMode;                              /* trạng thái máy trạng thái (FSM) */

typedef enum { MODE_LEVEL, MODE_ENDLESS } PlayMode;               /* chế độ chơi (US5) */
typedef enum { THEME_FOREST, THEME_DESERT, THEME_COUNT } ThemeId; /* theme (US6) */

typedef enum {
  LEAF_NONE, LEAF_NORMAL, LEAF_GOLD, LEAF_POISON, LEAF_POWERUP
} LeafType;

typedef enum { PU_NONE, PU_SPEED, PU_SLOW, PU_GHOST, PU_PHASE } PowerType;
#define PU_KINDS 4   /* số power-up có đồng hồ: SPEED/SLOW/GHOST/PHASE (index = PowerType-1) */

/* Một nút vật lý (JOY_SW) → IN_SELECT = "nút chính". Ý nghĩa do FSM quyết theo mode:
 * PLAYING→pause, PAUSED→resume, MENU/GAME_OVER/WIN/LEVEL_COMPLETE→chọn/tiếp. */
typedef enum { IN_NONE, IN_DIR, IN_SELECT } InputKind;
typedef struct { InputKind kind; Dir dir; } InputEvent;   /* dir chỉ dùng khi kind==IN_DIR */

/* ===================== Entity (data-model §2) ===================== */
typedef struct {
  Cell     body[WORM_CAP];   /* ring buffer các đốt (đầu→đuôi) */
  uint16_t head_idx;         /* chỉ số đốt đầu trong body */
  uint16_t len;              /* LEN_MIN..WORM_CAP */
  Dir      dir;              /* committed_dir — hướng đã đi ở bước trước */
  Dir      next_dir;         /* hướng chờ áp dụng ở bước kế (đã lọc 180°) */
  uint8_t  grow_pending;     /* số đốt còn phải mọc thêm */
} Worm;

typedef struct {
  Cell      pos;
  LeafType  type;
  PowerType pu_type;   /* chỉ khi type==LEAF_POWERUP */
  int32_t   life_ms;   /* thời gian còn lại; <0 = vô hạn */
} Leaf;

typedef struct {
  GameMode  mode;                    /* trạng thái FSM */
  PlayMode  play_mode;               /* MODE_LEVEL / MODE_ENDLESS (US5) */
  Worm      worm;
  Leaf      leaf_normal, leaf_gold, leaf_poison, leaf_pu;
  int32_t   power[PU_KINDS];         /* ms còn lại theo (PowerType-1); 0 = tắt */
  uint8_t   occupied[ROWS][COLS];    /* ô bận: thân/chướng ngại/lá (va chạm & sinh lá O(1)) */
  uint8_t   level_idx;               /* màn hiện tại 0..LEVELS-1 */
  uint16_t  leaves_eaten;            /* lá thường đã ăn trong màn */
  uint32_t  score;                   /* điểm tích luỹ (clamp >= 0) */
  uint16_t  step_ms;                 /* nhịp tick CƠ BẢN của màn (hệ số power-up áp ở game_step_ms) */
  uint8_t   menu_sel;                /* lựa chọn đang sáng ở MENU/PAUSED */
  uint8_t   theme_id;                /* ThemeId hiển thị (COSMETIC — game_step bỏ qua; chỉ render đọc) */
  uint32_t  rng;                     /* state PRNG (xorshift32) */
  /* US7 — cờ ô lưu (EXTERNAL: tasks set từ store, game/render chỉ ĐỌC để dựng MENU; game_step bỏ qua). */
  uint8_t   has_save[2];             /* ô lưu khả dụng theo PlayMode (MODE_LEVEL/MODE_ENDLESS) */
  uint8_t   save_request;            /* PAUSED "Lưu & Thoát" → tasks lưu Flash rồi về MENU (transient) */
  uint8_t   load_request;            /* MENU "Tiếp tục" → tasks nạp ô lưu vào state (transient) */
  uint8_t   from_save;               /* ván hiện tại khôi phục từ ô lưu? (tasks set; xóa ô lưu khi kết thúc) */
} GameState;

/* MENU động (US7): có thể chèn mục "Tiếp tục" theo ô lưu khả dụng. game.c & render.c cùng gọi
 * game_menu_items để dựng DANH SÁCH MỤC y hệt nhau (tránh lệch chỉ số). */
typedef enum {
  MI_CONTINUE_LEVEL, MI_CONTINUE_ENDLESS, MI_START, MI_ENDLESS, MI_THEME
} MenuItemId;
#define MENU_MAX_ITEMS 5

/* ===================== Sự kiện (contracts/game-core.md) ===================== */
typedef uint16_t GameEvents;   /* bitmask trả về từ game_step */
#define EV_MOVED        (1u<<0)
#define EV_ATE_NORMAL   (1u<<1)
#define EV_ATE_GOLD     (1u<<2)
#define EV_ATE_POISON   (1u<<3)
#define EV_ATE_POWERUP  (1u<<4)
#define EV_LEAF_EXPIRED (1u<<5)   /* lá vàng/power-up hết hạn, biến mất */
#define EV_GAME_OVER    (1u<<6)
#define EV_LEVEL_DONE   (1u<<7)
#define EV_WIN          (1u<<8)
#define EV_DIR_BLOCKED  (1u<<9)   /* input 180° bị bỏ qua */

/* ===================== API logic thuần (contracts/game-core.md) ===================== */
void       game_init(GameState *gs, uint32_t seed);            /* về MENU, sâu LEN_START, seed RNG */
void       game_start(GameState *gs);                          /* bắt đầu ván từ MENU */
GameEvents game_step(GameState *gs, InputEvent in, uint16_t dt_ms); /* một bước logic */
void       game_input_ui(GameState *gs, InputEvent in);        /* điều hướng MENU/PAUSED/GAME_OVER/WIN */
int        game_menu_items(const GameState *gs, MenuItemId out[MENU_MAX_ITEMS]); /* dựng MENU động → số mục */
LeafType   game_cell_content(const GameState *gs, Cell c);     /* nội dung ô (chỉ đọc) */
uint16_t   game_step_ms(const GameState *gs);                  /* tick hiệu dụng hiện tại */

#endif /* GAME_H */
