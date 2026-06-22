#include "game.h"
#include "rng.h"
#include "levels.h"
#include <string.h>

/* MENU có 3 mục: 0=START (chơi Màn), 1=ENDLESS (chơi Vô tận — US5), 2=THEME ("SOON" — US6). */
#define MENU_ITEMS    3
#define MENU_START    0
#define MENU_ENDLESS  1
#define MENU_THEME    2

/* game — logic thuần (Nguyên tắc II: KHÔNG gọi HAL/CMSIS/FreeRTOS).
 * T019: khởi tạo phiên + truy vấn đọc-chỉ. game_step/game_input_ui ở US1+ (T032–T035, T057+).
 *
 * ===== Ring buffer thân sâu (data-model §2.1) =====
 * Đốt đầu ở body[head_idx]; đốt thứ k (0 = đầu) ở body[(head_idx + k) % WORM_CAP];
 * đuôi ở k = len-1. game_step (T032) đẩy đầu mới vào (head_idx - 1 + CAP) % CAP. */

static int cell_in_grid(int c, int r)
{
  return (c >= 0 && c < COLS && r >= 0 && r < ROWS);
}

static Dir dir_opposite(Dir d)
{
  switch (d) {
    case DIR_UP:   return DIR_DOWN;
    case DIR_DOWN: return DIR_UP;
    case DIR_LEFT: return DIR_RIGHT;
    default:       return DIR_LEFT;   /* DIR_RIGHT */
  }
}

/* Dựng lại occupied[][] = chướng ngại màn (T042) + thân sâu.
 * MODE_ENDLESS (T072): sân mở — KHÔNG nạp chướng ngại (research §18). */
static void grid_rebuild(GameState *gs)
{
  memset(gs->occupied, 0, sizeof gs->occupied);
  if (gs->play_mode != MODE_ENDLESS) {
    const Level *lv = level_get(gs->level_idx);
    if (lv != 0 && lv->obstacles != 0) {
      for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++)
          if (lv->obstacles[r][c]) gs->occupied[r][c] = 1u;
    }
  }
  for (uint16_t k = 0; k < gs->worm.len; k++) {
    uint16_t i = (uint16_t)((gs->worm.head_idx + k) % WORM_CAP);
    Cell s = gs->worm.body[i];
    gs->occupied[s.r][s.c] = 1u;
  }
}

/* Có lá khác đang đứng ở ô (c,r)? Bỏ qua slot `self` để không tự chặn lúc respawn. */
static int cell_has_leaf(const GameState *gs, int c, int r, const Leaf *self)
{
  const Leaf *L[4] = { &gs->leaf_normal, &gs->leaf_gold, &gs->leaf_poison, &gs->leaf_pu };
  for (int i = 0; i < 4; i++) {
    if (L[i] == self) continue;
    if (L[i]->type != LEAF_NONE && L[i]->pos.c == c && L[i]->pos.r == r) {
      return 1;
    }
  }
  return 0;
}

/* Sinh lá `type` ở một ô TRỐNG ngẫu nhiên (không thân/chướng ngại trong occupied,
 * không trùng lá khác). Chọn ô thứ k trong các ô trống qua rng → xác định theo seed.
 * Trả 1 nếu đặt được; 0 nếu sân đầy (→ thắng-màn xử ở US2/T040). (T033) */
static int spawn_leaf(GameState *gs, Leaf *leaf, LeafType type, PowerType pu)
{
  uint16_t free_n = 0u;
  for (int r = 0; r < ROWS; r++)
    for (int c = 0; c < COLS; c++)
      if (!gs->occupied[r][c] && !cell_has_leaf(gs, c, r, leaf)) free_n++;

  if (free_n == 0u) {
    leaf->type = LEAF_NONE;
    return 0;
  }
  uint32_t pick = rng_range(&gs->rng, free_n);
  uint16_t idx = 0u;
  for (int r = 0; r < ROWS; r++) {
    for (int c = 0; c < COLS; c++) {
      if (!gs->occupied[r][c] && !cell_has_leaf(gs, c, r, leaf)) {
        if (idx == pick) {
          leaf->pos.c  = (int8_t)c;
          leaf->pos.r  = (int8_t)r;
          leaf->type   = type;
          leaf->pu_type= pu;
          leaf->life_ms= -1;            /* lá thường: vô hạn (vàng/power-up đặt life ở US3) */
          return 1;
        }
        idx++;
      }
    }
  }
  return 0;                              /* không tới (free_n > 0 đảm bảo tìm thấy) */
}

