# Phase 0 — Research & Decisions: LeafMuncher+

Chốt các hằng số mà spec cố ý để mở (xem [spec.md Assumptions](spec.md)) và các quyết định thiết kế.
Mỗi mục: **Decision / Rationale / Alternatives**. Hằng số đánh dấu *(tunable)* có thể tinh chỉnh khi
demo mà không đổi kiến trúc.

---

## 1. Hình học màn hình & lưới

- **Decision**: 320×240 RGB565. HUD y=0..31 (32px). Sân y=32..239 → **20 cột × 13 hàng**, ô 16×16px.
  Ô (c,r) → pixel `(c*16, 32 + r*16)`. **Biên ẩn**: đầu sâu ra ngoài `0..19 / 0..12` = chết (FR-005);
  vẽ 1 đường viền mảnh quanh sân cho dễ nhìn (không chiếm ô).
- **Rationale**: 20·16=320, 13·16=208, 208+32=240 — khít tuyệt đối, không thừa pixel. Biên ẩn giữ trọn
  260 ô chơi.
- **Alternatives**: dành vòng ngoài làm tường (giảm còn 18×11) — bỏ vì phí ô và phức tạp render.

## 2. Bộ đệm khung & bộ nhớ

- **Decision**: 2 framebuffer trong **SDRAM** (base `0xD0000000`), mỗi cái 320·240·2 = **153600 B
  (0x25800)**. Buffer A = `0xD0000000`, Buffer B = `0xD0025800`. RGB565, 1 LTDC layer.
- **Rationale**: SDRAM 8MB → 300KB không đáng kể; double-buffer cho swap VSYNC chống xé hình (NT V).
- **Alternatives**: 1 buffer (bỏ — xé hình); 2 LTDC layer blend (bỏ ở v1 — không cần, thêm phức tạp).

## 3. Vòng game & tốc độ

- **Decision**: `GameTask` chạy **`vTaskDelayUntil`** với chu kỳ `step_ms` theo level *(tunable)*:

  | Level | 1 | 2 | 3 | 4 | 5 |
  |---|---|---|---|---|---|
  | `step_ms` | 180 | 155 | 130 | 110 | 95 |

  Sàn cứng `STEP_MS_MIN = 70`. Power-up đổi tốc bằng *hệ số* trên `step_ms` hiện tại: **SPEED ×0.6**,
  **SLOW ×1.7**, clamp về `[STEP_MS_MIN, 400]`.
- **Rationale**: tick cố định → tốc độ ổn định cảm nhận được (SC-004); nhanh dần qua màn (FR-008).
- **Alternatives**: tick theo frame LTDC (bỏ — buộc logic dính tốc độ quét); gia tốc liên tục trong màn
  (bỏ — khó test, khó cảm nhận mốc).

## 4. Sâu (Worm)

- **Decision**: dài khởi đầu **3**, vị trí giữa sân, hướng **PHẢI**. Sàn độ dài `LEN_MIN = 3`. Sức chứa
  deque = 260 (ROWS·COLS). Mỗi bước: tiến đầu 1 ô; nếu không tăng trưởng thì nhả đuôi **cùng tick**.
- **Rationale**: chuẩn snake; nhả đuôi cùng tick là mấu chốt để va chạm ô-đuôi-cũ không chết oan (xem §7).
- **Alternatives**: dài khởi đầu 1 (bỏ — khó nhìn, dễ lỗi length=0 khi ăn lá độc).

## 5. Điểm & các loại lá *(tunable)*

| Loại | Điểm | Độ dài | Ghi chú |
|---|---|---|---|
| Lá thường | +10 | +1 | ăn xong sinh lá thường mới ở ô trống ngẫu nhiên (FR-004) |
| Lá vàng | +50 | +1 | hiếm, có hạn giờ, tự biến mất (FR-010) |
| Lá độc | −20 nếu ở `LEN_MIN`, ngược lại co **−2 đốt** | −2 (sàn `LEN_MIN`) | không Game Over (FR-011) |

