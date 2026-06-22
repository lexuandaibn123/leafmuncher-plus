/* test_game — unit-test logic thuần trên host (gcc). Nguyên tắc II / SC-006.
 * Phase 1: hằng số/kiểu chia sẻ. T019: game_init/game_start + truy vấn đọc-chỉ.
 * Luật core (ăn/va chạm/power-up) thêm ở US1+ (T028…). */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "game.h"

static int occupied_count(const GameState *gs) {
  int n = 0;
  for (int r = 0; r < ROWS; r++)
    for (int c = 0; c < COLS; c++)
      n += gs->occupied[r][c] ? 1 : 0;
  return n;
}

/* Đốt thứ k (0 = đầu) trong ring buffer. */
static Cell worm_seg(const GameState *gs, int k) {
  int i = (gs->worm.head_idx + k) % WORM_CAP;
  return gs->worm.body[i];
}

/* Dựng lại occupied khớp thân (dùng sau khi đặt thân thủ công cho test va chạm). */
static void rebuild_occupied(GameState *gs) {
  memset(gs->occupied, 0, sizeof gs->occupied);
  for (uint16_t k = 0; k < gs->worm.len; k++) {
    Cell s = worm_seg(gs, (int)k);
    gs->occupied[s.r][s.c] = 1;
  }
}

