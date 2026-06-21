# Feature Specification: LeafMuncher+ (Sâu Ăn Lá+)

**Feature Branch**: `001-leafmuncher-plus-game`

**Created**: 2026-06-19

**Status**: Draft

**Input**: Game "Sâu Ăn Lá+" trên STM32F429I-DISC1 + joystick analog, bản mở rộng của rắn săn mồi Nokia (đồ án Hệ thống nhúng).

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Chơi sâu ăn lá cổ điển (Priority: P1)

Người chơi cầm joystick, điều khiển con sâu di chuyển trên lưới để ăn lá. Mỗi lá ăn được làm sâu dài thêm và tăng điểm. Trò chơi kết thúc khi sâu đụng tường hoặc tự cắn vào thân. Điểm hiển thị trên màn hình.

**Why this priority**: Đây là cốt lõi gameplay (MVP). Không có phần này thì không có game. Tương ứng mốc M3.

**Independent Test**: Nạp firmware, gạt joystick 4 hướng → sâu di chuyển theo; lái sâu tới lá → sâu dài ra, điểm +10; lái sâu vào tường hoặc vào thân → màn Game Over hiện điểm cuối. Chơi trọn một ván mà không cần bất kỳ tính năng nào khác.

**Acceptance Scenarios**:

1. **Given** đang ở màn PLAYING, **When** người chơi gạt joystick sang phải, **Then** sâu chuyển hướng sang phải ở bước kế (không cho quay đầu 180° tức thì).
2. **Given** đầu sâu ở ô liền kề lá thường, **When** sâu tiến vào ô có lá, **Then** sâu dài thêm 1 đốt, điểm tăng 10, và một lá mới xuất hiện ở ô trống ngẫu nhiên.
3. **Given** đang chơi, **When** đầu sâu chạm biên sân hoặc chạm thân chính nó, **Then** chuyển sang trạng thái GAME_OVER và hiển thị điểm cuối.
4. **Given** joystick để ở vị trí giữa (deadzone), **When** không có tác động, **Then** sâu tiếp tục đi thẳng theo hướng hiện tại.

---

### User Story 2 - Chướng ngại vật & nhiều màn chơi (Priority: P2)

Mỗi màn (level) có bố cục tường/chướng ngại cố định. Người chơi đạt mục tiêu (số lá/điểm) thì qua màn kế, tốc độ sâu tăng dần để khó hơn. Hoàn thành màn cuối thì thắng (WIN).

**Why this priority**: Tạo chiều sâu và độ thử thách; biến "rắn" thành "Plus". Tương ứng mốc M4.

**Independent Test**: Vào màn có chướng ngại → đụng chướng ngại làm Game Over; ăn đủ mục tiêu → chuyển LEVEL_COMPLETE rồi vào màn kế với tốc độ nhanh hơn.

**Acceptance Scenarios**:

1. **Given** màn có chướng ngại, **When** đầu sâu chạm ô chướng ngại, **Then** GAME_OVER (trừ khi đang có power-up bất tử/xuyên tường phù hợp).
2. **Given** đã đạt mục tiêu của màn, **When** điều kiện qua màn thỏa, **Then** hiện LEVEL_COMPLETE; **When** người chơi nhấn nút joystick, **Then** sang màn kế với tốc độ cơ bản cao hơn.
3. **Given** đang ở màn cuối, **When** đạt mục tiêu màn cuối, **Then** hiển thị trạng thái WIN.

---

### User Story 3 - Đa dạng lá & power-up (Priority: P3)

Ngoài lá thường còn có lá vàng (điểm cao, hiếm, tự biến mất), lá độc (gây hại), và lá power-up kích hoạt hiệu ứng có thời hạn: tăng tốc, làm chậm, bất tử (xuyên thân), xuyên tường.

**Why this priority**: Yếu tố "Plus" làm gameplay phong phú; xây trên nền đã chạy ổn. Tương ứng mốc M5–M6.

