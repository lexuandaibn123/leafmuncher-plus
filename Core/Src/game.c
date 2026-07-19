#include "game.h"
#include "rng.h"
#include "levels.h"
#include <string.h>

#define PAUSE_ITEMS   3

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
    default:       return DIR_LEFT;
  }
}

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
          leaf->life_ms= -1;
          return 1;
        }
        idx++;
      }
    }
  }
  return 0;
}

static void worm_spawn_center(GameState *gs)
{
  Worm *w = &gs->worm;
  int cx = COLS / 2, cy = ROWS / 2;
  w->len = LEN_START;
  w->head_idx = 0u;
  w->dir = DIR_RIGHT;
  w->next_dir = DIR_RIGHT;
  w->grow_pending = 0u;
  for (uint16_t k = 0; k < LEN_START; k++) {
    w->body[k].c = (int8_t)(cx - (int)k);
    w->body[k].r = (int8_t)cy;
  }
}

static void reset_session(GameState *gs)
{
  gs->level_idx    = 0u;
  gs->score        = 0u;
  gs->leaves_eaten = 0u;
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
  gs->mode = ST_MENU;
}

void game_start(GameState *gs)
{
  reset_session(gs);
  spawn_leaf(gs, &gs->leaf_normal, LEAF_NORMAL, PU_NONE);
  gs->mode = ST_PLAYING;
}

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
  grid_rebuild(gs);
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
  return LEAF_NONE;
}

uint16_t game_step_ms(const GameState *gs)
{
  float ms = (float)gs->step_ms;
  if (gs->power[PU_SPEED - 1] > 0) ms *= SPEED_FACTOR;
  if (gs->power[PU_SLOW  - 1] > 0) ms *= SLOW_FACTOR;
  if (ms < (float)STEP_MS_MIN) ms = (float)STEP_MS_MIN;
  if (ms > (float)STEP_MS_MAX) ms = (float)STEP_MS_MAX;
  return (uint16_t)(ms + 0.5f);
}

static int cell_is_obstacle(const GameState *gs, int c, int r)
{
  const Level *lv = level_get(gs->level_idx);
  return (lv != 0 && lv->obstacles != 0 && lv->obstacles[r][c]) ? 1 : 0;
}

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

static void roll_specials(GameState *gs)
{
  int endless = (gs->play_mode == MODE_ENDLESS);
  
  if (gs->leaf_gold.type == LEAF_NONE && rng_range(&gs->rng, 100u) < GOLD_CHANCE_PCT) {
    if (spawn_leaf(gs, &gs->leaf_gold, LEAF_GOLD, PU_NONE)) {
      gs->leaf_gold.life_ms = GOLD_LIFE_MS;
    }
  }
  
  if ((endless || gs->level_idx >= 1u) && gs->leaf_poison.type == LEAF_NONE && rng_range(&gs->rng, 100u) < POISON_CHANCE_PCT) {
    spawn_leaf(gs, &gs->leaf_poison, LEAF_POISON, PU_NONE);
  }
  
  if ((endless || gs->level_idx >= 2u) && gs->leaf_pu.type == LEAF_NONE && rng_range(&gs->rng, 100u) < PU_CHANCE_PCT) {
    PowerType pt = (PowerType)(PU_SPEED + rng_range(&gs->rng, PU_KINDS));
    if (spawn_leaf(gs, &gs->leaf_pu, LEAF_POWERUP, pt)) {
      gs->leaf_pu.life_ms = PU_LIFE_MS;
    }
  }
}

