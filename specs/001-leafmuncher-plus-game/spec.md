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
2. **Given** đã đạt mục tiêu của màn, **When** điều kiện qua màn thỏa, **Then** hiện LEVEL_COMPLETE rồi sang màn kế với tốc độ cơ bản cao hơn.
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
2. **Given** đang PLAYING, **When** nhấn nút user trên bo, **Then** chuyển PAUSED và dừng cập nhật game; nhấn tiếp → tiếp tục.
3. **Given** đang GAME_OVER, **When** chọn "Chơi lại", **Then** ván mới bắt đầu từ màn 1 với điểm 0.

---

### Edge Cases

- Joystick rút ra / không kết nối → tín hiệu về mức giữa → coi như không bẻ lái (sâu đi thẳng), không lỗi.
- Sân chơi đầy (không còn ô trống để sinh lá) → coi như thắng màn.
- Người chơi cố quay đầu 180° tức thì → bị bỏ qua, giữ hướng cũ.
- Nhiều power-up kích hoạt liên tiếp → hiệu ứng cùng loại làm mới đồng hồ; khác loại theo luật ưu tiên đã định.
- Lá power-up/lá vàng không bao giờ sinh đè lên thân sâu hoặc chướng ngại.
- Lỗi khởi tạo phần cứng hiển thị/bộ nhớ → báo hiệu bằng LED đỏ và dừng an toàn (không treo im lặng).

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: Hệ thống MUST hiển thị sân chơi dạng lưới 20×13 ô (ô 16×16px) với dải HUD cao 32px ở trên cùng.
- **FR-002**: Hệ thống MUST cho người chơi điều khiển hướng đi của sâu bằng joystick analog, ánh xạ về 4 hướng (lên/xuống/trái/phải) dựa trên trục trội, có vùng chết (deadzone).
- **FR-003**: Hệ thống MUST chặn việc quay đầu 180° tức thì.
- **FR-004**: Hệ thống MUST làm sâu dài thêm 1 đốt và cộng 10 điểm khi ăn lá thường, rồi sinh lá thường mới ở ô trống ngẫu nhiên.
- **FR-005**: Hệ thống MUST kết thúc ván (GAME_OVER) khi đầu sâu chạm biên sân, chạm thân sâu, hoặc chạm chướng ngại (trừ khi power-up cho phép).
- **FR-006**: Hệ thống MUST hiển thị điểm hiện tại trên HUD trong khi chơi và điểm cuối ở màn GAME_OVER.
- **FR-007**: Hệ thống MUST hỗ trợ nhiều màn chơi, mỗi màn có bố cục chướng ngại cố định và mục tiêu qua màn (số lá/điểm).
- **FR-008**: Hệ thống MUST tăng tốc độ cơ bản của sâu khi lên màn mới.
- **FR-009**: Hệ thống MUST hiển thị trạng thái WIN khi hoàn thành màn cuối.
- **FR-010**: Hệ thống MUST hỗ trợ lá vàng: cộng 50 điểm, xuất hiện hiếm, và tự biến mất sau một khoảng thời gian nếu không ăn.
- **FR-011**: Hệ thống MUST hỗ trợ lá độc: làm sâu co ngắn hoặc trừ điểm (theo luật cố định), không gây Game Over.
- **FR-012**: Hệ thống MUST hỗ trợ power-up có thời hạn: tăng tốc, làm chậm, bất tử (xuyên thân), xuyên tường; mỗi power-up có đồng hồ đếm ngược và tự tắt khi hết giờ.
- **FR-013**: Hệ thống MUST đảm bảo lá và power-up không sinh đè lên thân sâu hoặc chướng ngại.
- **FR-014**: Hệ thống MUST cung cấp các trạng thái MENU, PLAYING, PAUSED, GAME_OVER, LEVEL_COMPLETE, WIN và chuyển trạng thái hợp lệ giữa chúng.
- **FR-015**: Hệ thống MUST cho điều hướng MENU bằng joystick (lên/xuống chọn option) và xác nhận bằng nút joystick.
- **FR-016**: Hệ thống MUST cho tạm dừng/tiếp tục ván bằng nút user trên bo.
- **FR-017**: Hệ thống MUST cho phép chơi lại từ màn GAME_OVER (bắt đầu ván mới, điểm 0).
- **FR-018**: Hệ thống MUST hiển thị hình ảnh mượt, không nhấp nháy và không xé hình trong suốt quá trình chơi.
- **FR-019**: Hệ thống MUST báo hiệu trạng thái: tín hiệu "hoạt động bình thường" và tín hiệu "lỗi/kết thúc" rõ ràng cho người chơi/giám khảo.
- **FR-020**: Khi joystick ở vùng chết, hệ thống MUST giữ sâu đi thẳng theo hướng hiện tại.

### Key Entities

- **Sâu (Worm)**: chuỗi đốt có thứ tự (đầu → đuôi) trên lưới; thuộc tính: độ dài, hướng hiện tại, hướng kế tiếp.
- **Lá / Mồi (Leaf)**: vị trí trên lưới + loại (thường, vàng, độc, power-up); lá vàng/power-up có hạn thời gian.
- **Power-up đang hiệu lực**: loại hiệu ứng + thời gian còn lại.
- **Màn chơi (Level)**: bố cục chướng ngại + mục tiêu qua màn + tốc độ cơ bản.
- **Phiên chơi (Game Session)**: điểm hiện tại, màn hiện tại, trạng thái máy trạng thái, sâu, danh sách lá, power-up đang hiệu lực.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Người chơi mới có thể bắt đầu và chơi trọn một ván "rắn cổ điển" (di chuyển, ăn lá, thua) mà không cần hướng dẫn ngoài màn hình.
- **SC-002**: Sâu phản hồi thay đổi hướng từ joystick trong vòng ≤ 1 bước di chuyển kể từ khi người chơi gạt cần.
- **SC-003**: Hình ảnh trong khi chơi không nhấp nháy/không xé hình mà người chơi quan sát được bằng mắt thường.
- **SC-004**: Tốc độ ván chơi ổn định và tăng dần qua các màn một cách cảm nhận được.
- **SC-005**: Mỗi mốc M1→M7 đều cho ra một bản nạp được và demo được (luôn có bản chạy được).
- **SC-006**: Toàn bộ luật gameplay cốt lõi (dài ra khi ăn, thua khi va chạm, không quay đầu 180°, lá không sinh đè thân) được kiểm chứng tự động ở mức logic, không phụ thuộc phần cứng.

## Assumptions

- Phần cứng cố định: kit STM32F429I-DISC1 (màn TFT 320×240 tích hợp) + một joystick analog 2 trục có nút nhấn, gắn ngoài; một người chơi.
- Chân kết nối joystick đã được chốt (VRx=PA5, VRy=PC3, nút=PB7) và ghi trong constitution.
- Âm thanh, lưu high-score vào bộ nhớ, và điều khiển bằng cảm biến nghiêng là **ngoài phạm vi v1** (stretch goals).
- Bộ tính năng theo lộ trình tăng dần M1→M7; phạm vi v1 ưu tiên hoàn thành tới M3 trước, các phần sau bổ sung dần.
- Số lượng màn chơi cụ thể và các hằng số (tốc độ, thời hạn lá vàng, thời lượng power-up) sẽ được chốt ở bước plan; mặc định hợp lý được dùng nếu không nêu.