**Independent Test**: Cho từng loại lá xuất hiện → ăn lá vàng (+50 điểm), ăn lá độc (sâu co ngắn/trừ điểm), ăn lá power-up (hiệu ứng bật, có đồng hồ đếm ngược, tự tắt khi hết giờ).

**Acceptance Scenarios**:

1. **Given** lá vàng đang hiển thị, **When** sâu ăn lá vàng, **Then** điểm tăng 50; **When** quá thời hạn mà chưa ăn, **Then** lá vàng biến mất.
2. **Given** sâu ăn lá độc, **When** xử lý, **Then** sâu co ngắn (hoặc trừ điểm) theo luật đã định, không gây Game Over.
3. **Given** đã ăn power-up "bất tử", **When** trong thời gian hiệu lực sâu đi qua thân mình, **Then** không Game Over; **When** hết giờ, **Then** hiệu ứng tắt và luật va chạm trở lại bình thường.

---

### User Story 4 - Điều hướng menu & tạm dừng (Priority: P3)

Có màn MENU để bắt đầu chơi, màn PAUSE để tạm dừng, và màn GAME OVER để chơi lại. Điều hướng menu bằng cách đẩy joystick lên/xuống để chọn option và nhấn nút joystick để xác nhận; nút user trên bo để Pause khi đang chơi.

**Why this priority**: Hoàn thiện trải nghiệm và vòng lặp chơi lại. Tương ứng mốc M7.

**Independent Test**: Từ MENU đẩy joystick chọn "Start" rồi nhấn nút → vào PLAYING; trong PLAYING nhấn nút user → PAUSED; ở GAME_OVER chọn chơi lại → về MENU/PLAYING.

**Acceptance Scenarios**:

1. **Given** đang ở MENU, **When** đẩy joystick lên/xuống, **Then** lựa chọn đang sáng di chuyển theo; **When** nhấn nút joystick, **Then** kích hoạt lựa chọn đó.
2. **Given** đang PLAYING, **When** nhấn nút user trên bo, **Then** chuyển PAUSED và dừng cập nhật game; PAUSED là menu gồm **Tiếp tục / Lưu & Thoát / Thoát**; chọn "Tiếp tục" → trở lại PLAYING.
3. **Given** đang GAME_OVER, **When** chọn "Chơi lại", **Then** ván mới bắt đầu từ màn 1 với điểm 0.

---

### User Story 5 - Hai chế độ chơi: Màn & Vô tận (Priority: P2)

Ở Main menu, người chơi chọn **Chế độ Màn** (campaign 5 màn có WIN) hoặc **Chế độ Vô tận** (sân mở, không vật cản va chạm, chỉ tăng tốc dần theo số lá ăn, chơi tới khi chết). Chế độ Vô tận lưu **điểm cao nhất** và giữ lại kể cả khi tắt nguồn.

**Why this priority**: Tăng giá trị chơi lại và bề rộng gameplay; Vô tận tận dụng lại engine sẵn có.

**Independent Test**: Từ menu chọn "Vô tận" → chơi sân trống, tốc độ tăng dần khi ăn lá, không có màn/WIN; chết → hiện điểm + điểm cao; tắt/bật nguồn → điểm cao vẫn còn.

**Acceptance Scenarios**:

1. **Given** đang ở MENU, **When** chọn "Chế độ Màn" → Start, **Then** vào campaign 5 màn (US1/US2).
2. **Given** đang ở MENU, **When** chọn "Chế độ Vô tận" → Start, **Then** vào sân mở không vật cản, không mục tiêu/WIN; tốc độ tăng dần theo số lá ăn (clamp sàn tối thiểu).
3. **Given** đang chơi Vô tận, **When** sâu chết, **Then** GAME_OVER hiện điểm ván + điểm cao; nếu điểm ván > điểm cao cũ thì cập nhật điểm cao.
4. **Given** vừa lập điểm cao Vô tận, **When** tắt rồi bật lại nguồn, **Then** điểm cao vẫn được giữ.

---

