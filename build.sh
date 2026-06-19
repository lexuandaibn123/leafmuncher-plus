#!/usr/bin/env bash
# Build / flash helper cho LeafMuncher+ (dùng toolchain bundled trong STM32CubeIDE)
#   ./build.sh          -> biên dịch (make -j)
#   ./build.sh flash    -> nạp build/leafmuncher-plus.elf xuống bo qua ST-LINK
#   ./build.sh clean    -> make clean
set -e

TARGET=leafmuncher-plus
IDE_PLUGINS="/c/ST/STM32CubeIDE_2.1.0/STM32CubeIDE/plugins"

# Tìm các công cụ bundled (glob theo phiên bản, không hardcode số build)
GCC_BIN="$(ls -d "$IDE_PLUGINS"/*gnu-tools-for-stm32*/tools/bin 2>/dev/null | head -1)"
MAKE_EXE="$(ls "$IDE_PLUGINS"/*externaltools.make*/tools/bin/make.exe 2>/dev/null | head -1)"
PROG_CLI="$(ls "$IDE_PLUGINS"/*cubeprogrammer*/tools/bin/STM32_Programmer_CLI.exe 2>/dev/null | head -1)"

[ -n "$GCC_BIN" ]  || { echo "Khong tim thay arm-gcc trong CubeIDE plugins"; exit 1; }
[ -n "$MAKE_EXE" ] || { echo "Khong tim thay make.exe trong CubeIDE plugins"; exit 1; }

# make (native Windows) can duong dan kieu C:/... cho GCC_PATH
GCC_WIN="$(echo "$GCC_BIN" | sed 's|^/\([a-zA-Z]\)/|\1:/|')"

case "${1:-build}" in
  clean)
    "$MAKE_EXE" clean GCC_PATH="$GCC_WIN"
    ;;
  flash)
    [ -n "$PROG_CLI" ] || { echo "Khong tim thay STM32_Programmer_CLI"; exit 1; }
    "$PROG_CLI" -c port=SWD -w "build/$TARGET.elf" -rst
    ;;
  build|*)
    "$MAKE_EXE" -j4 GCC_PATH="$GCC_WIN"
    echo "OK -> build/$TARGET.elf"
    ;;
esac