/* Đặt sâu dài LEN_START ở giữa sân, nằm ngang, đầu hướng phải. */
static void worm_spawn_center(GameState *gs)
{
  Worm *w = &gs->worm;
  int cx = COLS / 2, cy = ROWS / 2;   /* (10, 6) */
  w->len = LEN_START;
  w->head_idx = 0u;
  w->dir = DIR_RIGHT;
  w->next_dir = DIR_RIGHT;
  w->grow_pending = 0u;
  for (uint16_t k = 0; k < LEN_START; k++) {
    w->body[k].c = (int8_t)(cx - (int)k);   /* đầu (10,6); thân (9,6), (8,6) */
    w->body[k].r = (int8_t)cy;
  }
}

/* Reset toàn phiên về đầu ván màn 0 (dùng chung cho game_init & game_start). */
static void reset_session(GameState *gs)
{
  gs->level_idx    = 0u;
  gs->score        = 0u;
  gs->leaves_eaten = 0u;
  /* T072: nhịp khởi đầu theo chế độ — ENDLESS bắt từ ENDLESS_STEP0 rồi giảm dần (research §18). */
  gs->step_ms      = (gs->play_mode == MODE_ENDLESS) ? ENDLESS_STEP0 : STEP_MS[0];

  gs->leaf_normal.type = LEAF_NONE;
  gs->leaf_gold.type   = LEAF_NONE;
  gs->leaf_poison.type = LEAF_NONE;
  gs->leaf_pu.type     = LEAF_NONE;
  for (int k = 0; k < PU_KINDS; k++) {
    gs->power[k] = 0;
  }

  worm_spawn_center(gs);
  grid_rebuild(gs);
}

void game_init(GameState *gs, uint32_t seed)
{
  memset(gs, 0, sizeof *gs);
  rng_seed(&gs->rng, seed);
  gs->play_mode = MODE_LEVEL;
  gs->menu_sel  = 0u;
  reset_session(gs);
  gs->mode = ST_MENU;          /* khởi tạo dừng ở MENU (FR-014) */
}

void game_start(GameState *gs)
{
  reset_session(gs);                                   /* nạp màn 0, reset sâu/score/lá */
  spawn_leaf(gs, &gs->leaf_normal, LEAF_NORMAL, PU_NONE); /* lá thường đầu màn (FR-004) */
  gs->mode = ST_PLAYING;
}

/* T044: lên màn kế — GIỮ score, reset sâu/lá/leaves_eaten, nạp step_ms & chướng ngại
 * màn mới, sinh lá, vào PLAYING. Gọi từ game_input_ui khi LEVEL_COMPLETE + IN_SELECT. */
static void advance_level(GameState *gs)
{
  gs->level_idx++;
  const Level *lv = level_get(gs->level_idx);
  gs->step_ms = (lv != 0) ? lv->step_ms : STEP_MS[LEVELS - 1];
  gs->leaves_eaten = 0u;
  gs->leaf_normal.type = LEAF_NONE;
  gs->leaf_gold.type   = LEAF_NONE;
  gs->leaf_poison.type = LEAF_NONE;
  gs->leaf_pu.type     = LEAF_NONE;
  worm_spawn_center(gs);
  grid_rebuild(gs);                                    /* nạp chướng ngại màn mới + thân */
  spawn_leaf(gs, &gs->leaf_normal, LEAF_NORMAL, PU_NONE);
  gs->mode = ST_PLAYING;
}

LeafType game_cell_content(const GameState *gs, Cell c)
{
  if (!cell_in_grid(c.c, c.r)) {
    return LEAF_NONE;
  }
  const Leaf *leaves[4] = {
    &gs->leaf_normal, &gs->leaf_gold, &gs->leaf_poison, &gs->leaf_pu
  };
  for (int i = 0; i < 4; i++) {
    if (leaves[i]->type != LEAF_NONE &&
        leaves[i]->pos.c == c.c && leaves[i]->pos.r == c.r) {
      return leaves[i]->type;
    }
  }
  return LEAF_NONE;            /* trống (hoặc thân/chướng ngại — render đọc worm/occupied) */
}