int main(void) {
  /* ---- Phase 1: hình học lưới khớp data-model/research §1 ---- */
  assert(COLS == 20 && ROWS == 13 && CELL == 16 && HUD_H == 32);
  assert(GRID_CELLS == 260);
  assert(COLS * CELL == SCREEN_W);
  assert(ROWS * CELL + HUD_H == SCREEN_H);
  assert(LEN_MIN <= LEN_START);
  assert(WORM_CAP == GRID_CELLS);
  assert(sizeof(GameState) < 2048);

  /* ---- Bảng màn (levels.c) khớp research §3 ---- */
  assert(STEP_MS[0] == 180 && STEP_MS[LEVELS - 1] == 95);
  assert(TARGET_LEAVES[0] == 6 && TARGET_LEAVES[LEVELS - 1] == 14);

  /* ---- T019: game_init ---- */
  GameState gs;
  game_init(&gs, 12345u);
  assert(gs.mode == ST_MENU);
  assert(gs.play_mode == MODE_LEVEL);
  assert(gs.level_idx == 0 && gs.score == 0 && gs.leaves_eaten == 0);
  assert(gs.step_ms == STEP_MS[0]);
  assert(gs.rng == 12345u);                 /* seed != 0 giữ nguyên */
  assert(gs.menu_sel == 0);

  /* Sâu LEN_START ở giữa, nằm ngang, đầu hướng phải. */
  assert(gs.worm.len == LEN_START);
  assert(gs.worm.dir == DIR_RIGHT && gs.worm.next_dir == DIR_RIGHT);
  assert(gs.worm.grow_pending == 0);
  Cell head = worm_seg(&gs, 0);
  Cell neck = worm_seg(&gs, 1);
  Cell tail = worm_seg(&gs, LEN_START - 1);
  assert(head.c == COLS / 2 && head.r == ROWS / 2);     /* (10,6) */
  assert(neck.c == COLS / 2 - 1 && neck.r == ROWS / 2); /* (9,6)  */
  assert(tail.c == COLS / 2 - 2 && tail.r == ROWS / 2); /* (8,6)  */

  /* occupied chỉ gồm 3 đốt thân; mỗi đốt khớp occupied. */
  assert(occupied_count(&gs) == LEN_START);
  for (int k = 0; k < LEN_START; k++) {
    Cell s = worm_seg(&gs, k);
    assert(gs.occupied[s.r][s.c] == 1);
  }

  /* Chưa có lá nào. */
  assert(gs.leaf_normal.type == LEAF_NONE && gs.leaf_gold.type == LEAF_NONE);
  assert(gs.leaf_poison.type == LEAF_NONE && gs.leaf_pu.type == LEAF_NONE);
  for (int k = 0; k < PU_KINDS; k++) assert(gs.power[k] == 0);

  /* ---- T019: game_cell_content + game_step_ms ---- */
  Cell c_empty = { 0, 0 };
  assert(game_cell_content(&gs, c_empty) == LEAF_NONE);
  Cell c_oob = { -1, 5 };                                /* ngoài lưới → NONE, không tràn */
  assert(game_cell_content(&gs, c_oob) == LEAF_NONE);
  Cell c_oob2 = { COLS, ROWS };
  assert(game_cell_content(&gs, c_oob2) == LEAF_NONE);
  /* Đặt thử 1 lá vàng rồi truy vấn (kiểm ánh xạ ô→loại lá). */
  gs.leaf_gold.type = LEAF_GOLD; gs.leaf_gold.pos.c = 3; gs.leaf_gold.pos.r = 4;
  Cell c_gold = { 3, 4 };
  assert(game_cell_content(&gs, c_gold) == LEAF_GOLD);
  assert(game_step_ms(&gs) == STEP_MS[0]);

  /* ---- T019: game_start ---- */
  game_start(&gs);
  assert(gs.mode == ST_PLAYING);
  assert(gs.score == 0 && gs.level_idx == 0 && gs.leaves_eaten == 0);
  assert(gs.worm.len == LEN_START);
  assert(gs.leaf_gold.type == LEAF_NONE);   /* reset xoá lá đã đặt ở trên */
  assert(occupied_count(&gs) == LEN_START);

  /* ---- Tính xác định: cùng seed → cùng state byte-for-byte ---- */
  GameState a, b;
  game_init(&a, 0xC0FFEEu);
  game_init(&b, 0xC0FFEEu);
  assert(memcmp(&a, &b, sizeof a) == 0);
  /* Seed 0 được "vá" về khác 0 (xorshift không kẹt). */
  GameState z; game_init(&z, 0u);
  assert(z.rng != 0u);

  /* ================= US1 / M3 — luật core (T028–T031) ================= */

  /* ---- T028: di chuyển + deadzone đi thẳng + ăn lá (grow + score) ---- */
  {
    GameState g;
    game_init(&g, 0xABCDEFu);
    game_start(&g);                 /* PLAYING; có 1 lá thường đầu màn */
    assert(g.mode == ST_PLAYING);
    assert(g.leaf_normal.type == LEAF_NORMAL);   /* game_start sinh lá đầu */

    /* Deadzone (IN_NONE) → đi thẳng theo dir hiện tại (RIGHT). */
    Cell h0 = worm_seg(&g, 0);
    InputEvent none = { IN_NONE, DIR_RIGHT };
    /* Đặt lá ngay trước đầu để chắc chắn ăn ở bước này. */
    g.leaf_normal.type = LEAF_NORMAL;
    g.leaf_normal.pos.c = (int8_t)(h0.c + 1); g.leaf_normal.pos.r = h0.r;
    g.leaf_normal.life_ms = -1;
    uint16_t len0 = g.worm.len;
    uint32_t sc0  = g.score;
    GameEvents e = game_step(&g, none, g.step_ms);

    assert(e & EV_MOVED);
    assert(e & EV_ATE_NORMAL);
    Cell h1 = worm_seg(&g, 0);
    assert(h1.c == h0.c + 1 && h1.r == h0.r);     /* đi thẳng phải */
    assert(g.worm.len == len0 + 1);               /* dài ra 1 đốt */
    assert(g.score == sc0 + SCORE_LEAF);          /* +10 điểm */
    assert(g.leaves_eaten == 1);
    assert(g.mode == ST_PLAYING);
    /* Sinh lá mới ở ô trống (không trùng ô đầu vừa ăn). */
    assert(g.leaf_normal.type == LEAF_NORMAL);
    assert(!(g.leaf_normal.pos.c == h1.c && g.leaf_normal.pos.r == h1.r));
    /* occupied khớp số đốt thân (lá KHÔNG nằm trong occupied). */
    assert(occupied_count(&g) == g.worm.len);
  }

  /* ---- T029: va tường → GAME_OVER ---- */
  {
    GameState g;
    game_init(&g, 7u);
    game_start(&g);
    /* Đẩy đầu sát biên phải rồi đâm tường. Dời thủ công đầu tới (COLS-1, r). */
    Worm *w = &g.worm;
    int r = ROWS / 2;
    /* Dựng sâu nằm ngang đầu hướng phải tại sát biên: head (COLS-1,r). */
    w->head_idx = 0u; w->len = LEN_START; w->dir = DIR_RIGHT; w->next_dir = DIR_RIGHT;
    for (int k = 0; k < LEN_START; k++) { w->body[k].c = (int8_t)(COLS - 1 - k); w->body[k].r = (int8_t)r; }
    g.leaf_normal.type = LEAF_NONE;   /* khỏi vướng lá ở biên */
    /* rebuild occupied bằng 1 bước trung lập? gọi game_step để nó tự rebuild;
     * nhưng head ở (COLS-1) đi RIGHT là ra ngoài ngay → GAME_OVER. */
    GameEvents e = game_step(&g, (InputEvent){ IN_NONE, DIR_RIGHT }, g.step_ms);
    assert(e & EV_GAME_OVER);
    assert(g.mode == ST_GAME_OVER);
  }

  /* ---- T029b: va thân (giữa) → chết; nhưng đi vào ô ĐUÔI vừa nhả → KHÔNG chết ---- */
  {
    /* Sâu vuông 2x2: head→tail = (5,5),(6,5),(6,6),(5,6). dir=LEFT (đến (5,5) từ phải). */
    GameState g;
    game_init(&g, 99u);
    game_start(&g);
    Worm *w = &g.worm;
    w->head_idx = 0u; w->len = 4u; w->dir = DIR_LEFT; w->next_dir = DIR_LEFT;
    w->body[0] = (Cell){ 5, 5 };   /* head */
    w->body[1] = (Cell){ 6, 5 };
    w->body[2] = (Cell){ 6, 6 };
    w->body[3] = (Cell){ 5, 6 };   /* tail */
    g.leaf_normal.type = LEAF_NONE;
    rebuild_occupied(&g);

    /* Đi XUỐNG vào (5,6) = ô đuôi sắp nhả → hợp lệ, KHÔNG chết. */
    GameEvents e = game_step(&g, (InputEvent){ IN_DIR, DIR_DOWN }, g.step_ms);
    assert(!(e & EV_GAME_OVER));
    assert(g.mode == ST_PLAYING);
    Cell nh = worm_seg(&g, 0);
    assert(nh.c == 5 && nh.r == 6);
    assert(g.worm.len == 4u);                 /* không mọc → giữ độ dài */
    assert(occupied_count(&g) == 4);          /* không có đốt trùng */
  }

  {
    /* Đâm thân giữa → chết. head→tail = (5,6),(5,5),(6,5),(6,6),(6,7). dir=DOWN. */
    GameState g;
    game_init(&g, 1234u);
    game_start(&g);
    Worm *w = &g.worm;
    w->head_idx = 0u; w->len = 5u; w->dir = DIR_DOWN; w->next_dir = DIR_DOWN;
    w->body[0] = (Cell){ 5, 6 };   /* head */
    w->body[1] = (Cell){ 5, 5 };
    w->body[2] = (Cell){ 6, 5 };
    w->body[3] = (Cell){ 6, 6 };   /* giữa thân */
    w->body[4] = (Cell){ 6, 7 };   /* tail */
    g.leaf_normal.type = LEAF_NONE;
    rebuild_occupied(&g);
    /* Đi PHẢI vào (6,6) = thân giữa (KHÁC ô đuôi (6,7)) → chết. */
    GameEvents e = game_step(&g, (InputEvent){ IN_DIR, DIR_RIGHT }, g.step_ms);
    assert(e & EV_GAME_OVER);
    assert(g.mode == ST_GAME_OVER);
  }

  /* ---- T030: chặn 180° theo committed_dir (gồm chuỗi gạt trong các tick liên tiếp) ---- */
  {
    GameState g;
    game_init(&g, 55u);
    game_start(&g);
    g.leaf_normal.type = LEAF_NONE;
    assert(g.worm.dir == DIR_RIGHT);
    /* Gạt LEFT (đối hướng RIGHT) → bị chặn, giữ hướng, vẫn đi phải. */
    Cell h0 = worm_seg(&g, 0);
    GameEvents e = game_step(&g, (InputEvent){ IN_DIR, DIR_LEFT }, g.step_ms);
    assert(e & EV_DIR_BLOCKED);
    assert(g.worm.dir == DIR_RIGHT);
    Cell h1 = worm_seg(&g, 0);
    assert(h1.c == h0.c + 1 && h1.r == h0.r);

    /* Rẽ UP hợp lệ → committed_dir = UP. */
    e = game_step(&g, (InputEvent){ IN_DIR, DIR_UP }, g.step_ms);
    assert(!(e & EV_DIR_BLOCKED));
    assert(g.worm.dir == DIR_UP);
    Cell h2 = worm_seg(&g, 0);
    assert(h2.c == h1.c && h2.r == h1.r - 1);

    /* Ngay tick kế gạt DOWN (đối hướng committed UP) → chặn (đây là bẫy double-flick:
     * UP rồi DOWN — nếu lọc theo next_dir thay vì committed_dir sẽ tự cắn). */
    e = game_step(&g, (InputEvent){ IN_DIR, DIR_DOWN }, g.step_ms);
    assert(e & EV_DIR_BLOCKED);
    assert(g.worm.dir == DIR_UP);
    Cell h3 = worm_seg(&g, 0);
    assert(h3.c == h2.c && h3.r == h2.r - 1);     /* vẫn đi lên */
  }

  /* ---- T031: lá sinh ra không bao giờ đè thân (qua nhiều lần ăn) ---- */
  {
    GameState g;
    game_init(&g, 0x5EED01u);
    game_start(&g);
    /* Ăn dọc hàng giữa từ c=11..15 (5 lá < target 6 → vẫn PLAYING): mỗi lần đặt lá
     * ngay trước đầu rồi bước, kiểm lá mới sinh không đè thân. */
    for (int c = 11; c <= 15; c++) {
      Cell h = worm_seg(&g, 0);
      /* Xoá mọi lá đặc biệt roll_specials có thể đã sinh để chỉ kiểm lá thường. */
      g.leaf_gold.type = LEAF_NONE;
      g.leaf_poison.type = LEAF_NONE;
      g.leaf_pu.type = LEAF_NONE;
      g.leaf_normal.type = LEAF_NORMAL;
      g.leaf_normal.pos.c = (int8_t)c; g.leaf_normal.pos.r = h.r;
      g.leaf_normal.life_ms = -1;
      GameEvents e = game_step(&g, (InputEvent){ IN_NONE, DIR_RIGHT }, g.step_ms);
      assert(e & EV_ATE_NORMAL);
      /* Lá mới sinh: trong lưới, không trùng đốt thân nào, ô không bị occupied. */
      assert(g.leaf_normal.type == LEAF_NORMAL);
      int lc = g.leaf_normal.pos.c, lr = g.leaf_normal.pos.r;
      assert(lc >= 0 && lc < COLS && lr >= 0 && lr < ROWS);
      assert(g.occupied[lr][lc] == 0);
      for (uint16_t k = 0; k < g.worm.len; k++) {
        Cell s = worm_seg(&g, (int)k);
        assert(!(s.c == lc && s.r == lr));
      }
    }
    assert(g.worm.len == (uint16_t)(LEN_START + 5));
    assert(g.score == (uint32_t)(5 * SCORE_LEAF));
    assert(g.mode == ST_PLAYING);
  }

  /* ---- US4: GAME_OVER + nút → MENU (chơi lại = về MENU rồi Start, FR-017) ---- */
  {
    GameState g;
    game_init(&g, 314u);
    game_start(&g);
    g.mode = ST_GAME_OVER; g.score = 123u;
    game_input_ui(&g, (InputEvent){ IN_SELECT, DIR_UP });
    assert(g.mode == ST_MENU);                          /* về MENU trước */
    game_input_ui(&g, (InputEvent){ IN_SELECT, DIR_UP });
    assert(g.mode == ST_PLAYING);                       /* Start từ MENU */
    assert(g.score == 0u && g.worm.len == LEN_START);   /* reset ván mới */
  }

  /* ================= US2 / M4 — chướng ngại & nhiều màn (T040) ================= */

  /* ---- T040a: va chướng ngại → GAME_OVER (màn 1 có thanh ngang hàng 3) ---- */
  {
    GameState g;
    game_init(&g, 11u);
    game_start(&g);
    g.level_idx = 1;                 /* màn 1: chướng ngại hàng 3 & 9, cột 5..14 */
    g.leaf_normal.type = LEAF_NORMAL;
    g.leaf_normal.pos.c = 19; g.leaf_normal.pos.r = 12;   /* lá xa, ngoài lối đi */
    /* Sâu thẳng đứng dưới thanh: head (7,5) hướng UP, thân (7,6),(7,7). */
    Worm *w = &g.worm;
    w->head_idx = 0u; w->len = LEN_START; w->dir = DIR_UP; w->next_dir = DIR_UP;
    w->body[0] = (Cell){ 7, 5 };
    w->body[1] = (Cell){ 7, 6 };
    w->body[2] = (Cell){ 7, 7 };
    rebuild_occupied(&g);            /* occupied tạm (chưa chướng ngại) cho bước 1 */

    /* Bước 1: UP (7,5)→(7,4) ô trống → di chuyển; grid_rebuild nạp chướng ngại màn 1. */
    GameEvents e1 = game_step(&g, (InputEvent){ IN_NONE, DIR_UP }, g.step_ms);
    assert(e1 & EV_MOVED);
    assert(!(e1 & EV_GAME_OVER));
    assert(g.occupied[3][7] == 1);   /* chướng ngại (7,3) đã nạp vào occupied */

    /* Bước 2: UP (7,4)→(7,3) = chướng ngại → GAME_OVER. */
    GameEvents e2 = game_step(&g, (InputEvent){ IN_NONE, DIR_UP }, g.step_ms);
    assert(e2 & EV_GAME_OVER);
    assert(g.mode == ST_GAME_OVER);
  }

  /* ---- T040b: đạt target → LEVEL_COMPLETE; IN_SELECT → lên màn kế (tăng tốc, giữ điểm) ---- */
  {
    GameState g;
    game_init(&g, 22u);
    game_start(&g);                  /* màn 0, target 6 */
    /* Ăn 6 lá dọc hàng giữa (c=11..16) → đạt target. */
    for (int c = 11; c <= 16; c++) {
      Cell h = worm_seg(&g, 0);
      g.leaf_gold.type = LEAF_NONE;          /* loại nhiễu lá đặc biệt (roll_specials) */
      g.leaf_poison.type = LEAF_NONE;
      g.leaf_pu.type = LEAF_NONE;
      g.leaf_normal.type = LEAF_NORMAL;
      g.leaf_normal.pos.c = (int8_t)c; g.leaf_normal.pos.r = h.r;
      g.leaf_normal.life_ms = -1;
      GameEvents e = game_step(&g, (InputEvent){ IN_NONE, DIR_RIGHT }, g.step_ms);
      assert(e & EV_ATE_NORMAL);
      if (c < 16) assert(g.mode == ST_PLAYING);
    }
    assert(g.leaves_eaten == 6);
    assert(g.mode == ST_LEVEL_COMPLETE);
    assert(g.score == 6 * SCORE_LEAF);

    uint32_t kept = g.score;
    game_input_ui(&g, (InputEvent){ IN_SELECT, DIR_UP });
    assert(g.mode == ST_PLAYING);
    assert(g.level_idx == 1);
    assert(g.step_ms == STEP_MS[1]);          /* nhanh hơn màn 0 */
    assert(g.step_ms < STEP_MS[0]);
    assert(g.score == kept);                  /* giữ điểm */
    assert(g.leaves_eaten == 0);              /* đếm lại từ đầu màn */
    assert(g.worm.len == LEN_START);          /* sâu reset */
    assert(g.leaf_normal.type == LEAF_NORMAL);/* lá mới đã sinh */
  }

  /* ---- T040c: màn cuối đạt target → WIN; IN_SELECT → chơi lại từ màn 0 ---- */
  {
    GameState g;
    game_init(&g, 33u);
    game_start(&g);
    g.level_idx = LEVELS - 1;        /* màn cuối, target 14 */
    g.leaves_eaten = 13;             /* coi như đã ăn 13 lá trước đó */
    Cell h = worm_seg(&g, 0);
    g.leaf_normal.type = LEAF_NORMAL;
    g.leaf_normal.pos.c = (int8_t)(h.c + 1); g.leaf_normal.pos.r = h.r;
    g.leaf_normal.life_ms = -1;
    GameEvents e = game_step(&g, (InputEvent){ IN_NONE, DIR_RIGHT }, g.step_ms);
    assert(e & EV_ATE_NORMAL);
    assert(e & EV_WIN);
    assert(g.mode == ST_WIN);
    assert(g.leaves_eaten == 14);

    game_input_ui(&g, (InputEvent){ IN_SELECT, DIR_UP });
    assert(g.mode == ST_MENU);                /* WIN → MENU (chơi lại) */
    game_input_ui(&g, (InputEvent){ IN_SELECT, DIR_UP });
    assert(g.mode == ST_PLAYING);
    assert(g.level_idx == 0 && g.score == 0);
  }

  /* ---- T040d: sân đầy (không còn ô sinh lá) → thắng màn (LEVEL_COMPLETE ở màn 0) ---- */
  {
    GameState g;
    game_init(&g, 44u);
    game_start(&g);                  /* màn 0 (không chướng ngại) */
    Worm *w = &g.worm;
    /* Phủ kín lưới TRỪ ô (0,0) (để lá). head = ô đầu tiên thêm = (1,0), hướng LEFT. */
    g.leaf_normal.type = LEAF_NORMAL;
    g.leaf_normal.pos.c = 0; g.leaf_normal.pos.r = 0;
    g.leaf_normal.life_ms = -1;
    uint16_t n = 0;
    for (int r = 0; r < ROWS; r++) {
      for (int c = 0; c < COLS; c++) {
        if (c == 0 && r == 0) continue;          /* chừa ô lá */
        w->body[n].c = (int8_t)c; w->body[n].r = (int8_t)r;
        n++;
      }
    }
    w->head_idx = 0u; w->len = n; w->dir = DIR_LEFT; w->next_dir = DIR_LEFT;
    assert(n == GRID_CELLS - 1);                 /* 259 đốt */
    rebuild_occupied(&g);

    /* head (1,0) đi LEFT vào (0,0) = lá cuối → ăn, sân đầy → không còn ô sinh lá. */
    GameEvents e = game_step(&g, (InputEvent){ IN_NONE, DIR_LEFT }, g.step_ms);
    assert(e & EV_ATE_NORMAL);
    assert(e & EV_LEVEL_DONE);
    assert(g.mode == ST_LEVEL_COMPLETE);
    assert(g.leaf_normal.type == LEAF_NONE);     /* không sinh được lá mới */
  }

  /* ================= US3 / M5–M6 — lá đa dạng & power-up (T047–T048) ================= */

  /* ---- T047a: lá vàng → +50, mọc +1, biến mất; KHÔNG tính vào target ---- */
  {
    GameState g;
    game_init(&g, 0x901D01u);
    game_start(&g);                              /* màn 0, PLAYING */
    Cell h = worm_seg(&g, 0);
    g.leaf_normal.type = LEAF_NORMAL;            /* để xa, ngoài lối đi (tránh safety respawn) */
    g.leaf_normal.pos.c = 0; g.leaf_normal.pos.r = 0; g.leaf_normal.life_ms = -1;
    g.leaf_gold.type = LEAF_GOLD;                /* lá vàng ngay trước đầu */
    g.leaf_gold.pos.c = (int8_t)(h.c + 1); g.leaf_gold.pos.r = h.r;
    g.leaf_gold.life_ms = GOLD_LIFE_MS;
    uint16_t len0 = g.worm.len;
    uint32_t sc0 = g.score;
    GameEvents e = game_step(&g, (InputEvent){ IN_NONE, DIR_RIGHT }, g.step_ms);
    assert(e & EV_ATE_GOLD);
    assert(g.score == sc0 + SCORE_GOLD);         /* +50 */
    assert(g.worm.len == len0 + 1);              /* mọc +1 */
    assert(g.leaf_gold.type == LEAF_NONE);       /* biến mất */
    assert(g.leaves_eaten == 0);                 /* vàng không tính target */
    assert(g.mode == ST_PLAYING);
  }

  /* ---- T047b: lá vàng quá hạn → tự biến mất (EV_LEAF_EXPIRED), không ăn ---- */
  {
    GameState g;
    game_init(&g, 0x901D02u);
    game_start(&g);
    Cell h = worm_seg(&g, 0);
    g.leaf_normal.type = LEAF_NORMAL;
    g.leaf_normal.pos.c = 0; g.leaf_normal.pos.r = 0; g.leaf_normal.life_ms = -1;
    g.leaf_gold.type = LEAF_GOLD;                /* đặt LỆCH lối đi (đi phải) */
    g.leaf_gold.pos.c = h.c; g.leaf_gold.pos.r = (int8_t)(h.r + 3);
    g.leaf_gold.life_ms = 100;                   /* sắp hết hạn */
    GameEvents e = game_step(&g, (InputEvent){ IN_NONE, DIR_RIGHT }, 200);  /* dt 200 > 100 */
    assert(e & EV_LEAF_EXPIRED);
    assert(g.leaf_gold.type == LEAF_NONE);
    assert(!(e & EV_ATE_GOLD));
    assert(g.mode == ST_PLAYING);
  }

  /* ---- T047c: lá độc → co POISON_SHRINK đốt, KHÔNG Game Over ---- */
  {
    GameState g;
    game_init(&g, 0x9015E0u);
    game_start(&g);                              /* màn 0 (trống) */
    Worm *w = &g.worm;                           /* sâu dài 5 nằm ngang, đầu (10,6) */
    w->head_idx = 0u; w->len = 5u; w->dir = DIR_RIGHT; w->next_dir = DIR_RIGHT;
    for (int k = 0; k < 5; k++) { w->body[k].c = (int8_t)(10 - k); w->body[k].r = 6; }
    g.leaf_normal.type = LEAF_NORMAL;
    g.leaf_normal.pos.c = 0; g.leaf_normal.pos.r = 0; g.leaf_normal.life_ms = -1;
    g.leaf_poison.type = LEAF_POISON;            /* lá độc ngay trước đầu */
    g.leaf_poison.pos.c = 11; g.leaf_poison.pos.r = 6; g.leaf_poison.life_ms = -1;
    rebuild_occupied(&g);
    uint32_t sc0 = g.score;
    GameEvents e = game_step(&g, (InputEvent){ IN_NONE, DIR_RIGHT }, g.step_ms);
    assert(e & EV_ATE_POISON);
    assert(!(e & EV_GAME_OVER));
    assert(g.mode == ST_PLAYING);
    assert(g.worm.len == (uint16_t)(5 - POISON_SHRINK));   /* co 2 → 3 */
    assert(g.leaf_poison.type == LEAF_NONE);
    assert(g.score == sc0);                      /* trên sàn → không phạt điểm */
  }

  /* ---- T047d: lá độc khi đã ở sàn LEN_MIN → −20 điểm (clamp ≥ 0), giữ độ dài ---- */
  {
    GameState g;
    game_init(&g, 0x9015E1u);
    game_start(&g);                              /* len = LEN_START = LEN_MIN = 3 */
    Cell h = worm_seg(&g, 0);
    g.score = 5u;                                /* 5 − 20 → clamp 0 */
    g.leaf_normal.type = LEAF_NORMAL;
    g.leaf_normal.pos.c = 0; g.leaf_normal.pos.r = 0; g.leaf_normal.life_ms = -1;
    g.leaf_poison.type = LEAF_POISON;
    g.leaf_poison.pos.c = (int8_t)(h.c + 1); g.leaf_poison.pos.r = h.r; g.leaf_poison.life_ms = -1;
    GameEvents e = game_step(&g, (InputEvent){ IN_NONE, DIR_RIGHT }, g.step_ms);
    assert(e & EV_ATE_POISON);
    assert(g.worm.len == LEN_MIN);               /* không xuống dưới sàn */
    assert(g.score == 0u);                       /* phạt clamp ≥ 0 */
    assert(g.mode == ST_PLAYING);
  }

  /* ---- T048a: power-up — bật PU_EFFECT_MS, trừ dt cuối bước; cùng loại = refresh; khác loại = stack ---- */
  {
    GameState g;
    game_init(&g, 0x9000U);
    game_start(&g);
    Cell h = worm_seg(&g, 0);
    g.leaf_normal.type = LEAF_NORMAL;
    g.leaf_normal.pos.c = 0; g.leaf_normal.pos.r = 0; g.leaf_normal.life_ms = -1;
    /* Ăn GHOST. */
    g.leaf_pu.type = LEAF_POWERUP; g.leaf_pu.pu_type = PU_GHOST;
    g.leaf_pu.pos.c = (int8_t)(h.c + 1); g.leaf_pu.pos.r = h.r; g.leaf_pu.life_ms = PU_LIFE_MS;
    uint16_t len0 = g.worm.len;
    GameEvents e = game_step(&g, (InputEvent){ IN_NONE, DIR_RIGHT }, 100);
    assert(e & EV_ATE_POWERUP);
    assert(g.leaf_pu.type == LEAF_NONE);
    assert(g.power[PU_GHOST - 1] == PU_EFFECT_MS - 100);   /* set 6000 → −100 cuối bước */
    assert(g.worm.len == len0);                  /* power-up KHÔNG mọc */

    /* Bước trôi (không ăn) → trừ tiếp 100. */
    e = game_step(&g, (InputEvent){ IN_NONE, DIR_RIGHT }, 100);
    assert(g.power[PU_GHOST - 1] == PU_EFFECT_MS - 200);

    /* Ăn GHOST lần nữa → refresh về PU_EFFECT_MS (rồi −100). */
    Cell h2 = worm_seg(&g, 0);
    g.leaf_pu.type = LEAF_POWERUP; g.leaf_pu.pu_type = PU_GHOST;
    g.leaf_pu.pos.c = (int8_t)(h2.c + 1); g.leaf_pu.pos.r = h2.r; g.leaf_pu.life_ms = PU_LIFE_MS;
    e = game_step(&g, (InputEvent){ IN_NONE, DIR_RIGHT }, 100);
    assert(g.power[PU_GHOST - 1] == PU_EFFECT_MS - 100);   /* refresh, không cộng dồn */

    /* Ăn SPEED → stack độc lập với GHOST; tick hiệu dụng nhanh hơn. */
    Cell h3 = worm_seg(&g, 0);
    g.leaf_pu.type = LEAF_POWERUP; g.leaf_pu.pu_type = PU_SPEED;
    g.leaf_pu.pos.c = (int8_t)(h3.c + 1); g.leaf_pu.pos.r = h3.r; g.leaf_pu.life_ms = PU_LIFE_MS;
    e = game_step(&g, (InputEvent){ IN_NONE, DIR_RIGHT }, 100);
    assert(g.power[PU_SPEED - 1] == PU_EFFECT_MS - 100);
    assert(g.power[PU_GHOST - 1] > 0);           /* GHOST vẫn chạy song song */
    assert(game_step_ms(&g) < g.step_ms);        /* SPEED → tick ngắn hơn nhịp cơ bản */
  }

  /* ---- T048b: GHOST cho đầu xuyên qua THÂN (không Game Over) ---- */
  {
    GameState g;
    game_init(&g, 0x9001U);
    game_start(&g);
    Worm *w = &g.worm;                           /* đầu (6,6) đi LEFT vào (5,6) = đốt thân giữa */
    w->head_idx = 0u; w->len = 5u; w->dir = DIR_LEFT; w->next_dir = DIR_LEFT;
    w->body[0] = (Cell){ 6, 6 };                 /* head */
    w->body[1] = (Cell){ 6, 5 };
    w->body[2] = (Cell){ 5, 5 };
    w->body[3] = (Cell){ 5, 6 };                 /* đầu sẽ đi vào đây (KHÁC ô đuôi) */
    w->body[4] = (Cell){ 5, 7 };                 /* tail */
    g.leaf_normal.type = LEAF_NORMAL;
    g.leaf_normal.pos.c = 0; g.leaf_normal.pos.r = 0; g.leaf_normal.life_ms = -1;
    rebuild_occupied(&g);
    g.power[PU_GHOST - 1] = PU_EFFECT_MS;        /* GHOST đang bật */
    GameEvents e = game_step(&g, (InputEvent){ IN_NONE, DIR_LEFT }, g.step_ms);
    assert(!(e & EV_GAME_OVER));                 /* xuyên thân, không chết */
    assert(g.mode == ST_PLAYING);
    Cell nh = worm_seg(&g, 0);
    assert(nh.c == 5 && nh.r == 6);
  }

  /* ---- T048c: PHASE cho đầu wrap qua biên sang cạnh đối diện ---- */
  {
    GameState g;
    game_init(&g, 0x9002U);
    game_start(&g);
    Worm *w = &g.worm;                           /* đầu sát biên phải (19,6) đi RIGHT */
    w->head_idx = 0u; w->len = 3u; w->dir = DIR_RIGHT; w->next_dir = DIR_RIGHT;
    w->body[0] = (Cell){ 19, 6 };
    w->body[1] = (Cell){ 18, 6 };
    w->body[2] = (Cell){ 17, 6 };
    g.leaf_normal.type = LEAF_NORMAL;
    g.leaf_normal.pos.c = 0; g.leaf_normal.pos.r = 0; g.leaf_normal.life_ms = -1;
    rebuild_occupied(&g);
    g.power[PU_PHASE - 1] = PU_EFFECT_MS;        /* PHASE đang bật */
    GameEvents e = game_step(&g, (InputEvent){ IN_NONE, DIR_RIGHT }, g.step_ms);
    assert(!(e & EV_GAME_OVER));                 /* không chết ở biên */
    assert(g.mode == ST_PLAYING);
    Cell nh = worm_seg(&g, 0);
    assert(nh.c == 0 && nh.r == 6);              /* wrap sang cạnh trái cùng hàng */
  }

  /* ================= US4 / M7 — MENU / PAUSE / chơi lại (T056) ================= */

  /* ---- T056a: MENU + SELECT → PLAYING (Start) ---- */
  {
    GameState g;
    game_init(&g, 0x4040u);
    assert(g.mode == ST_MENU);                   /* game_init dừng ở MENU (FR-014) */
    game_input_ui(&g, (InputEvent){ IN_SELECT, DIR_UP });
    assert(g.mode == ST_PLAYING);
    assert(g.score == 0 && g.level_idx == 0 && g.worm.len == LEN_START);
    assert(g.leaf_normal.type == LEAF_NORMAL);   /* lá đầu màn đã sinh */
  }

  /* ---- T056b: MENU nav 3 mục (START/ENDLESS/THEME), clamp 2 đầu; SELECT mục khoá = no-op ---- */
  {
    GameState g;
    game_init(&g, 0x4041u);
    assert(g.menu_sel == 0);
    game_input_ui(&g, (InputEvent){ IN_DIR, DIR_DOWN });   /* → ENDLESS */
    assert(g.menu_sel == 1);
    game_input_ui(&g, (InputEvent){ IN_DIR, DIR_DOWN });   /* → THEME */
    assert(g.menu_sel == 2);
    game_input_ui(&g, (InputEvent){ IN_DIR, DIR_DOWN });   /* clamp ở cuối */
    assert(g.menu_sel == 2);
    /* SELECT trên mục "SOON" (THEME) → không vào game, vẫn ở MENU. */
    game_input_ui(&g, (InputEvent){ IN_SELECT, DIR_UP });
    assert(g.mode == ST_MENU);
    /* Lên lại START rồi SELECT → vào PLAYING. */
    game_input_ui(&g, (InputEvent){ IN_DIR, DIR_UP });
    game_input_ui(&g, (InputEvent){ IN_DIR, DIR_UP });
    assert(g.menu_sel == 0);
    game_input_ui(&g, (InputEvent){ IN_DIR, DIR_UP });     /* clamp ở đầu */
    assert(g.menu_sel == 0);
    game_input_ui(&g, (InputEvent){ IN_SELECT, DIR_UP });
    assert(g.mode == ST_PLAYING);
  }

  /* ---- T056c: pause toggle — PLAYING + SELECT → PAUSED (dừng game); + SELECT → PLAYING ---- */
  {
    GameState g;
    game_init(&g, 0x4042u);
    game_start(&g);
    assert(g.mode == ST_PLAYING);
    Cell h0 = worm_seg(&g, 0);
    GameEvents e = game_step(&g, (InputEvent){ IN_SELECT, DIR_RIGHT }, g.step_ms);
    assert(g.mode == ST_PAUSED);
    assert(!(e & EV_MOVED));                      /* không dời sâu khi bấm pause */
    Cell h1 = worm_seg(&g, 0);
    assert(h1.c == h0.c && h1.r == h0.r);         /* sâu đứng yên */
    /* Trong PAUSED, game_step là no-op (dừng cập nhật game). */
    e = game_step(&g, (InputEvent){ IN_NONE, DIR_RIGHT }, g.step_ms);
    assert(e == 0u && g.mode == ST_PAUSED);
    /* Nút chính khi PAUSED → resume. */
    game_input_ui(&g, (InputEvent){ IN_SELECT, DIR_RIGHT });
    assert(g.mode == ST_PLAYING);
  }

  printf("test_game: all assertions passed (T019 init/start + T028-T031 core + "
         "T040 obstacles/levels + T047-T048 leaves/power-ups + T056 menu/pause); "
         "sizeof(GameState)=%lu\n", (unsigned long)sizeof(GameState));
  return 0;
}
