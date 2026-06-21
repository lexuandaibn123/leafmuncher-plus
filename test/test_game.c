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

  /* ---- M3: game_input_ui tối thiểu — GAME_OVER + nút chính → chơi lại ---- */
  {
    GameState g;
    game_init(&g, 314u);
    game_start(&g);
    g.mode = ST_GAME_OVER; g.score = 123u;
    game_input_ui(&g, (InputEvent){ IN_SELECT, DIR_UP });
    assert(g.mode == ST_PLAYING);
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

  printf("test_game: all assertions passed (T019 init/start + T028-T031 core + "
         "T040 obstacles/levels); sizeof(GameState)=%lu\n", (unsigned long)sizeof(GameState));
  return 0;
}