- **Decision**: lá độc **co 2 đốt**; nếu độ dài đã ở `LEN_MIN` thì **−20 điểm** thay vì co (không bao giờ
  để length < `LEN_MIN`). Điểm không âm (clamp ≥ 0).
- **Rationale**: deterministic, dễ unit-test; bảo toàn bất biến độ dài.
- **Alternatives**: lá độc gây Game Over (bỏ — spec cấm); co 1 đốt (chọn 2 cho cảm giác "phạt" rõ).

## 6. Sinh lá vàng / lá độc / power-up *(tunable)*

- **Decision**: sau mỗi lần ăn lá thường, rút ngẫu nhiên các sự kiện phụ (tối đa 1 lá đặc biệt + 0–1
  power-up tồn tại cùng lúc):
  - **Lá vàng**: 15% mỗi lần ăn nếu chưa có lá vàng trên sân; tuổi thọ **8000 ms**, hết giờ tự biến mất.
  - **Lá độc**: từ **level ≥ 2**, 20% mỗi lần ăn nếu chưa có lá độc; **tồn tại đến khi bị ăn** (người
    chơi tránh được).
  - **Power-up**: từ **level ≥ 3**, 12% mỗi lần ăn nếu chưa có power-up; loại chọn ngẫu nhiên đều; tuổi
    thọ trên sân **10000 ms**.
  - Mọi lá/power-up sinh ở **ô trống** (không đè thân/chướng ngại/lá khác) — FR-013.
- **Rationale**: tần suất "thưa" giữ độ khó; mở khoá dần theo level khớp lộ trình M4→M6.
- **Alternatives**: power-up từ level 1 (bỏ — loãng MVP); lá vàng có giờ cố định toàn cục (bỏ — kém linh hoạt).

## 7. Quy tắc va chạm & 2 bẫy snake kinh điển

- **Decision**:
  1. **Chống quay đầu 180°** (FR-003): chỉ chấp nhận hướng mới khi `new != opposite(committed_dir)`,
     trong đó `committed_dir` là hướng **đã thực sự đi ở bước trước** (không phải `next_dir` đang chờ).
     Buffer **đúng 1** lệnh rẽ mỗi tick.
  2. **Va chạm thân trừ ô đuôi**: khi sâu **không** tăng trưởng, ô đuôi nhả ra cùng tick → đi vào ô đuôi
     cũ là **hợp lệ**. Kiểm va chạm thân bỏ qua ô đuôi sẽ nhả.
  3. Va chạm tường (biên / chướng ngại) → GAME_OVER, trừ khi power-up phù hợp đang bật (GHOST bỏ qua va
     thân; PHASE cho xuyên tường biên — xem §8).
- **Rationale**: hai lỗi này là nguyên nhân crash/chết-oan phổ biến nhất; là một phần SC-006 phải test.
- **Alternatives**: kiểm va chạm gồm cả ô đuôi (bỏ — chết oan); validate theo `next_dir` (bỏ — thủng bẫy
  gạt-2-lần-trong-1-tick).

## 8. Power-up: mô hình hiệu lực

- **Decision**: 4 loại — **SPEED** (tăng tốc), **SLOW** (làm chậm), **GHOST** (bất tử/xuyên thân),
  **PHASE** (xuyên tường biên = wrap sang cạnh đối diện). Thời lượng hiệu lực khi ăn = **6000 ms** *(tunable)*.
  - **Stack độc lập**: mỗi loại 1 đồng hồ riêng, các loại khác nhau cùng hiệu lực song song.
  - Cùng loại ăn lại → **làm mới (refresh)** đồng hồ.
  - **PHASE wrap**: đầu ra khỏi biên → xuất hiện ở cạnh đối diện cùng hàng/cột.
  - **GHOST hết giờ khi đầu còn chồng thân**: gia hạn ngầm tới khi đầu rời khỏi ô thân (không Game Over
    đột ngột).
- **Rationale**: stack độc lập đơn giản & vui hơn "luật ưu tiên"; refresh cùng loại khớp edge case spec;
  grace cho GHOST tránh chết oan lúc hết giờ.
