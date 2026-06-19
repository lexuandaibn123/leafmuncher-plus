# Build & Flash

Project xuất kiểu **Makefile**, dùng **arm-gcc + make + STM32_Programmer_CLI bundled sẵn
trong STM32CubeIDE** (không cần cài thêm). Script [`build.sh`](../../build.sh) tự dò đường dẫn các công cụ này.

## Lệnh

```bash
./build.sh          # biên dịch -> build/leafmuncher-plus.elf (.hex/.bin)
./build.sh flash    # nạp xuống bo qua ST-LINK (SWD) rồi reset
./build.sh clean    # xoá build/
```

## Toolchain bundled (trong CubeIDE 2.1.0)

| Công cụ | Vị trí (glob trong `…/STM32CubeIDE/plugins/`) |
|---|---|
| `arm-none-eabi-gcc` 14.3 | `*gnu-tools-for-stm32*/tools/bin` |
| `make` | `*externaltools.make*/tools/bin` |
| `openocd` | `*externaltools.openocd*/tools/bin` |
| `STM32_Programmer_CLI` | `*cubeprogrammer*/tools/bin` |

## Nạp bằng cách khác

- **STM32CubeIDE GUI:** import project rồi Run/Debug như bình thường.
- **OpenOCD:** `openocd -f board/stm32f429discovery.cfg -c "program build/leafmuncher-plus.elf verify reset exit"`.

## Debug (tuỳ chọn, VSCode)

Cài extension **Cortex-Debug**, trỏ `gdb` + `STM32_Programmer`/`openocd` tới các đường dẫn bundled ở trên,
launch config dùng `build/leafmuncher-plus.elf`.

## Ghi chú kích thước

Build nền (chưa có game) ~ `text 36 KB / bss 40 KB`. Flash 2 MB và RAM 256 KB + SDRAM 8 MB
nên thừa sức cho framebuffer (320×240×2 = 150 KB/buffer trong SDRAM).
