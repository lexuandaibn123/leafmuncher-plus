# Contract: `store` — lưu trạng thái bền vững (Flash)

Object storage gọn cho dữ liệu cần giữ qua tắt nguồn. **Chỉ `store.c` chạm Flash HAL**; logic game
nhận/đưa giá trị qua tham số (Nguyên tắc II, III). Thể hiện peripheral **Flash nội**.

## Kiểu

```c
typedef struct {
  uint32_t magic;            // 0x4C4D2B01 ('LM+\1') nhận diện dữ liệu hợp lệ
  uint16_t version;          // version schema (migration sau này)
  uint16_t theme_id;         // ThemeId đã chọn
  uint32_t endless_high;     // điểm cao chế độ Vô tận
  uint32_t crc;              // checksum của các field trên
} PersistData;               // mở rộng thêm field ở cuối, tăng version
```

## API

| Hàm | Mô tả |
|---|---|
| `void store_init(void)` | Đọc Flash → cache RAM; nếu magic/version/crc sai → nạp **mặc định** (theme=FOREST, high=0). |
| `const PersistData *store_get(void)` | Trả cache hiện hành (đọc, không chạm Flash). |
| `void store_set_theme(ThemeId id)` | Cập nhật cache (chưa ghi Flash). |
| `void store_set_endless_high(uint32_t s)` | Cập nhật cache nếu `s` > giá trị cũ. |
| `bool store_commit(void)` | Ghi cài đặt (theme + điểm cao) xuống Flash; trả `false` nếu lỗi. |
| `bool store_has_save(PlayMode m)` | Có ô lưu hợp lệ cho chế độ `m` không (valid + version + crc đúng). |
| `bool store_save_game(PlayMode m, const GameState *s)` | Lưu snapshot ván của chế độ `m` xuống Flash. |
| `bool store_load_game(PlayMode m, GameState *out)` | Khôi phục ván đã lưu của `m` vào `out`; `false` nếu không hợp lệ. |
| `void store_clear_save(PlayMode m)` | Xóa ô lưu của chế độ `m` (gọi khi ván kết thúc — FR-031). |

## Hợp đồng / quyết định

- **Backing**: **1 sector Flash 16 KB ở Bank 2 — sector 12 @ `0x08100000`**, chứa cài đặt `PersistData`
  + 2 ô `SavedGame` (~1.9 KB / 16 KB → thừa ~8×). Code nằm Bank 1 → **dual-bank**: xóa/ghi Bank 2 không
  đơ instruction fetch ở Bank 1. Reserve sector này trong `STM32F429XX_FLASH.ld` (MEMORY riêng / `NOLOAD`).
- **Ghi**: **read-modify-write** qua RAM-mirror cụm record → **xóa cả sector → ghi lại 1 lần** (xóa là cả
  sector). Không wear-leveling (tần suất ghi thấp). Độ bền ~10.000 chu kỳ xóa ⇒ dư cho đồ án.
- **Thời gian**: xóa 16 KB ~ vài trăm ms → chỉ gọi lúc PAUSED/GAME_OVER (đã nêu mục Đồng thời).
- **Thời điểm commit**: chỉ `store_commit()` khi có thay đổi đáng lưu (lập điểm cao mới, đổi theme rồi
  rời menu) — KHÔNG ghi mỗi frame (tránh mòn Flash).
- **An toàn**: dữ liệu trống/hỏng → `store_init` nạp mặc định, không crash (Edge case spec, FR-027).
- **Cô lập**: `game.c` không gọi `store`/Flash; `tasks`/menu đọc-ghi `store` rồi truyền giá trị vào logic.
- **Đồng thời**: `store_commit`/`store_save_game` (erase/program Flash) chặn CPU vài ms → gọi ngoài vòng
  tick gắt (lúc Lưu & Thoát, hoặc chuyển trạng thái MENU/GAME_OVER), không trong `GameTask` đang chạy.
- **Bố cục**: 1 record cài đặt (`PersistData`, §2.7) + **2 ô lưu ván** (`SavedGame` cho MODE_LEVEL và
  MODE_ENDLESS, §2.8), nằm trong vùng Flash dành riêng (cùng sector hoặc các sector kề). `GameState` là
  POD → `store_save_game`/`store_load_game` chép thẳng byte + crc; sai version/crc → coi như không có
  (FR-032). `store_clear_save` chỉ cần đặt `valid=0` + ghi lại.