- **Alternatives**: luật ưu tiên giữa các loại (spec gợi ý nhưng bỏ — phức tạp, kém vui); PHASE đi xuyên
  chướng ngại (bỏ ở v1 — chỉ xuyên *tường biên*, chướng ngại vẫn chặn để giữ thử thách).
- **Lưu ý spec**: spec đã được **cập nhật khớp** quyết định này — Edge Cases + FR-012 nay ghi "stack độc lập"
  (đồng bộ ngày 2026-06-21).

## 9. Level: số lượng, mục tiêu, layout

- **Decision**: **5 level**. Mục tiêu qua màn = **số lá thường đã ăn trong màn** *(tunable)*:

  | Level | Mục tiêu (lá) | Layout chướng ngại |
  |---|---|---|
  | 1 | 6 | trống (làm quen) |
  | 2 | 8 | 2 thanh ngang ngắn |
  | 3 | 10 | khung chữ thập giữa sân |
  | 4 | 12 | 4 khối góc |
  | 5 | 14 | mê cung thưa |

  Đạt mục tiêu level 5 → **WIN** (FR-009). Layout lưu dạng **bitmap 20×13** `const` trong `levels.c`.
- **Rationale**: 5 màn đủ cho demo đồ án; mục tiêu theo "lá ăn trong màn" dễ hiểu & dễ test; bitmap thuần
  → host-test được, không phụ thuộc HW.
- **Alternatives**: mục tiêu theo điểm (bỏ — lá vàng làm nhảy bậc khó đoán); sinh layout ngẫu nhiên (bỏ —
  khó cân bằng, khó test).

## 10. Joystick: ánh xạ & hiệu chỉnh

- **Decision**: ADC1 12-bit (0..4095), `HAL_ADC_Start_DMA(buf,2)` circular → `buf[0]=VRx(PA5)`,
  `buf[1]=VRy(PC3)`. Lúc boot **hiệu chỉnh center** = trung bình 16 mẫu mỗi trục. Deadzone
  `DEADZONE = ±500` quanh center *(tunable)*. Ánh xạ 4 hướng theo **trục trội**, có **hysteresis**: chỉ
  đổi hướng khi `|axis-center| > DEADZONE` **và** trục trội vượt trục kia ≥ **1.3×**. Trong deadzone →
  giữ hướng cũ (FR-020). Dấu trục Y xác định lúc hiệu chỉnh (tránh đảo lên/xuống do đấu dây).
- **Rationale**: hiệu chỉnh xử lý lệch tâm joystick; hysteresis chống rung ở ranh giới chéo; joystick rút
  ra → về center → deadzone → đi thẳng (edge case spec), không crash.
- **Alternatives**: ngưỡng cố định không hiệu chỉnh (bỏ — joystick lệch tâm gây trôi); 8 hướng (bỏ —
  lưới chỉ cần 4).

## 11. Nút bấm & debounce

- **Decision**: **JOY_SW (PB7)** = xác nhận MENU / chọn / chơi lại. **B1 user (PA0)** = Pause/Resume khi
  PLAYING. Lấy mẫu trong `InputTask` @ **50Hz (20ms)**, debounce yêu cầu ổn định **≥ 30ms**, phát sự kiện
  theo **cạnh nhấn** (press-edge), không theo giữ.
- **Rationale**: polling + edge-detect đơn giản, đủ nhanh, dễ kiểm soát hơn EXTI; Pause phải là *nhấn*
  không phải *giữ*.
- **Alternatives**: EXTI + debounce phần cứng (bỏ — phức tạp hơn mức cần).

## 12. Kiến trúc FreeRTOS & đồng bộ

