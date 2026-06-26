#include "store.h"

/* store_codec — phần THUẦN của module store: CRC32 + validate + defaults.
 * KHÔNG chạm HAL/Flash → biên dịch & host-test trên gcc (test_store.c, SC-006).
 * store.c (chạm Flash) gọi các hàm này. Hợp đồng: contracts/store.md. */

/* CRC32 chuẩn (poly phản chiếu 0xEDB88320, init 0xFFFFFFFF, xorout 0xFFFFFFFF) — bitwise,
 * không bảng (record chỉ 12 byte, tốc độ không quan trọng; nhỏ gọn). */
uint32_t store_crc32(const void *data, uint32_t len)
{
  const uint8_t *p = (const uint8_t *)data;
  uint32_t crc = 0xFFFFFFFFu;
  for (uint32_t i = 0; i < len; i++) {
    crc ^= p[i];
    for (int b = 0; b < 8; b++) {
      crc = (crc >> 1) ^ (0xEDB88320u & (uint32_t)(-(int32_t)(crc & 1u)));
    }
  }
  return crc ^ 0xFFFFFFFFu;
}

/* CRC phủ mọi field trừ chính `crc` (nằm cuối struct). */
static uint32_t pd_payload_len(void)
{
  return (uint32_t)(sizeof(PersistData) - sizeof(((PersistData *)0)->crc));
}

bool store_pd_valid(const PersistData *pd)
{
  if (pd->magic != STORE_MAGIC) return false;
  if (pd->version > STORE_VERSION) return false;          /* schema tương lai → bỏ */
  return pd->crc == store_crc32(pd, pd_payload_len());
}

void store_pd_defaults(PersistData *pd)
{
  pd->magic = STORE_MAGIC;
  pd->version = STORE_VERSION;
  pd->theme_id = (uint16_t)THEME_FOREST;
  pd->endless_high = 0u;
  pd->crc = store_crc32(pd, pd_payload_len());
}
