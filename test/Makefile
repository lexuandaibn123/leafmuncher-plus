# Host unit-test cho logic thuần (gcc, KHÔNG link HAL) — Nguyên tắc II / SC-006.
# Chạy tất cả: make -C test        | dọn: make -C test clean
# Mỗi file test_*.c có main() riêng → build thành 1 binary riêng.

CC      = gcc
CFLAGS  = -I../Core/Inc -Wall -Wextra -std=c11 -g

CORE    = ../Core/Src

# Mỗi binary = 1 file test + các logic source THUẦN cần thiết.
#   (thêm test_levels.c, test_game.c… link game.c/levels.c ở các phase sau)
all: test_game test_levels test_rng test_theme

test_game: test_game.c $(CORE)/game.c $(CORE)/rng.c $(CORE)/levels.c
	$(CC) $(CFLAGS) $^ -o $@
	./$@

test_theme: test_theme.c $(CORE)/theme.c
	$(CC) $(CFLAGS) $^ -o $@
	./$@

test_levels: test_levels.c $(CORE)/levels.c
	$(CC) $(CFLAGS) $^ -o $@
	./$@

test_rng: test_rng.c $(CORE)/rng.c
	$(CC) $(CFLAGS) $^ -o $@
	./$@

clean:
	rm -f test_game test_game.exe test_levels test_levels.exe test_rng test_rng.exe test_theme test_theme.exe

.PHONY: all clean test_game test_levels test_rng test_theme
