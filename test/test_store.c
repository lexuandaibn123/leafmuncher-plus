/* test_store — unit-test codec THUẦN của module store (gcc). US5 / T071.
 * Chỉ kiểm phần thuần (store_codec.c): CRC32, validate (magic/version/crc), defaults.
 * Phần Flash thật (store.c) chạm HAL → nghiệm thu on-board (T078), không host-test. */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "store.h"

int main(void)
{
  /* T071a — CRC32 ổn định & nhạy thay đổi: chuỗi biết trước + đổi 1 byte → CRC khác. */
  assert(store_crc32("123456789", 9) == 0xCBF43926u);   /* vector CRC-32 chuẩn */
  uint8_t a[4] = {1, 2, 3, 4}, b[4] = {1, 2, 3, 5};
  assert(store_crc32(a, 4) != store_crc32(b, 4));

  /* T071b — defaults: magic/version đúng, theme=FOREST, high=0, và TỰ hợp lệ. */
  PersistData pd;
  store_pd_defaults(&pd);
  assert(pd.magic == STORE_MAGIC);
  assert(pd.version == STORE_VERSION);
  assert(pd.theme_id == THEME_FOREST);
  assert(pd.endless_high == 0u);
  assert(store_pd_valid(&pd));

  /* T071c — round-trip: cập nhật field rồi tính lại crc → vẫn hợp lệ; đọc lại đúng giá trị. */
  pd.theme_id = THEME_DESERT;
  pd.endless_high = 12345u;
  pd.crc = store_crc32(&pd, (uint32_t)(sizeof(PersistData) - sizeof(pd.crc)));
  assert(store_pd_valid(&pd));
  assert(pd.theme_id == THEME_DESERT && pd.endless_high == 12345u);

  /* T071d — hỏng crc → invalid (fallback về mặc định ở store_init). */
  pd.crc ^= 0xFFFFFFFFu;
  assert(!store_pd_valid(&pd));

  /* T071e — magic sai (Flash trống = 0xFFFFFFFF) → invalid. */
  PersistData blank;
  memset(&blank, 0xFF, sizeof(blank));
  assert(!store_pd_valid(&blank));

  /* T071f — version tương lai (> hiện hành) → invalid (chặn đọc nhầm schema mới). */
  store_pd_defaults(&pd);
  pd.version = STORE_VERSION + 1u;
  pd.crc = store_crc32(&pd, (uint32_t)(sizeof(PersistData) - sizeof(pd.crc)));
  assert(!store_pd_valid(&pd));

  printf("test_store: all assertions passed (T071 codec: crc/defaults/roundtrip/fallback)\n");
  return 0;
}