- **Decision**: 3 task + đối tượng đồng bộ:
  - **InputTask** (prio cao hơn): 20ms đọc ADC buffer + nút → đẩy `InputEvent` vào **queue** (độ sâu 4,
    ghi đè cũ nếu đầy) cho GameTask.
  - **GameTask** (prio giữa): `vTaskDelayUntil(step_ms)`; rút input mới nhất, gọi `game_step`, ghi
    `GameState` vào **snapshot** (bảo vệ bằng **mutex**), báo RenderTask qua **task notification /
    counting semaphore** ("frame ready").
  - **RenderTask** (prio thấp hơn): chờ tín hiệu, đọc snapshot (khoá mutex tối thiểu để copy ô thay đổi),
    vẽ bằng `gfx`, gọi `gfx_present()`.
  - **ISR ngắt line LTDC**: thực hiện swap buffer đang chờ (VSYNC) — không nằm trong task.
- **Rationale**: đủ queue+mutex+semaphore như constitution yêu cầu (§2 ràng buộc); khoá mutex ngắn để
  DMA2D không bị chặn; `vTaskDelayUntil` cho nhịp ổn định.
- **Alternatives**: 1 super-loop (bỏ — không thể hiện RTOS, dễ nhấp nháy); chia sẻ state bằng biến
  volatile trần (bỏ — đua dữ liệu khi đọc nhiều trường).

## 13. RNG

- **Decision**: **xorshift32** thuần trong `rng.c` (`rng_seed(uint32_t)`, `rng_next()`,
  `rng_range(n)`). Seed lười: lấy giá trị `SysTick`/`DWT` tại **thời điểm bấm nút bắt đầu game đầu tiên**
  XOR với **LSB nhiễu ADC** tích luỹ lúc boot — seed truyền *vào* từ lớp `input`/`tasks`, không để `rng.c`
  gọi HAL.
- **Rationale**: chuỗi lá khác nhau mỗi ván (tránh seed cố định lặp lại); `rng.c` vẫn thuần → test được.
- **Alternatives**: `rand()` libc (bỏ — không kiểm soát, không nhúng gọn); RNG phần cứng (F429 không có TRNG).

## 14. Render: chiến lược vẽ & DMA2D

- **Decision**:
  - **PLAYING dùng dirty-rect**: mỗi bước chỉ vẽ lại **ô đầu mới + ô đuôi cũ (nếu nhả) + ô lá vừa ăn/mới
    sinh + vùng điểm HUD**. Đổi trạng thái → vẽ lại toàn khung 1 lần.
  - DMA2D: **R2M** tô đặc ô/HUD; **M2M** blit glyph font; **M2M_BLEND** phủ mờ PAUSE.
  - **`gfx_present()`** đặt cờ swap; áp dụng tại ngắt line LTDC (VSYNC) → không xé hình.
- **Rationale**: dirty-rect giảm tải DMA2D, mượt; blend cho hiệu ứng pause; swap VSYNC đạt FR-018/SC-003.
- **Alternatives**: vẽ lại cả khung mỗi tick (chấp nhận được ở tốc snake nhưng phí — chỉ dùng khi đổi state).

## 15. Font & tile art (quyết định nhẹ, không ảnh hưởng kiến trúc) *(tunable)*

- **Decision (mặc định)**: **font bitmap 8×16 ASCII** (tập `0-9 A-Z : + - ! space`, ~40 glyph) `const`
  trong flash → chữ trên màn dùng **ASCII** (SCORE/LEVEL/GAME OVER…). Tile **tô màu phẳng** + 1 chấm mắt
  cho đầu sâu. Bảng màu: sâu vàng-cam `#FFB400` / đầu cam đậm; lá thường xanh `#2ECC40`; lá vàng `#FFD700`
  (nhấp nháy); lá độc tím `#B10DC9`; chướng ngại xám `#555555`; nền sân `#0B1A0B`; power-up: SPEED cyan /
  SLOW lam / GHOST trắng / PHASE cam-gạch.
- **Rationale**: ASCII + fill-rect là đường nhẹ nhất, hợp ưu tiên M3; tương phản sâu-cam/lá-xanh rõ. Đây
  là quyết định **không khoá kiến trúc** — đổi sang sprite/tiếng-Việt sau được mà không sửa module.