### User Story 6 - Đổi theme (Khu rừng / Sa mạc) (Priority: P3)

Người chơi đổi theme hiển thị của game (Khu rừng hoặc Sa mạc) ở menu. Theme đổi **bảng màu** (sâu/lá/nền/HUD), **nền cảnh** và **hình vẽ chướng ngại** (rừng = thân cây, sa mạc = đá/xương rồng). Theme đã chọn được lưu lại giữa các lần bật nguồn. Theme **không** đổi luật chơi, layout ô chướng ngại hay va chạm.

**Why this priority**: Tăng tính nhận diện/đa dạng hình ảnh; thuần cosmetic nên ít rủi ro, làm sau khi gameplay ổn.

**Independent Test**: Ở menu đổi theme Rừng↔Sa mạc → màu/nền/hình chướng ngại đổi theo ngay; vào chơi thấy đúng theme; tắt/bật nguồn → theme đã chọn vẫn giữ.

**Acceptance Scenarios**:

1. **Given** đang ở MENU, **When** đổi theme sang "Sa mạc", **Then** bảng màu/nền/sprite chướng ngại chuyển sang Sa mạc, lưới và luật chơi không đổi.
2. **Given** đã chọn theme, **When** tắt rồi bật lại nguồn, **Then** game khởi động với theme đã chọn lần trước.

---

### User Story 7 - Lưu & tiếp tục ván (resume sau, kể cả ngày khác) (Priority: P3)

Khi tạm dừng, người chơi có thể **Lưu & Thoát** về Home; ván đang chơi được lưu xuống Flash. Sau này — kể cả sau khi tắt nguồn — vào Home chọn **Tiếp tục** để chơi tiếp đúng chỗ đã dừng. Mỗi chế độ (Màn / Vô tận) có **một ô lưu riêng**. Áp dụng cho **cả 2 chế độ**.

**Why this priority**: Trải nghiệm "chơi dở để mai chơi tiếp"; tận dụng module `store` (Flash) đã có.

**Independent Test**: Đang chơi → Pause → Lưu & Thoát → tắt nguồn → bật lại → Home → Tiếp tục → ván hiện đúng trạng thái đã lưu (sâu, điểm, level/tốc độ, theme); chơi tới khi chết → save tự mất.

**Acceptance Scenarios**:

1. **Given** đang PAUSED, **When** chọn "Lưu & Thoát", **Then** ván hiện tại được lưu vào ô lưu của chế độ đang chơi và về MENU; **When** chọn "Thoát (không lưu)", **Then** về MENU và **không** ghi đè ô lưu cũ.
2. **Given** một chế độ có ván đã lưu, **When** ở MENU chọn "Tiếp tục" cho chế độ đó (kể cả sau khi tắt/bật nguồn), **Then** khôi phục đúng trạng thái (sâu, lá, power-up còn hiệu lực, điểm, level/tốc độ, theme) và vào PLAYING.
3. **Given** đang chơi tiếp một ván đã lưu, **When** ván kết thúc (GAME_OVER/WIN), **Then** ô lưu của chế độ đó **bị xóa** (MENU ẩn "Tiếp tục" cho chế độ đó).
4. **Given** chế độ Màn và Vô tận đều có ván lưu, **Then** hai ô lưu **độc lập** — tiếp tục cái này không ảnh hưởng cái kia.

---

### Edge Cases

