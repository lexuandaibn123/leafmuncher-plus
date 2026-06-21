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
- **Lưu ý spec**: spec mục Edge Cases nói "khác loại theo luật ưu tiên" — plan **thay bằng stack độc lập**;
  ghi nhận lệch để cập nhật spec ở bước review.

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
```

> Tất cả mục *(tunable)* có thể chỉnh khi cân bằng gameplay lúc demo — không yêu cầu đổi kiến trúc/module.
