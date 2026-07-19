@echo off
rem ================================================================
rem  build_play.bat - Build ban choi thu tren PC (khong can bo mach)
rem  Dung:   test\build_play.bat     (chay tu dau cung duoc)
rem  Ra:     play_win.exe  - UI that trong cua so Windows
rem          play_host.exe - console ASCII (neu co test/play_host.c)
rem  Yeu cau: MinGW-w64 gcc (mac dinh tim o C:\mingw64\bin)
rem ================================================================
setlocal
pushd "%~dp0.."

where gcc >nul 2>nul
if errorlevel 1 set "PATH=C:\mingw64\bin;%PATH%"
where gcc >nul 2>nul
if errorlevel 1 (
  echo [LOI] Khong tim thay gcc. Cai MinGW-w64 roi sua duong dan trong script nay.
  popd
  exit /b 1
)

rem Logic thuan dung chung; theme.c chi ton tai o nhanh con he theme.
set "SRC_LOGIC=Core/Src/game.c Core/Src/rng.c Core/Src/levels.c Core/Src/store_codec.c"
if exist "Core\Src\theme.c" set "SRC_LOGIC=%SRC_LOGIC% Core/Src/theme.c"

echo === build play_win.exe (UI that) ===
gcc -ICore/Inc -Wall -Wextra -std=c11 test/play_win.c Core/Src/render.c Core/Src/font8x16.c %SRC_LOGIC% -o play_win.exe -lgdi32 -luser32
if errorlevel 1 (
  echo [LOI] Build play_win.exe that bai.
  popd
  exit /b 1
)

if exist "test\play_host.c" (
  echo === build play_host.exe (console) ===
  gcc -ICore/Inc -Wall -Wextra -std=c11 test/play_host.c %SRC_LOGIC% -o play_host.exe
)

echo.
echo OK! Chay:  .\play_win.exe   (mui ten/WASD lai, Enter/Space chon, ESC/Q thoat)
popd
endlocal