- **Alternatives**: sprite 16×16 / chữ tiếng Việt có dấu — để dành sau M3 nếu còn thời gian (người dùng
  đã tạm hoãn chọn; dùng mặc định này để không chặn plan). Chi tiết ở [docs/ui/ui-design.md] (tạo ở M1).

## 16. Kiểm thử host (NT II / SC-006)

- **Decision**: thư mục `test/` build bằng **gcc** (không HAL), include `game.h`/`levels.h`/`rng.h`. Bao
  phủ: dài ra khi ăn; Game Over khi va tường/thân/chướng ngại; chặn 180° (kể cả gạt-2-lần); không sinh lá
  đè thân; sàn `LEN_MIN` khi ăn lá độc; lá vàng hết hạn; đồng hồ power-up bật/tắt/stack/refresh; sân đầy →
  thắng màn. Lệnh: `make -C test` (hoặc `test/run.sh`).
- **Rationale**: hiện thực hoá SC-006 — kiểm chứng luật core độc lập phần cứng; chạy nhanh trên PC.
- **Alternatives**: chỉ test trên bo (bỏ — chậm, khó tái lập, vi phạm tinh thần NT II).

## 17. Theme (cosmetic) — `theme` module

- **Decision**: 2 theme v1 — **THEME_FOREST**, **THEME_DESERT**. Mỗi theme là một struct `const` trong
  Flash: bảng màu (nền/lưới/HUD/sâu/4 loại lá/chướng ngại) + **sprite 16×16 cho ô chướng ngại** (rừng =
  thân cây, sa mạc = đá). `render` đọc theme hiện hành để vẽ; **ô chướng ngại vẫn do `levels` quyết định**.
  Đổi theme **chỉ ở MENU**. Hợp đồng: [contracts/theme.md](contracts/theme.md).
- **Rationale**: tách màu/sprite khỏi logic → giữ `game.c` thuần (NT II); thêm theme sau chỉ là thêm
  một struct, không sửa module.
- **Alternatives**: theme đổi layout/loại chướng ngại (bỏ — dính logic, mất tính test được); hardcode màu
  trong `render` (bỏ — không mở rộng theme được).

## 18. Chế độ chơi — LEVEL vs ENDLESS

- **Decision**: `GameState.play_mode ∈ {MODE_LEVEL, MODE_ENDLESS}` chọn ở Main menu (lưu ý: trường `mode`
  đã dùng cho trạng thái FSM, nên chế độ chơi là `play_mode`).
  - **MODE_LEVEL**: campaign 5 màn như §3/§9 (có LEVEL_COMPLETE/WIN).
  - **MODE_ENDLESS**: **không nạp chướng ngại**, không mục tiêu/WIN; `step_ms` bắt đầu = `ENDLESS_STEP0`
    (180ms) và **giảm `ENDLESS_STEP_DEC` (3ms) mỗi `ENDLESS_RAMP_EVERY` (3) lá thường ăn**, clamp sàn
    `STEP_MS_MIN`. Lá đặc biệt + power-up mở khoá **ngay từ đầu** (không gắn ngưỡng level). Chết → GAME_OVER
    với điểm + điểm cao.
- **Rationale**: tái dùng nguyên `game_step`; nhánh khác biệt nhỏ gọn theo `mode` → vẫn thuần, test được
  riêng. "Chỉ nhanh dần" theo yêu cầu người dùng.
- **Alternatives**: viết engine endless riêng (bỏ — trùng lặp); endless có chướng ngại sinh dần (bỏ —
  người dùng chọn "chỉ nhanh dần").

## 19. Lưu bền vững — `store` module (Flash)