/* Xử lý logic từng bước đi (di chuyển, ăn lá, kiểm tra va chạm) */
GameEvents game_step(GameState *gs, InputEvent in, uint16_t dt_ms)
{
  if (gs->mode != ST_PLAYING) {
    return 0u;
  }
  
  if (in.kind == IN_SELECT) {
    gs->mode = ST_PAUSED;
    gs->menu_sel = 0u;
    return 0u;
  }
  
  Worm *w = &gs->worm;
  GameEvents ev = 0u;

  int ghost_on = (gs->power[PU_GHOST - 1] > 0);
  int phase_on = (gs->power[PU_PHASE - 1] > 0);

  if (gs->leaf_normal.type == LEAF_NONE) {
    spawn_leaf(gs, &gs->leaf_normal, LEAF_NORMAL, PU_NONE);
  }

  if (in.kind == IN_DIR) {
    if (in.dir != dir_opposite(w->dir)) {
      w->next_dir = in.dir;
    } else {
      ev |= EV_DIR_BLOCKED;
    }
  }
  w->dir = w->next_dir;

  Cell nh = w->body[w->head_idx];
  switch (w->dir) {
    case DIR_RIGHT: nh.c++; break;
    case DIR_LEFT:  nh.c--; break;
    case DIR_UP:    nh.r--; break;
    case DIR_DOWN:  nh.r++; break;
  }

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

  int ate_normal = (gs->leaf_normal.type == LEAF_NORMAL &&
                    gs->leaf_normal.pos.c == nh.c && gs->leaf_normal.pos.r == nh.r);
  int ate_gold   = (gs->leaf_gold.type == LEAF_GOLD &&
                    gs->leaf_gold.pos.c == nh.c && gs->leaf_gold.pos.r == nh.r);
  int ate_poison = (gs->leaf_poison.type == LEAF_POISON &&
                    gs->leaf_poison.pos.c == nh.c && gs->leaf_poison.pos.r == nh.r);
  int ate_pu     = (gs->leaf_pu.type == LEAF_POWERUP &&
                    gs->leaf_pu.pos.c == nh.c && gs->leaf_pu.pos.r == nh.r);
  int growing = (ate_normal || ate_gold);

  if (gs->occupied[nh.r][nh.c]) {
    Cell tail = w->body[(w->head_idx + w->len - 1u) % WORM_CAP];
    int into_vacated_tail = (!growing && nh.c == tail.c && nh.r == tail.r);
    
    if (!into_vacated_tail) {
      if (cell_is_obstacle(gs, nh.c, nh.r) || !ghost_on) {
        gs->mode = ST_GAME_OVER;
        return ev | EV_GAME_OVER;
      }
    }
  }

  uint16_t new_head = (uint16_t)((w->head_idx + WORM_CAP - 1u) % WORM_CAP);
  w->body[new_head] = nh;
  w->head_idx = new_head;
  if (growing && w->len < WORM_CAP) {
    w->len++;
  }

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

  grid_rebuild(gs);
  ev |= EV_MOVED;

  if (ate_gold) {
    gs->score += SCORE_GOLD;
    gs->leaf_gold.type = LEAF_NONE;
    ev |= EV_ATE_GOLD;
  }

  if (ate_pu) {
    PowerType pt = gs->leaf_pu.pu_type;
    if (pt >= PU_SPEED && pt <= PU_PHASE) {
      gs->power[pt - 1] = PU_EFFECT_MS;
    }
    gs->leaf_pu.type = LEAF_NONE;
    ev |= EV_ATE_POWERUP;
  }

  if (ate_normal) {
    gs->score += SCORE_LEAF;
    gs->leaves_eaten++;
    gs->leaf_normal.type = LEAF_NONE;
    
    int placed = spawn_leaf(gs, &gs->leaf_normal, LEAF_NORMAL, PU_NONE);
    ev |= EV_ATE_NORMAL;

    roll_specials(gs);

    if (gs->play_mode == MODE_ENDLESS) {
      if (gs->leaves_eaten % ENDLESS_RAMP_EVERY == 0u && gs->step_ms > STEP_MS_MIN) {
        uint16_t s = (uint16_t)(gs->step_ms - ENDLESS_STEP_DEC);
        gs->step_ms = (s < STEP_MS_MIN) ? STEP_MS_MIN : s;
      }
      (void)placed;
    } else {
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

  ev |= leaf_age(&gs->leaf_gold, dt_ms);
  ev |= leaf_age(&gs->leaf_pu,   dt_ms);

  for (int k = 0; k < PU_KINDS; k++) {
    if (gs->power[k] > 0) {
      gs->power[k] -= (int32_t)dt_ms;
      if (gs->power[k] < 0) gs->power[k] = 0;
    }
  }
  
  if (gs->power[PU_GHOST - 1] == 0 && ghost_on && head_overlaps_body(gs)) {
    gs->power[PU_GHOST - 1] = 1;
  }

  return ev;
}

int game_menu_items(const GameState *gs, MenuItemId out[MENU_MAX_ITEMS])
{
  int n = 0;
  if (gs->has_save[MODE_LEVEL])   out[n++] = MI_CONTINUE_LEVEL;
  if (gs->has_save[MODE_ENDLESS]) out[n++] = MI_CONTINUE_ENDLESS;
  out[n++] = MI_START;
  out[n++] = MI_ENDLESS;
  return n;
}

/* Xử lý phím bấm điều hướng UI */
void game_input_ui(GameState *gs, InputEvent in)
{
  switch (gs->mode) {
    case ST_MENU: {
      MenuItemId items[MENU_MAX_ITEMS];
      int n = game_menu_items(gs, items);
      
      if (in.kind == IN_DIR) {
        if (in.dir == DIR_UP && gs->menu_sel > 0u) {
          gs->menu_sel--;
        } else if (in.dir == DIR_DOWN && (int)(gs->menu_sel + 1u) < n) {
          gs->menu_sel++;
        }
      } else if (in.kind == IN_SELECT) {
        if ((int)gs->menu_sel >= n) gs->menu_sel = (uint8_t)(n - 1);
        
        switch (items[gs->menu_sel]) {
          case MI_CONTINUE_LEVEL:
            gs->play_mode = MODE_LEVEL;
            gs->load_request = 1u;
            break;
          case MI_CONTINUE_ENDLESS:
            gs->play_mode = MODE_ENDLESS;
            gs->load_request = 1u;
            break;
          case MI_START:
            gs->play_mode = MODE_LEVEL;
            game_start(gs);
            break;
          case MI_ENDLESS:
            gs->play_mode = MODE_ENDLESS;
            game_start(gs);
            break;
        }
      }
      break;
    }
    case ST_PAUSED:
      if (in.kind == IN_DIR) {
        if (in.dir == DIR_UP && gs->menu_sel > 0u) {
          gs->menu_sel--;
        } else if (in.dir == DIR_DOWN && (uint8_t)(gs->menu_sel + 1u) < PAUSE_ITEMS) {
          gs->menu_sel++;
        }
      } else if (in.kind == IN_SELECT) {
        if (gs->menu_sel == 0u) {
          gs->mode = ST_PLAYING;
        } else if (gs->menu_sel == 1u) {
          gs->mode = ST_PLAYING;
          gs->save_request = 1u;
        } else {
          gs->mode = ST_MENU;
          gs->menu_sel = 0u;
        }
      }
      break;
    case ST_LEVEL_COMPLETE:
      if (in.kind == IN_SELECT) {
        advance_level(gs);
      }
      break;
    case ST_WIN:
    case ST_GAME_OVER:
      if (in.kind == IN_SELECT) {
        gs->mode = ST_MENU;
        gs->menu_sel = 0u;
      }
      break;
    default:
      break;
  }
}
