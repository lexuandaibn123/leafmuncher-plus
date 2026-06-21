# Host unit-test cho logic thuần (gcc, KHÔNG link HAL) — Nguyên tắc II / SC-006.
# Chạy: make -C test

CC      = gcc
CFLAGS  = -I../Core/Inc -Wall -Wextra -std=c11 -g

# Test files (thêm test_levels.c, test_rng.c… ở các phase sau).
TESTS = test_game.c

# Logic sources THUẦN (thêm dần khi hiện thực — Phase 2+):
#   SRC = ../Core/Src/rng.c ../Core/Src/game.c ../Core/Src/levels.c
# Phase 1: chưa hiện thực hàm nào nên để trống (test chỉ kiểm hằng số/kiểu).
SRC =

BIN = run_tests

all: run

$(BIN): $(TESTS) $(SRC)
	$(CC) $(CFLAGS) $(TESTS) $(SRC) -o $(BIN)

run: $(BIN)
	./$(BIN)

clean:
	rm -f $(BIN) $(BIN).exe

.PHONY: all run clean
