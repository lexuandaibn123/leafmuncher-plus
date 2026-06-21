#include "game.h"
#include "rng.h"
#include "levels.h"
#include <string.h>

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

/* Dựng lại occupied[][] = chướng ngại màn (T042) + thân sâu. */
static void grid_rebuild(GameState *gs)
{
  memset(gs->occupied, 0, sizeof gs->occupied);
  const Level *lv = level_get(gs->level_idx);
  if (lv != 0 && lv->obstacles != 0) {
    for (int r = 0; r < ROWS; r++)
      for (int c = 0; c < COLS; c++)
        if (lv->obstacles[r][c]) gs->occupied[r][c] = 1u;
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
  gs->step_ms      = STEP_MS[0];

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

uint16_t game_step_ms(const GameState *gs)
{
  return gs->step_ms;          /* tick hiệu dụng; hệ số power-up áp ở game_step (T034) */
}

/* ===== game_step — luật core US1 (T032–T035) =====
 * Một bước logic ở ST_PLAYING: commit hướng (lọc 180° theo committed_dir), dời sâu,
 * ăn lá thường (mọc +1, +10đ, sinh lá mới), phát hiện va chạm (tường + thân-trừ-đuôi)
 * → ST_GAME_OVER. Lá vàng/độc/power-up & qua-màn ở US2/US3 (T040, T049–T054).
 *   dt_ms: bước thời gian hiệu dụng — dùng cho đồng hồ lá/power-up (US3); M3 chưa cần. */
GameEvents game_step(GameState *gs, InputEvent in, uint16_t dt_ms)
{
  (void)dt_ms;
  if (gs->mode != ST_PLAYING) {
    return 0u;
  }
  Worm *w = &gs->worm;
  GameEvents ev = 0u;

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

  /* T034: va tường → Game Over (PHASE wrap biên ở US3). */
  if (!cell_in_grid(nh.c, nh.r)) {
    gs->mode = ST_GAME_OVER;
    return ev | EV_GAME_OVER;
  }

  /* T033: quyết định ăn lá thường TRƯỚC để biết bước này có mọc không. */
  int ate_normal = (gs->leaf_normal.type == LEAF_NORMAL &&
                    gs->leaf_normal.pos.c == nh.c &&
                    gs->leaf_normal.pos.r == nh.r);
  int growing = ate_normal;                  /* mọc ⇒ đuôi KHÔNG nhả bước này */

  /* T034: va thân/chướng ngại → Game Over, TRỪ ô đuôi sắp nhả (khi không mọc).
   * occupied dựng từ thân hiện tại; đuôi nằm trong occupied nên cần ngoại lệ này. */
  if (gs->occupied[nh.r][nh.c]) {
    Cell tail = w->body[(w->head_idx + w->len - 1u) % WORM_CAP];
    int into_vacated_tail = (!growing && nh.c == tail.c && nh.r == tail.r);
    if (!into_vacated_tail) {
      gs->mode = ST_GAME_OVER;
      return ev | EV_GAME_OVER;
    }
  }

  /* T032: đẩy đầu mới vào ring buffer. Không mọc → len giữ nguyên (đuôi tự rớt). */
  uint16_t new_head = (uint16_t)((w->head_idx + WORM_CAP - 1u) % WORM_CAP);
  w->body[new_head] = nh;
  w->head_idx = new_head;
  if (growing && w->len < WORM_CAP) {
    w->len++;
  }
  grid_rebuild(gs);
  ev |= EV_MOVED;

  /* T033: hiệu lực ăn lá thường — điểm, đếm, sinh lá mới ở ô trống. */
  if (ate_normal) {
    gs->score += SCORE_LEAF;
    gs->leaves_eaten++;
    gs->leaf_normal.type = LEAF_NONE;
    int placed = spawn_leaf(gs, &gs->leaf_normal, LEAF_NORMAL, PU_NONE);
    ev |= EV_ATE_NORMAL;

    /* T044: qua màn khi đạt target HOẶC sân đầy (không còn ô sinh lá → coi như thắng màn).
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
  return ev;
}

/* ===== game_input_ui — điều hướng ngoài ST_PLAYING =====
 * Nút chính (IN_SELECT) theo trạng thái:
 *   LEVEL_COMPLETE → lên màn kế (giữ score) (T044, FR-021)
 *   WIN / GAME_OVER → chơi lại từ màn 0
 * MENU/PAUSED đầy đủ ở US4 (T057). */
void game_input_ui(GameState *gs, InputEvent in)
{
  if (in.kind != IN_SELECT) {
    return;
  }
  switch (gs->mode) {
    case ST_LEVEL_COMPLETE:
      advance_level(gs);                     /* sang màn kế, nhanh hơn, giữ điểm */
      break;
    case ST_WIN:
    case ST_GAME_OVER:
      game_start(gs);                        /* RNG giữ state → ván mới khác biệt */
      break;
    default:
      break;
  }
}