- **Decision**: 1 record cài đặt `PersistData` {magic, version, theme_id, endless_high, crc} + **2 ô lưu ván**
  (§20) gộp trong **1 sector Flash 16 KB ở Bank 2 — sector 12 @ `0x08100000`**.
  - **Vì sao Bank 2**: F429ZI 2MB **dual-bank** → CPU nạp lệnh từ **Bank 1** (code) trong khi xóa/ghi
    **Bank 2** (store) ⇒ không đơ instruction fetch. Code (~36 KB, dự kiến <200 KB) nằm đầu Bank 1, không đụng.
  - **Ngân sách**: cài đặt ~20 B + 2×(GameState ~900 B + header) ≈ **~1.9 KB** ≪ **16 KB** (thừa ~8×).
  - **Read-modify-write**: giữ **RAM-mirror** cụm record; sửa → **xóa sector → ghi lại cả cụm 1 lần** (vì xóa
    là cả sector). `store_init` ở boot nạp mirror, sai magic/version/crc → mặc định (FOREST, high=0).
  - **Reserve linker**: khai báo vùng store là MEMORY riêng / section `NOLOAD` trong
    `STM32F429XX_FLASH.ld` để linker không đặt code/const đè.
  - **Commit thưa**: chỉ ghi khi Lưu&Thoát / lập điểm cao / đổi theme rời menu — KHÔNG ghi mỗi frame.
  - Chỉ `store.c` gọi `HAL_FLASH_*`. Hợp đồng: [contracts/store.md](contracts/store.md).
- **Rationale**: thoả FR-027 (giữ qua tắt nguồn) + thể hiện peripheral Flash; commit thưa → tránh mòn Flash;
  cô lập Flash khỏi logic (NT II/III).
- **Alternatives**: EEPROM emulation 2-sector của ST (bỏ ở v1 — thừa cho 1 record nhỏ, tần suất ghi thấp);
  backup SRAM/RTC (bỏ — mất khi mất pin VBAT).
- **Lưu ý độ bền/thời gian**: Flash F4 ~**10.000 chu kỳ xóa**/sector — commit thưa ⇒ thừa sức cho đồ án.
  Xóa sector 16 KB ~**vài trăm ms** → chỉ commit lúc **PAUSED/GAME_OVER** (game đang dừng), không lúc chạy
  tick. Mất điện giữa lúc ghi → magic+version+CRC giúp `store_init` phát hiện hỏng và nạp mặc định.

## 20. Pause nâng cao + Lưu/Tiếp tục ván (US7)

- **Decision**: PAUSED là **menu 3 mục** (Tiếp tục / Lưu & Thoát / Thoát-không-lưu), điều hướng joystick +
  nút. "Lưu & Thoát" → `store_save_game(play_mode, &state)` rồi về MENU. MENU hiện **"Tiếp tục [chế độ]"**
  khi `store_has_save(mode)`. **Mỗi chế độ 1 ô lưu** (2 ô); ván tiếp-tục khi kết thúc → `store_clear_save`.
  Snapshot = **toàn bộ `GameState`** (POD → chép byte + crc). Lưu power-up theo **ms còn lại** để khôi phục
  liền mạch.
- **Rationale**: `GameState` đã thuần/POD nên serialize trực tiếp — không cần code marshalling; tái dùng
  `store` (Flash). 3 mục cho người chơi chủ động (theo lựa chọn người dùng). 2 ô độc lập tránh ghi đè chéo.
- **Alternatives**: tự lưu khi về Home (bỏ — người dùng muốn chủ động Lưu/Thoát riêng); 1 ô lưu chung
  (bỏ — người dùng muốn mỗi mode 1 ô); chỉ lưu hạt giống + thao tác để replay (bỏ — phức tạp, không cần).
- **Lưu ý kích thước**: `GameState` ~1KB (worm 260·2B + occupied 260B + lá/power/scalars) → 2 ô + cài đặt
  vẫn nằm gọn trong 1 sector Flash.

---

## Tổng hợp hằng số (đặt trong `game.h`/`levels.h`)