/* T052: tick hiệu dụng = step_ms cơ bản của màn × hệ số power-up tốc độ (clamp).
 * gs->step_ms GIỮ là nhịp cơ bản; hệ số áp ở đây để hết hiệu lực thì tự về nhịp gốc. */
uint16_t game_step_ms(const GameState *gs)
{
  float ms = (float)gs->step_ms;
  if (gs->power[PU_SPEED - 1] > 0) ms *= SPEED_FACTOR;   /* tăng tốc ×0.6 */
  if (gs->power[PU_SLOW  - 1] > 0) ms *= SLOW_FACTOR;    /* làm chậm ×1.7 */
  if (ms < (float)STEP_MS_MIN) ms = (float)STEP_MS_MIN;
  if (ms > (float)STEP_MS_MAX) ms = (float)STEP_MS_MAX;
  return (uint16_t)(ms + 0.5f);
}

/* T053: ô (c,r) có phải chướng ngại màn hiện tại? GHOST xuyên THÂN nhưng KHÔNG xuyên
 * chướng ngại (occupied gộp cả hai → cần phân biệt khi quyết va chạm). */
static int cell_is_obstacle(const GameState *gs, int c, int r)
{
  const Level *lv = level_get(gs->level_idx);
  return (lv != 0 && lv->obstacles != 0 && lv->obstacles[r][c]) ? 1 : 0;
}

/* T049: hạ đồng hồ 1 lá có hạn (vàng/power-up). Trả EV_LEAF_EXPIRED nếu vừa hết & biến mất. */
static GameEvents leaf_age(Leaf *lf, uint16_t dt_ms)
{
  if (lf->type != LEAF_NONE && lf->life_ms >= 0) {
    lf->life_ms -= (int32_t)dt_ms;
    if (lf->life_ms <= 0) {
      lf->type = LEAF_NONE;
      return EV_LEAF_EXPIRED;
    }
  }
  return 0u;
}

/* T053: đầu (k=0) có đang chồng lên một đốt thân khác? (cho grace GHOST lúc hết giờ.) */
static int head_overlaps_body(const GameState *gs)
{
  const Worm *w = &gs->worm;
  Cell h = w->body[w->head_idx];
  for (uint16_t k = 1; k < w->len; k++) {
    uint16_t i = (uint16_t)((w->head_idx + k) % WORM_CAP);
    if (w->body[i].c == h.c && w->body[i].r == h.r) {
      return 1;
    }
  }
  return 0;
}

/* T049/T050/T052: sau khi ăn lá thường, rút ngẫu nhiên lá đặc biệt/power-up (research §6).
 * Mở khoá dần (MODE_LEVEL): vàng mọi màn; độc từ level_idx>=1 (lv2); power-up từ level_idx>=2 (lv3).
 * MODE_ENDLESS (research §18): tất cả mở khoá NGAY từ đầu.
 * Chỉ sinh khi loại đó CHƯA có trên sân; mỗi loại ≤ 1. Mọi rút đi qua rng → xác định. */
static void roll_specials(GameState *gs)
{
  int endless = (gs->play_mode == MODE_ENDLESS);
  if (gs->leaf_gold.type == LEAF_NONE &&
      rng_range(&gs->rng, 100u) < GOLD_CHANCE_PCT) {
    if (spawn_leaf(gs, &gs->leaf_gold, LEAF_GOLD, PU_NONE)) {
      gs->leaf_gold.life_ms = GOLD_LIFE_MS;          /* lá vàng có hạn giờ */
    }
  }
  if ((endless || gs->level_idx >= 1u) && gs->leaf_poison.type == LEAF_NONE &&
      rng_range(&gs->rng, 100u) < POISON_CHANCE_PCT) {
    spawn_leaf(gs, &gs->leaf_poison, LEAF_POISON, PU_NONE);  /* life_ms=-1: tồn tại tới khi bị ăn */
  }
  if ((endless || gs->level_idx >= 2u) && gs->leaf_pu.type == LEAF_NONE &&
      rng_range(&gs->rng, 100u) < PU_CHANCE_PCT) {
    PowerType pt = (PowerType)(PU_SPEED + rng_range(&gs->rng, PU_KINDS));  /* đều trong 4 loại */
    if (spawn_leaf(gs, &gs->leaf_pu, LEAF_POWERUP, pt)) {
      gs->leaf_pu.life_ms = PU_LIFE_MS;              /* power-up trên sân có hạn giờ */
    }
  }
}