- Joystick rút ra / không kết nối → tín hiệu về mức giữa → coi như không bẻ lái (sâu đi thẳng), không lỗi.
- Sân chơi đầy (không còn ô trống để sinh lá) → coi như thắng màn.
- Người chơi cố quay đầu 180° tức thì → bị bỏ qua, giữ hướng cũ.
- Nhiều power-up kích hoạt liên tiếp → **stack độc lập**: mỗi loại có đồng hồ riêng chạy song song; ăn lại cùng loại thì làm mới (refresh) đồng hồ loại đó.
- Lá power-up/lá vàng không bao giờ sinh đè lên thân sâu hoặc chướng ngại.
- Lỗi khởi tạo phần cứng hiển thị/bộ nhớ → báo hiệu bằng LED đỏ và dừng an toàn (không treo im lặng).
- Chế độ Vô tận khi tốc độ chạm sàn tối thiểu → giữ nguyên tốc độ (không nhanh vô hạn).
- Bộ nhớ lưu trống/hỏng (lần đầu chạy hoặc checksum sai) → nạp mặc định (theme Khu rừng, điểm cao 0), không crash.
- Đổi theme chỉ ở MENU (không đổi giữa lúc đang chơi) để tránh phức tạp render.
- "Lưu & Thoát" nhưng ghi Flash thất bại → thông báo và về MENU theo kiểu best-effort (ô lưu cũ giữ nguyên), không treo.
- Ô lưu có version cũ (sau khi nạp firmware mới đổi cấu trúc state) → coi như không có ô lưu, ẩn "Tiếp tục".
- Tiếp tục một ván đã lưu rồi lại Pause → Lưu & Thoát → ghi đè chính ô lưu đó (vẫn 1 ô/chế độ).

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: Hệ thống MUST hiển thị sân chơi dạng lưới 20×13 ô (ô 16×16px) với dải HUD cao 32px ở trên cùng.
- **FR-002**: Hệ thống MUST cho người chơi điều khiển hướng đi của sâu bằng joystick analog, ánh xạ về 4 hướng (lên/xuống/trái/phải) dựa trên trục trội, có vùng chết (deadzone).
- **FR-003**: Hệ thống MUST chặn việc quay đầu 180° tức thì.
- **FR-004**: Hệ thống MUST làm sâu dài thêm 1 đốt và cộng 10 điểm khi ăn lá thường, rồi sinh lá thường mới ở ô trống ngẫu nhiên.
- **FR-005**: Hệ thống MUST kết thúc ván (GAME_OVER) khi đầu sâu chạm biên sân, chạm thân sâu, hoặc chạm chướng ngại (trừ khi power-up cho phép).
- **FR-006**: Hệ thống MUST hiển thị điểm hiện tại trên HUD trong khi chơi và điểm cuối ở màn GAME_OVER.
- **FR-007**: Hệ thống MUST hỗ trợ **5 màn chơi**, mỗi màn có bố cục chướng ngại cố định và mục tiêu qua màn (số lá thường ăn được trong màn).
- **FR-008**: Hệ thống MUST tăng tốc độ cơ bản của sâu khi lên màn mới.
- **FR-009**: Hệ thống MUST hiển thị trạng thái WIN khi hoàn thành màn cuối.
- **FR-010**: Hệ thống MUST hỗ trợ lá vàng: cộng 50 điểm, xuất hiện hiếm, và tự biến mất sau một khoảng thời gian nếu không ăn.
- **FR-011**: Hệ thống MUST hỗ trợ lá độc: ăn vào làm sâu **co 2 đốt**; nếu sâu đã ở độ dài tối thiểu thì **−20 điểm** thay vì co. Không gây Game Over. Điểm không âm.
- **FR-012**: Hệ thống MUST hỗ trợ power-up có thời hạn: **tăng tốc, làm chậm, bất tử (xuyên thân), xuyên tường**; mỗi power-up có đồng hồ đếm ngược và tự tắt khi hết giờ. "Xuyên tường" = **đầu sâu ra khỏi biên thì hiện ra ở cạnh đối diện (wrap)**; chướng ngại vật **vẫn chặn**. Khi nhiều loại cùng hiệu lực thì chạy song song (stack độc lập).
- **FR-013**: Hệ thống MUST đảm bảo lá và power-up không sinh đè lên thân sâu hoặc chướng ngại.
- **FR-014**: Hệ thống MUST cung cấp các trạng thái MENU, PLAYING, PAUSED, GAME_OVER, LEVEL_COMPLETE, WIN và chuyển trạng thái hợp lệ giữa chúng.
- **FR-015**: Hệ thống MUST cho điều hướng MENU bằng joystick (lên/xuống chọn option) và xác nhận bằng nút joystick.
- **FR-016**: Hệ thống MUST cho tạm dừng/tiếp tục ván bằng nút user trên bo.
- **FR-017**: Hệ thống MUST cho phép chơi lại từ màn GAME_OVER (bắt đầu ván mới, điểm 0).
- **FR-018**: Hệ thống MUST hiển thị hình ảnh mượt, không nhấp nháy và không xé hình trong suốt quá trình chơi.
- **FR-019**: Hệ thống MUST báo hiệu trạng thái: tín hiệu "hoạt động bình thường" và tín hiệu "lỗi/kết thúc" rõ ràng cho người chơi/giám khảo.
- **FR-020**: Khi joystick ở vùng chết, hệ thống MUST giữ sâu đi thẳng theo hướng hiện tại.
- **FR-021**: Khi đạt mục tiêu qua màn, hệ thống MUST hiển thị màn LEVEL_COMPLETE và **chỉ sang màn kế khi người chơi nhấn nút joystick** (không tự động chuyển).
- **FR-022**: Hệ thống MUST cung cấp 2 chế độ chơi chọn ở Main menu: **Chế độ Màn** (campaign 5 màn có WIN) và **Chế độ Vô tận**.
- **FR-023**: Chế độ Vô tận MUST là sân mở **không vật cản va chạm**, **không** mục tiêu/LEVEL_COMPLETE/WIN; tốc độ **tăng dần theo số lá thường đã ăn**, có sàn tối thiểu (không nhanh vô hạn). Lá đặc biệt và power-up mở khoá ngay từ đầu.
- **FR-024**: Chế độ Vô tận MUST hiển thị điểm ván + điểm cao nhất, và cập nhật điểm cao khi điểm ván vượt mức cũ.
- **FR-025**: Hệ thống MUST hỗ trợ ≥ 2 theme (**Khu rừng**, **Sa mạc**); theme đổi **bảng màu, nền cảnh và hình vẽ chướng ngại**, KHÔNG đổi luật chơi, layout ô chướng ngại hay quy tắc va chạm.
- **FR-026**: Hệ thống MUST cho chọn theme ở menu; áp dụng ngay cho hiển thị.
- **FR-027**: Hệ thống MUST lưu bền vững (giữ qua tắt nguồn) ít nhất: **điểm cao Vô tận** và **theme đã chọn**; khi dữ liệu lưu trống/hỏng MUST nạp giá trị mặc định (theme Khu rừng, điểm cao 0) mà không lỗi.
- **FR-028**: Trạng thái PAUSED MUST là một menu gồm **Tiếp tục**, **Lưu & Thoát**, **Thoát (không lưu)**; điều hướng bằng joystick + nút joystick. Áp dụng cả 2 chế độ.
- **FR-029**: "Lưu & Thoát" MUST lưu **snapshot ván hiện tại** (sâu, lá, power-up còn hiệu lực, điểm, level/tốc độ, chế độ) vào **ô lưu riêng theo chế độ** trong Flash rồi về MENU; "Thoát (không lưu)" MUST về MENU và không ghi đè ô lưu cũ.
- **FR-030**: MENU MUST cho **Tiếp tục** ván đã lưu của một chế độ (chỉ hiện khi ô lưu hợp lệ), khôi phục đúng trạng thái và vào PLAYING; ô lưu MUST giữ qua tắt nguồn.
- **FR-031**: Khi ván được tiếp tục từ ô lưu kết thúc (GAME_OVER/WIN), hệ thống MUST **xóa** ô lưu của chế độ đó.
- **FR-032**: Mỗi chế độ (Màn / Vô tận) MUST có **một ô lưu độc lập** (ghi đè trong cùng chế độ; hai chế độ không ảnh hưởng nhau). Ô lưu sai version/hỏng MUST bị coi như không có.

