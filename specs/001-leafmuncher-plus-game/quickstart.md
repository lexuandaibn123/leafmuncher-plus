# Quickstart — Kiểm chứng LeafMuncher+

Hướng dẫn **chạy & kiểm chứng** rằng game hoạt động đúng spec, theo từng mốc M1→M7. Chi tiết luật ở
[contracts/](contracts/) và [data-model.md](data-model.md). Đây là guide xác thực, **không** chứa code.

## Yêu cầu trước

- Kit **STM32F429I-DISC1** + joystick analog (VRx→PA5, VRy→PC3, SW→PB7, VCC→3V3, GND→GND).
- Toolchain bundled trong STM32CubeIDE (xem [docs/setup/02-build-flash.md](../../docs/setup/02-build-flash.md)).
- `gcc` host để chạy unit-test logic.

## Lệnh

```bash
./build.sh          # biên dịch firmware → build/leafmuncher-plus.elf
./build.sh flash    # nạp xuống bo qua ST-LINK rồi reset
./build.sh clean    # xoá build/
make -C test        # build & chạy unit-test logic thuần trên PC (SC-006)
```

**Cổng bắt buộc (NT VII):** mỗi mốc chỉ "xong" khi `./build.sh` **0 error** và quan sát được trên bo.

---

## Kiểm chứng theo mốc

### M1 — Bring-up HW & gfx
- [ ] `./build.sh` 0 error, nạp được.
- [ ] **Panel ILI9341 hiển thị** (init qua SPI5 thành công) — màn sáng, có nền + vài ô màu, **không nhấp nháy**.
- [ ] Gạt joystick 4 hướng → 1 ô vuông di chuyển tương ứng; thả về giữa → ô đứng yên.
- [ ] **LED xanh nhấp nháy heartbeat đều (~1Hz) do Timer TIM7** (chứng tỏ Timer + interrupt chạy).
- [ ] Nhấn nút joystick / nút B1 → LED/state đổi. LED xanh = OK.
- [ ] Rút joystick ra → ô đứng yên, **không treo/không reset**.

### M2 — Khung engine
- [ ] `make -C test` build & chạy (dù test còn ít) — chứng tỏ logic tách biên dịch host được.
- [ ] Trên bo: "đầu sâu" chạy trên lưới theo **nhịp tick cố định**, đổi hướng theo joystick.
- [ ] 3 task chạy đồng thời (Input/Game/Render) không nhấp nháy, không kẹt.

### M3 — Rắn cổ điển (MVP) ⭐
Bám [contracts/game-core.md](contracts/game-core.md) + Acceptance Scenarios US1.
- [ ] **AS1**: đang PLAYING, gạt phải → sâu rẽ phải ở bước kế; gạt ngược 180° → **bị bỏ qua**.
- [ ] **AS2**: đầu vào ô lá → `len+1`, điểm +10, lá mới xuất hiện ở ô trống khác.
- [ ] **AS3**: đầu chạm biên **hoặc** thân → GAME_OVER, hiện điểm cuối, LED đỏ.
- [ ] **AS4**: joystick giữa (deadzone) → sâu đi thẳng.
- [ ] Đi vào **ô đuôi vừa nhả** (khi không ăn) → **không** chết.
- [ ] `make -C test`: pass các test grow/collide/180°/spawn-không-đè-thân/deadzone.
- [ ] SC-001: người chơi mới chơi trọn 1 ván không cần hướng dẫn ngoài màn hình.
- [ ] SC-002: đổi hướng phản hồi trong ≤ 1 bước.

### M4 — Levels & chướng ngại
Bám US2 + [contracts/levels.md](contracts/levels.md).
- [ ] Đụng ô chướng ngại → GAME_OVER.
- [ ] Ăn đủ `target_leaves` → LEVEL_COMPLETE → sang màn kế, **tốc độ nhanh hơn cảm nhận được** (SC-004).
- [ ] Đạt mục tiêu màn cuối → **WIN**.
- [ ] **Sân đầy** (sâu rất dài, hết ô trống sinh lá) → coi như **thắng màn** (LEVEL_COMPLETE/WIN).
- [ ] `make -C test`: target>0, step_ms giảm dần, ô khởi đầu trống ở mọi level; nhánh board-full→thắng-màn.

### M5 — Lá đa dạng
Bám US3 (lá vàng/độc).
- [ ] Lá vàng xuất hiện (hiếm), ăn → **+50**; để quá `GOLD_LIFE_MS` chưa ăn → **tự biến mất**.
- [ ] Ăn lá độc → sâu **co ngắn** (hoặc trừ điểm khi ở sàn độ dài), **không** Game Over.
- [ ] `make -C test`: lá vàng hết hạn; lá độc giữ `len ≥ LEN_MIN`.

### M6 — Power-up
Bám FR-012 + [contracts/game-core.md](contracts/game-core.md).
- [ ] Ăn power-up → hiệu ứng bật, HUD hiện **đồng hồ đếm ngược**, tự tắt khi hết giờ.
- [ ] GHOST: trong hiệu lực đi qua thân **không** Game Over; hết giờ → va chạm trở lại bình thường.
- [ ] PHASE: đầu ra biên → **wrap** sang cạnh đối diện.
- [ ] Ăn cùng loại lần nữa → đồng hồ **refresh**; khác loại → **stack** song song.
- [ ] `make -C test`: timer bật/trừ/tắt/refresh; stack độc lập.

### M7 — Menu / Pause / Polish
Bám US4.
- [ ] MENU: gạt lên/xuống đổi mục sáng; nút joystick xác nhận → vào PLAYING.
- [ ] PLAYING: nhấn nút B1 → PAUSED (game dừng, màn mờ + hộp "PAUSED"); nhấn tiếp → tiếp tục.
- [ ] GAME_OVER: chọn "Chơi lại" → ván mới từ màn 1, **điểm 0**.

---

## Định nghĩa "Done" của plan này
- [ ] M1→M7 đều có một bản **nạp được + demo được** (SC-005, NT VI).
- [ ] `make -C test` xanh cho toàn bộ luật core (SC-006).
- [ ] `./build.sh` 0 error ở mọi mốc (NT VII).
- [ ] Không nhấp nháy/không xé hình quan sát bằng mắt (SC-003).
- [ ] **8 peripheral bắt buộc đều nghiệm thu được** (constitution §2): GPIO, ADC+DMA, **Timer (TIM7)**, Interrupt, LTDC, FMC/SDRAM, DMA2D, FreeRTOS.