/* ===== game_step — luật core US1 (T032–T035) =====
 * Một bước logic ở ST_PLAYING: commit hướng (lọc 180° theo committed_dir), dời sâu,
 * ăn lá thường (mọc +1, +10đ, sinh lá mới), phát hiện va chạm (tường + thân-trừ-đuôi)
 * → ST_GAME_OVER. Lá vàng/độc/power-up & qua-màn ở US2/US3 (T040, T049–T054).
 *   dt_ms: bước thời gian hiệu dụng — dùng cho đồng hồ lá/power-up (US3); M3 chưa cần. */
GameEvents game_step(GameState *gs, InputEvent in, uint16_t dt_ms)
{
  if (gs->mode != ST_PLAYING) {
    return 0u;
  }
  /* T058: nút chính (IN_SELECT) khi đang chơi → PAUSED, KHÔNG dời sâu (FSM phân tách theo mode). */
  if (in.kind == IN_SELECT) {
    gs->mode = ST_PAUSED;
    return 0u;
  }
  Worm *w = &gs->worm;
  GameEvents ev = 0u;

  /* Power-up đang BẬT khi vào bước (quyết va biên/va thân của chính bước này). */
  int ghost_on = (gs->power[PU_GHOST - 1] > 0);
  int phase_on = (gs->power[PU_PHASE - 1] > 0);

  /* An toàn: nếu thiếu lá thường (vd sân từng đầy) thì thử sinh lại. */
  if (gs->leaf_normal.type == LEAF_NONE) {
    spawn_leaf(gs, &gs->leaf_normal, LEAF_NORMAL, PU_NONE);
  }

  /* T032: commit hướng — lọc 180° theo committed_dir (w->dir), KHÔNG theo next_dir
   * (chống bẫy "gạt 2 lần trong 1 tick" UP→LEFT→DOWN tự cắn). IN_NONE = đi thẳng (FR-020). */
  if (in.kind == IN_DIR) {
    if (in.dir != dir_opposite(w->dir)) {
      w->next_dir = in.dir;
    } else {
      ev |= EV_DIR_BLOCKED;                  /* chặn quay đầu 180° (FR-003) */
    }
  }
  w->dir = w->next_dir;

  /* Ô đầu kế theo hướng đã commit. */
  Cell nh = w->body[w->head_idx];
  switch (w->dir) {
    case DIR_RIGHT: nh.c++; break;
    case DIR_LEFT:  nh.c--; break;
    case DIR_UP:    nh.r--; break;
    case DIR_DOWN:  nh.r++; break;
  }

  /* T053: va biên → PHASE wrap sang cạnh đối diện; không PHASE → Game Over (T034). */
  if (!cell_in_grid(nh.c, nh.r)) {
    if (phase_on) {
      if (nh.c < 0)          nh.c = (int8_t)(COLS - 1);
      else if (nh.c >= COLS) nh.c = 0;
      if (nh.r < 0)          nh.r = (int8_t)(ROWS - 1);
      else if (nh.r >= ROWS) nh.r = 0;
    } else {
      gs->mode = ST_GAME_OVER;
      return ev | EV_GAME_OVER;
    }
  }

  /* T033/T049/T050/T052: ăn lá gì ở ô đầu mới? (lá KHÔNG nằm trong occupied → quyết riêng;
   * bất biến: tối đa 1 lá/ô.) Lá thường & vàng mọc +1; độc co; power-up giữ độ dài. */
  int ate_normal = (gs->leaf_normal.type == LEAF_NORMAL &&
                    gs->leaf_normal.pos.c == nh.c && gs->leaf_normal.pos.r == nh.r);
  int ate_gold   = (gs->leaf_gold.type == LEAF_GOLD &&
                    gs->leaf_gold.pos.c == nh.c && gs->leaf_gold.pos.r == nh.r);
  int ate_poison = (gs->leaf_poison.type == LEAF_POISON &&
                    gs->leaf_poison.pos.c == nh.c && gs->leaf_poison.pos.r == nh.r);
  int ate_pu     = (gs->leaf_pu.type == LEAF_POWERUP &&
                    gs->leaf_pu.pos.c == nh.c && gs->leaf_pu.pos.r == nh.r);
  int growing = (ate_normal || ate_gold);    /* mọc ⇒ đuôi KHÔNG nhả bước này */

  /* T034/T053: va thân/chướng ngại → Game Over, TRỪ ô đuôi sắp nhả (khi không mọc).
   * GHOST xuyên qua THÂN (không Game Over) nhưng KHÔNG xuyên chướng ngại.
   * occupied dựng từ thân hiện tại; đuôi nằm trong occupied nên cần ngoại lệ này. */
  if (gs->occupied[nh.r][nh.c]) {
    Cell tail = w->body[(w->head_idx + w->len - 1u) % WORM_CAP];
    int into_vacated_tail = (!growing && nh.c == tail.c && nh.r == tail.r);
    if (!into_vacated_tail) {
      if (cell_is_obstacle(gs, nh.c, nh.r) || !ghost_on) {
        gs->mode = ST_GAME_OVER;
        return ev | EV_GAME_OVER;
      }
      /* GHOST: đi xuyên qua thân (đầu sẽ chồng đốt thân — grace xử ở cuối bước). */
    }
  }

  /* T032: đẩy đầu mới vào ring buffer. Không mọc → len giữ nguyên (đuôi tự rớt). */
  uint16_t new_head = (uint16_t)((w->head_idx + WORM_CAP - 1u) % WORM_CAP);
  w->body[new_head] = nh;
  w->head_idx = new_head;
  if (growing && w->len < WORM_CAP) {
    w->len++;
  }

  /* T050: lá độc — co POISON_SHRINK đốt (sàn LEN_MIN) VÀ luôn phạt POISON_PENALTY điểm
   * (clamp ≥0) để phản hồi rõ ở HUD; KHÔNG gây Game Over. */
  if (ate_poison) {
    if (w->len > LEN_MIN) {
      uint16_t shrink = POISON_SHRINK;
      if ((uint16_t)(w->len - shrink) < LEN_MIN) shrink = (uint16_t)(w->len - LEN_MIN);
      w->len = (uint16_t)(w->len - shrink);
    }
    gs->score = (gs->score > (uint32_t)POISON_PENALTY) ? (gs->score - POISON_PENALTY) : 0u;
    gs->leaf_poison.type = LEAF_NONE;
    ev |= EV_ATE_POISON;
  }

  grid_rebuild(gs);                            /* sau khi chốt len (mọc/co) */
  ev |= EV_MOVED;

  /* T049: lá vàng — +50 điểm, đã mọc ở trên, biến mất. */
  if (ate_gold) {
    gs->score += SCORE_GOLD;
    gs->leaf_gold.type = LEAF_NONE;
    ev |= EV_ATE_GOLD;
  }

  /* T052: power-up — bật/refresh đồng hồ hiệu lực theo loại (stack độc lập). */
  if (ate_pu) {
    PowerType pt = gs->leaf_pu.pu_type;
    if (pt >= PU_SPEED && pt <= PU_PHASE) {
      gs->power[pt - 1] = PU_EFFECT_MS;
    }
    gs->leaf_pu.type = LEAF_NONE;
    ev |= EV_ATE_POWERUP;
  }

  /* T033/T044: hiệu lực ăn lá thường — điểm, đếm, sinh lá mới, rút lá đặc biệt, kiểm qua màn. */
  if (ate_normal) {
    gs->score += SCORE_LEAF;
    gs->leaves_eaten++;
    gs->leaf_normal.type = LEAF_NONE;
    int placed = spawn_leaf(gs, &gs->leaf_normal, LEAF_NORMAL, PU_NONE);
    ev |= EV_ATE_NORMAL;

    roll_specials(gs);                         /* T049/T050/T052: rút vàng/độc/power-up theo level */

    if (gs->play_mode == MODE_ENDLESS) {
      /* T072: Vô tận — KHÔNG có target/LEVEL_COMPLETE/WIN; nhịp giảm dần theo số lá ăn
       * (ENDLESS_STEP_DEC mỗi ENDLESS_RAMP_EVERY lá, clamp sàn STEP_MS_MIN) — research §18. */
      if (gs->leaves_eaten % ENDLESS_RAMP_EVERY == 0u && gs->step_ms > STEP_MS_MIN) {
        uint16_t s = (uint16_t)(gs->step_ms - ENDLESS_STEP_DEC);
        gs->step_ms = (s < STEP_MS_MIN) ? STEP_MS_MIN : s;
      }
      (void)placed;                            /* sân đầy ở Vô tận: hiếm; không kết thúc màn */
    } else {
      /* T044: MODE_LEVEL — qua màn khi đạt target HOẶC sân đầy (coi như thắng màn).
       * KHÔNG tự sang màn — chờ IN_SELECT ở game_input_ui (FR-021). */
      const Level *lv = level_get(gs->level_idx);
      uint16_t target = (lv != 0) ? lv->target_leaves : TARGET_LEAVES[gs->level_idx];
      if (gs->leaves_eaten >= target || placed == 0) {
        if (level_is_last(gs->level_idx)) {
          gs->mode = ST_WIN;
          ev |= EV_WIN;
        } else {
          gs->mode = ST_LEVEL_COMPLETE;
          ev |= EV_LEVEL_DONE;
        }
      }
    }
  }

  /* ===== Hạ đồng hồ ở CUỐI bước theo dt_ms thực (lá có hạn + power-up) ===== */
  ev |= leaf_age(&gs->leaf_gold, dt_ms);       /* lá vàng hết hạn → EV_LEAF_EXPIRED */
  ev |= leaf_age(&gs->leaf_pu,   dt_ms);       /* power-up trên sân hết hạn */

  for (int k = 0; k < PU_KINDS; k++) {
    if (gs->power[k] > 0) {
      gs->power[k] -= (int32_t)dt_ms;
      if (gs->power[k] < 0) gs->power[k] = 0;
    }
  }
  /* T053: GHOST hết giờ khi đầu CÒN chồng thân → gia hạn ngầm tới khi đầu rời khỏi (không chết oan). */
  if (gs->power[PU_GHOST - 1] == 0 && ghost_on && head_overlaps_body(gs)) {
    gs->power[PU_GHOST - 1] = 1;
  }

  return ev;
}

