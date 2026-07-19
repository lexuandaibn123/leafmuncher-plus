#ifndef GAME_H
#define GAME_H

/* Chứa toàn bộ các cấu trúc dữ liệu và logic điều khiển trò chơi.
 * Tách biệt hoàn toàn với phần cứng và hệ điều hành. */

#include <stdint.h>
#include <stdbool.h>

/* ===================== Hình học lưới ===================== */
#define SCREEN_W    320
#define SCREEN_H    240
#define HUD_H       32
#define CELL        16
#define COLS        20
#define ROWS        13
#define GRID_CELLS  (COLS * ROWS)

/* ===================== Sâu & độ dài ===================== */
#define LEN_START   3
#define LEN_MIN     3
#define WORM_CAP    GRID_CELLS

/* ===================== Tốc độ / tick ===================== */
#define LEVELS      5
#define STEP_MS_MIN 70
#define STEP_MS_MAX 400
#define SPEED_FACTOR 0.6f
#define SLOW_FACTOR  1.7f
#define PU_EFFECT_MS 6000

/* Bảng tick & mục tiêu mỗi level — định nghĩa ở levels.c */
extern const uint16_t STEP_MS[LEVELS];
extern const uint16_t TARGET_LEAVES[LEVELS];

/* ===================== Điểm & lá ===================== */
#define SCORE_LEAF        10
#define SCORE_GOLD        50
#define POISON_SHRINK     2
#define POISON_PENALTY    20
#define GOLD_CHANCE_PCT   40
#define GOLD_LIFE_MS      8000
#define POISON_CHANCE_PCT 20
#define PU_CHANCE_PCT     12
#define PU_LIFE_MS        10000

/* ===================== Chế độ vô tận ===================== */
#define ENDLESS_STEP0      180
#define ENDLESS_STEP_DEC   3
#define ENDLESS_RAMP_EVERY 3

/* ===================== Input ===================== */
#define DEADZONE     500
#define INPUT_HZ     50
#define DEBOUNCE_MS  30

/* ===================== Cấu trúc dữ liệu ===================== */
typedef enum { DIR_UP, DIR_DOWN, DIR_LEFT, DIR_RIGHT } Dir;

typedef struct { int8_t c, r; } Cell;

typedef enum {
  ST_MENU, ST_PLAYING, ST_PAUSED,
  ST_GAME_OVER, ST_LEVEL_COMPLETE, ST_WIN
} GameMode;

typedef enum { MODE_LEVEL, MODE_ENDLESS } PlayMode;

typedef enum {
  LEAF_NONE, LEAF_NORMAL, LEAF_GOLD, LEAF_POISON, LEAF_POWERUP
} LeafType;

typedef enum { PU_NONE, PU_SPEED, PU_SLOW, PU_GHOST, PU_PHASE } PowerType;
#define PU_KINDS 4

typedef enum { IN_NONE, IN_DIR, IN_SELECT } InputKind;
typedef struct { InputKind kind; Dir dir; } InputEvent;

/* ===================== Thực thể trong game ===================== */
typedef struct {
  Cell     body[WORM_CAP];   /* Vòng lặp các đốt (đầu→đuôi) */
  uint16_t head_idx;         /* Chỉ số đốt đầu trong body */
  uint16_t len;
  Dir      dir;
  Dir      next_dir;
  uint8_t  grow_pending;
} Worm;

typedef struct {
  Cell      pos;
  LeafType  type;
  PowerType pu_type;
  int32_t   life_ms;
} Leaf;

typedef struct {
  GameMode  mode;
  PlayMode  play_mode;
  Worm      worm;
  Leaf      leaf_normal, leaf_gold, leaf_poison, leaf_pu;
  int32_t   power[PU_KINDS];
  uint8_t   occupied[ROWS][COLS];
  uint8_t   level_idx;
  uint16_t  leaves_eaten;
  uint32_t  score;
  uint16_t  step_ms;
  uint8_t   menu_sel;
  uint32_t  rng;
  uint8_t   has_save[2];
  uint8_t   save_request;
  uint8_t   load_request;
  uint8_t   from_save;
} GameState;

/* MENU động: có thể chèn mục "Tiếp tục" theo ô lưu khả dụng. */
typedef enum {
  MI_CONTINUE_LEVEL, MI_CONTINUE_ENDLESS, MI_START, MI_ENDLESS
} MenuItemId;
#define MENU_MAX_ITEMS 4

/* ===================== Sự kiện ===================== */
typedef uint16_t GameEvents;
#define EV_MOVED        (1u<<0)
#define EV_ATE_NORMAL   (1u<<1)
#define EV_ATE_GOLD     (1u<<2)
#define EV_ATE_POISON   (1u<<3)
#define EV_ATE_POWERUP  (1u<<4)
#define EV_LEAF_EXPIRED (1u<<5)
#define EV_GAME_OVER    (1u<<6)
#define EV_LEVEL_DONE   (1u<<7)
#define EV_WIN          (1u<<8)
#define EV_DIR_BLOCKED  (1u<<9)

/* ===================== API logic thuần ===================== */
void       game_init(GameState *gs, uint32_t seed);
void       game_start(GameState *gs);
GameEvents game_step(GameState *gs, InputEvent in, uint16_t dt_ms);
void       game_input_ui(GameState *gs, InputEvent in);
int        game_menu_items(const GameState *gs, MenuItemId out[MENU_MAX_ITEMS]);
LeafType   game_cell_content(const GameState *gs, Cell c);
uint16_t   game_step_ms(const GameState *gs);

#endif /* GAME_H */
