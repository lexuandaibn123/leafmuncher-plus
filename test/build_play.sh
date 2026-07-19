#!/usr/bin/env bash
# ================================================================
#  build_play.sh — Build bản chơi thử trên PC (không cần bo mạch)
#  Dùng:   ./test/build_play.sh        (chạy từ đâu cũng được)
#  Ra:     play_win.exe  — UI thật trong cửa sổ Windows
#          play_host.exe — console ASCII (nếu có test/play_host.c)
#  Yêu cầu: MinGW-w64 gcc (mặc định tìm ở /c/mingw64/bin — Git Bash)
# ================================================================
set -e
cd "$(dirname "$0")/.."

command -v gcc >/dev/null 2>&1 || export PATH="/c/mingw64/bin:$PATH"
command -v gcc >/dev/null 2>&1 || { echo "[LOI] Khong tim thay gcc — cai MinGW-w64 hoac sua PATH trong script."; exit 1; }

# Logic thuần dùng chung; theme.c chỉ tồn tại ở nhánh còn hệ theme.
SRC="Core/Src/game.c Core/Src/rng.c Core/Src/levels.c Core/Src/store_codec.c"
[ -f Core/Src/theme.c ] && SRC="$SRC Core/Src/theme.c"

echo "=== build play_win.exe (UI that) ==="
gcc -ICore/Inc -Wall -Wextra -std=c11 test/play_win.c \
    Core/Src/render.c Core/Src/font8x16.c $SRC \
    -o play_win.exe -lgdi32 -luser32

if [ -f test/play_host.c ]; then
  echo "=== build play_host.exe (console) ==="
  gcc -ICore/Inc -Wall -Wextra -std=c11 test/play_host.c $SRC -o play_host.exe
fi

echo
echo "OK! Chay:  ./play_win.exe   (mui ten/WASD lai, Enter/Space chon, ESC/Q thoat)"