### Key Entities

- **Sâu (Worm)**: chuỗi đốt có thứ tự (đầu → đuôi) trên lưới; thuộc tính: độ dài, hướng hiện tại, hướng kế tiếp.
- **Lá / Mồi (Leaf)**: vị trí trên lưới + loại (thường, vàng, độc, power-up); lá vàng/power-up có hạn thời gian.
- **Power-up đang hiệu lực**: loại hiệu ứng + thời gian còn lại.
- **Màn chơi (Level)**: bố cục chướng ngại + mục tiêu qua màn + tốc độ cơ bản.
- **Phiên chơi (Game Session)**: chế độ chơi, điểm hiện tại, màn hiện tại, trạng thái máy trạng thái, sâu, danh sách lá, power-up đang hiệu lực.
- **Chế độ chơi (Game Mode)**: `LEVEL` (campaign 5 màn) hoặc `ENDLESS` (sân mở, tăng tốc, không WIN).
- **Theme**: định danh (Khu rừng / Sa mạc) + bảng màu (sâu/lá/nền/HUD) + nền cảnh + hình vẽ chướng ngại. Thuần hiển thị.
- **Bộ nhớ bền vững (Persistent Store)**: struct lưu qua tắt nguồn — điểm cao Vô tận, theme đã chọn (kèm magic/version để kiểm tính hợp lệ).
- **Ván đã lưu (Saved Game)**: snapshot toàn bộ trạng thái ván của một chế độ (sâu, lá, power-up còn hiệu lực, điểm, level/tốc độ) + cờ hợp lệ; mỗi chế độ một ô lưu, giữ qua tắt nguồn.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Người chơi mới có thể bắt đầu và chơi trọn một ván "rắn cổ điển" (di chuyển, ăn lá, thua) mà không cần hướng dẫn ngoài màn hình.
- **SC-002**: Sâu phản hồi thay đổi hướng từ joystick trong vòng ≤ 1 bước di chuyển kể từ khi người chơi gạt cần.
- **SC-003**: Hình ảnh trong khi chơi không nhấp nháy/không xé hình mà người chơi quan sát được bằng mắt thường.
- **SC-004**: Tốc độ ván chơi ổn định và tăng dần qua các màn một cách cảm nhận được.
- **SC-005**: Mỗi mốc M1→M8 đều cho ra một bản nạp được và demo được (luôn có bản chạy được).
- **SC-006**: Toàn bộ luật gameplay cốt lõi (dài ra khi ăn, thua khi va chạm, không quay đầu 180°, lá không sinh đè thân) được kiểm chứng tự động ở mức logic, không phụ thuộc phần cứng.