```text
COLS=20  ROWS=13  CELL=16  HUD_H=32  SCREEN_W=320  SCREEN_H=240
FB_A=0xD0000000  FB_B=0xD0025800  FB_SIZE=0x25800
LEN_START=3  LEN_MIN=3  DIR_START=RIGHT
STEP_MS[1..5]={180,155,130,110,95}  STEP_MS_MIN=70
SPEED_FACTOR=0.6  SLOW_FACTOR=1.7  PU_EFFECT_MS=6000
SCORE_LEAF=10  SCORE_GOLD=50  POISON_SHRINK=2  POISON_PENALTY=20
GOLD_CHANCE=15%  GOLD_LIFE_MS=8000
POISON_CHANCE=20% (lvl>=2)  PU_CHANCE=12% (lvl>=3)  PU_LIFE_MS=10000
LEVELS=5  TARGET[1..5]={6,8,10,12,14}
DEADZONE=±500  HYSTERESIS=1.3x  INPUT_HZ=50  DEBOUNCE_MS=30
ENDLESS_STEP0=180  ENDLESS_STEP_DEC=3  ENDLESS_RAMP_EVERY=3  (clamp STEP_MS_MIN=70)
THEME_COUNT=2 {FOREST,DESERT}
STORE_MAGIC=0x4C4D2B01  STORE_VERSION=1  (1 sector Flash riêng)
```

> Tất cả mục *(tunable)* có thể chỉnh khi cân bằng gameplay lúc demo — không yêu cầu đổi kiến trúc/module.

## 21. Bring-up phần cứng hiển thị — gotchas ĐÃ XÁC MINH trên bo (M1)

Quá trình đưa LTDC + ILI9341 + SDRAM lên bo STM32F429I-DISC1 phát hiện 4 điểm bắt buộc; ghi lại để
không vấp lại (đối chiếu được vì máy chạy ảnh TouchGFX bình thường → phần cứng tốt, lỗi do cấu hình):

1. **SDRAM cần chuỗi init thiết bị.** `MX_FMC_Init()` (CubeMX) **chỉ** cấu hình controller. Phải tự gửi
   chuỗi: CLK_ENABLE → delay → PALL → AUTOREFRESH×8 → LOAD_MODE (BL=1, CAS=3) → `ProgramRefreshRate`.
   SDCLK = HCLK/2 = 36MHz → refresh count ≈ **542**. Thiếu → màn trắng/nhiễu. (đặt trong `gfx_init`).

2. **Panel ILI9341 RGB mode cần lệnh `0xB0 = 0xC2`** (RGB Interface Signal Control). Đây là mảnh hay bị
   thiếu nhất → biểu hiện **sọc ngang** dù framebuffer đồng nhất. Chuỗi init đúng = khớp **BSP ST/MaJerle**
   (gồm `0xCA`, `0xB0=0xC2`, MADCTL `0x36=0xC8`, `0xF6={0x01,0x00,0x06}`, set cửa sổ `0x2A/0x2B`, `0x2C`).

3. **DMA2D Register-to-Memory (R2M): màu nạp vào OCOLR phải là `ARGB8888`, KHÔNG phải RGB565.** DMA2D tự
   chuyển ARGB8888→RGB565 khi ghi framebuffer. Truyền thẳng RGB565 → **lệch/xoay kênh màu** (đỏ hiện ra
   lục…). `gfx` phải có helper `argb_from_565()` trước khi gọi `HAL_DMA2D_Start` ở chế độ R2M.

4. **LTDC pixel clock phải ~6MHz** (dải ILI9341 RGB ~6–10MHz). CubeMX mặc định cho PLLSAI ra **~25MHz**
   (PLLSAIN=50, R=2, DivR=2 với HCLK 72MHz) → quá nhanh ~4× → panel không chốt kịp. Hạ về ~6.25MHz
   (R=4, DivR=4). **Nên đặt trong CubeMX (Clock Config)**, hoặc reconfig PLLSAI ở USER CODE.

> Ghi chú clock: dự án đang chạy HCLK **72MHz** (PLLM4/PLLN72/P2). Cân nhắc nâng **180MHz** chuẩn F429
> (CPU nhanh hơn, SDCLK 90MHz gấp đôi băng thông) — khi đó tính lại refresh count SDRAM và PLLSAI.