/* ===== game_input_ui — điều hướng ngoài ST_PLAYING (US4: T057–T059) =====
 *   MENU:           IN_DIR lên/xuống đổi menu_sel; IN_SELECT = Start (FR-015)
 *   PAUSED:         IN_SELECT = resume (FR-016; menu 3 mục đầy đủ ở US7)
 *   LEVEL_COMPLETE: IN_SELECT = lên màn kế (giữ score) (T044, FR-021)
 *   WIN/GAME_OVER:  IN_SELECT = chơi lại → về MENU rồi Start, điểm 0 (FR-017)
 * (Re-seed RNG tại sườn nhấn Start do lớp tasks lo — T061.) */
void game_input_ui(GameState *gs, InputEvent in)
{
  switch (gs->mode) {
    case ST_MENU:
      if (in.kind == IN_DIR) {
        if (in.dir == DIR_UP && gs->menu_sel > 0u) {
          gs->menu_sel--;
        } else if (in.dir == DIR_DOWN && (uint8_t)(gs->menu_sel + 1u) < MENU_ITEMS) {
          gs->menu_sel++;
        }
      } else if (in.kind == IN_SELECT) {
        if (gs->menu_sel == MENU_START) {          /* chơi Màn (campaign) */
          gs->play_mode = MODE_LEVEL;
          game_start(gs);
        } else if (gs->menu_sel == MENU_ENDLESS) { /* chơi Vô tận (US5) */
          gs->play_mode = MODE_ENDLESS;
          game_start(gs);
        } else if (gs->menu_sel == MENU_THEME) {   /* US6: cuộn theme (cosmetic), ở lại MENU */
          gs->theme_id = (uint8_t)((gs->theme_id + 1u) % THEME_COUNT);
        }
      }
      break;
    case ST_PAUSED:
      if (in.kind == IN_SELECT) {
        gs->mode = ST_PLAYING;               /* resume */
      }
      break;
    case ST_LEVEL_COMPLETE:
      if (in.kind == IN_SELECT) {
        advance_level(gs);                   /* sang màn kế, nhanh hơn, giữ điểm */
      }
      break;
    case ST_WIN:
    case ST_GAME_OVER:
      if (in.kind == IN_SELECT) {
        gs->mode = ST_MENU;                  /* chơi lại = về MENU (Start sẽ reset điểm 0) */
        gs->menu_sel = 0u;
      }
      break;
    default:
      break;
  }
}
