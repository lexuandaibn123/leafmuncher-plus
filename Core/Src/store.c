#include "store.h"
#include "main.h"   /* HAL Flash (HAL_FLASH_*, HAL_FLASHEx_Erase, FLASH_SECTOR_4) */

/* store — phần chạm Flash (HAL). CHỈ file này đụng Flash (Nguyên tắc II). Giữ 1 cache RAM của
 * PersistData; đọc lúc boot, ghi khi store_commit. Hợp đồng: contracts/store.md.
 *
 * Backing: sector 4 single-bank @ 0x08010000 (64 KB), reserve trong STM32F429XX_FLASH.ld
 * (vùng STORE riêng) → code không bao giờ nằm đè. Ghi = read-modify-write: sửa cache → erase
 * cả sector → program lại record. Erase 64 KB dừng CPU ~vài trăm ms–1 s → chỉ gọi lúc rời
 * MENU/GAME_OVER (ngoài vòng tick GameTask). Codec thuần (crc/validate/defaults) ở store_codec.c. */

#define STORE_ADDR    0x08010000u   /* đầu sector 4 (Bank 1) */
#define STORE_SECTOR  FLASH_SECTOR_4

/* Layout phải chia hết 4 (ghi Flash theo word). */
_Static_assert(sizeof(PersistData) % 4u == 0u, "PersistData phai chia het 4 byte");

static PersistData s_cache;

void store_init(void)
{
  const PersistData *flash = (const PersistData *)STORE_ADDR;
  if (store_pd_valid(flash)) {
    s_cache = *flash;                 /* dữ liệu hợp lệ → nạp vào cache */
  } else {
    store_pd_defaults(&s_cache);      /* trống/hỏng → mặc định (FR-027, không crash) */
  }
}

const PersistData *store_get(void)
{
  return &s_cache;
}

void store_set_theme(ThemeId id)
{
  s_cache.theme_id = (uint16_t)id;
}

void store_set_endless_high(uint32_t s)
{
  if (s > s_cache.endless_high) {
    s_cache.endless_high = s;         /* chỉ giữ điểm cao nhất */
  }
}

bool store_commit(void)
{
  /* Tính lại crc cho cache trước khi ghi. */
  s_cache.magic = STORE_MAGIC;
  s_cache.version = STORE_VERSION;
  s_cache.crc = store_crc32(&s_cache, (uint32_t)(sizeof(PersistData) - sizeof(s_cache.crc)));

  if (HAL_FLASH_Unlock() != HAL_OK) {
    return false;
  }

  /* Xóa cả sector 4 (đưa về 0xFF) trước khi program. */
  FLASH_EraseInitTypeDef erase = {0};
  erase.TypeErase    = FLASH_TYPEERASE_SECTORS;
  erase.Banks        = FLASH_BANK_1;
  erase.Sector       = STORE_SECTOR;
  erase.NbSectors    = 1u;
  erase.VoltageRange = FLASH_VOLTAGE_RANGE_3;   /* Vdd 2.7–3.6 V → ghi theo word */
  uint32_t sector_err = 0u;
  if (HAL_FLASHEx_Erase(&erase, &sector_err) != HAL_OK) {
    HAL_FLASH_Lock();
    return false;
  }

  /* Program record theo từng word (16 byte = 4 word). */
  const uint32_t *w = (const uint32_t *)&s_cache;
  bool ok = true;
  for (uint32_t i = 0; i < sizeof(PersistData) / 4u; i++) {
    if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, STORE_ADDR + i * 4u, w[i]) != HAL_OK) {
      ok = false;
      break;
    }
  }

  HAL_FLASH_Lock();

  /* Xác nhận đã ghi đúng (đọc lại Flash so với cache). */
  if (ok) {
    const PersistData *flash = (const PersistData *)STORE_ADDR;
    ok = store_pd_valid(flash) && (flash->theme_id == s_cache.theme_id) &&
         (flash->endless_high == s_cache.endless_high);
  }
  return ok;
}
