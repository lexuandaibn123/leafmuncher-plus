#include "store.h"

/* Mã hoá/giải mã và kiểm tra CRC32 cho dữ liệu lưu trữ. */

/* CRC32 chuẩn (poly phản chiếu 0xEDB88320) */
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

static uint32_t pd_payload_len(void)
{
  return (uint32_t)(sizeof(PersistData) - sizeof(((PersistData *)0)->crc));
}

bool store_pd_valid(const PersistData *pd)
{
  if (pd->magic != STORE_MAGIC) return false;
  if (pd->version > STORE_VERSION) return false;
  return pd->crc == store_crc32(pd, pd_payload_len());
}

void store_pd_defaults(PersistData *pd)
{
  pd->magic = STORE_MAGIC;
  pd->version = STORE_VERSION;
  pd->_rsv = 0u;
  pd->endless_high = 0u;
  pd->crc = store_crc32(pd, pd_payload_len());
}

bool store_sg_valid(const SavedGame *sg)
{
  if (sg->version != SAVE_VERSION) return false;
  return sg->crc == store_crc32(sg, (uint32_t)(sizeof(SavedGame) - sizeof(sg->crc)));
}