## Assumptions

- Phần cứng cố định: kit STM32F429I-DISC1 (màn TFT 320×240 tích hợp) + một joystick analog 2 trục có nút nhấn, gắn ngoài; một người chơi.
- Chân kết nối joystick đã được chốt (VRx=PA5, VRy=PC3, nút=PB7) và ghi trong constitution.
- **Lưu bền vững (điểm cao Vô tận + theme) qua Flash nội — nay TRONG phạm vi v1** (qua một module storage, xem FR-027).
- Âm thanh và điều khiển bằng cảm biến nghiêng vẫn **ngoài phạm vi v1** (stretch goals).
- 2 theme cho v1 (Khu rừng, Sa mạc), mở rộng thêm theme sau dễ dàng; theme thuần cosmetic (không đổi luật/layout).
- Chế độ Vô tận tái dùng engine PLAYING; khác biệt: không nạp chướng ngại, không mục tiêu/WIN, tăng tốc theo số lá ăn.
- Bộ tính năng theo lộ trình tăng dần M1→M8; phạm vi v1 ưu tiên hoàn thành tới M3 trước, các phần sau bổ sung dần.
- **5 màn chơi** (đã chốt). Các hằng số (tốc độ tick mỗi màn, thời hạn lá vàng, thời lượng power-up, tỉ lệ sinh lá đặc biệt) là *tunable* — giá trị cụ thể nằm trong `research.md`, có thể tinh chỉnh khi demo mà không đổi kiến trúc.
