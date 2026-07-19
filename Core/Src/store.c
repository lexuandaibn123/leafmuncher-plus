#include "store.h"
#include "main.h"   /* HAL Flash */
#include <string.h>

/* Lưu dữ liệu vào Flash
 * Backing: sector 4 single-bank @ 0x08010000 (64 KB). 
 * Mọi lần ghi = read-modify-write mức sector: erase cả sector rồi program lại TOÀN BỘ.
 * Erase 64 KB mất một khoảng thời gian đáng kể nên cẩn thận khi gọi.
 */

#define STORE_ADDR    0x08010000u
#define STORE_SECTOR  FLASH_SECTOR_4

/* Bố cục trong sector: cài đặt ở đầu, rồi 2 ô lưu liên tiếp. */
#define OFF_SETTINGS  0u
#define OFF_SAVE(m)   ((uint32_t)sizeof(PersistData) + (uint32_t)(m) * (uint32_t)sizeof(SavedGame))

_Static_assert(sizeof(PersistData) % 4u == 0u, "PersistData phai chia het 4 byte");
_Static_assert(sizeof(SavedGame)   % 4u == 0u, "SavedGame phai chia het 4 byte");
_Static_assert((sizeof(PersistData) + 2u * sizeof(SavedGame)) <= (64u * 1024u),
               "store khong vua sector 4 (64 KB)");

static PersistData s_settings;
static SavedGame   s_save[2];

void store_init(void)
{
  const PersistData *f = (const PersistData *)(STORE_ADDR + OFF_SETTINGS);
  if (store_pd_valid(f)) {
    s_settings = *f;
  } else {
    store_pd_defaults(&s_settings);
  }

  for (uint32_t m = 0u; m < 2u; m++) {
    const SavedGame *sf = (const SavedGame *)(STORE_ADDR + OFF_SAVE(m));
    if (store_sg_valid(sf)) {
      s_save[m] = *sf;
    } else {
      memset(&s_save[m], 0, sizeof s_save[m]);
      s_save[m].version = (uint16_t)SAVE_VERSION;
    }
  }
}

const PersistData *store_get(void)
{
  return &s_settings;
}

void store_set_endless_high(uint32_t s)
{
  if (s > s_settings.endless_high) {
    s_settings.endless_high = s;
  }
}

static bool program_words(uint32_t addr, const void *data, uint32_t size)
{
  const uint32_t *w = (const uint32_t *)data;
  for (uint32_t i = 0u; i < size / 4u; i++) {
    if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr + i * 4u, w[i]) != HAL_OK) {
      return false;
    }
  }
  return true;
}

static bool flash_write_all(void)
{
  s_settings.magic = STORE_MAGIC;
  s_settings.version = STORE_VERSION;
  s_settings.crc = store_crc32(&s_settings, (uint32_t)(sizeof(PersistData) - sizeof(s_settings.crc)));

  if (HAL_FLASH_Unlock() != HAL_OK) {
    return false;
  }

  FLASH_EraseInitTypeDef erase = {0};
  erase.TypeErase    = FLASH_TYPEERASE_SECTORS;
  erase.Banks        = FLASH_BANK_1;
  erase.Sector       = STORE_SECTOR;
  erase.NbSectors    = 1u;
  erase.VoltageRange = FLASH_VOLTAGE_RANGE_3;
  uint32_t sector_err = 0u;
  if (HAL_FLASHEx_Erase(&erase, &sector_err) != HAL_OK) {
    HAL_FLASH_Lock();
    return false;
  }

  bool ok = program_words(STORE_ADDR + OFF_SETTINGS, &s_settings,  sizeof s_settings) &&
            program_words(STORE_ADDR + OFF_SAVE(0),  &s_save[0],   sizeof s_save[0]) &&
            program_words(STORE_ADDR + OFF_SAVE(1),  &s_save[1],   sizeof s_save[1]);

  HAL_FLASH_Lock();

  if (ok) {
    const PersistData *f = (const PersistData *)(STORE_ADDR + OFF_SETTINGS);
    ok = store_pd_valid(f) && (f->endless_high == s_settings.endless_high);
  }
  return ok;
}

bool store_commit(void)
{
  return flash_write_all();
}

bool store_has_save(PlayMode m)
{
  return ((unsigned)m <= 1u) && (s_save[m].valid == 1u);
}

bool store_save_game(PlayMode m, const GameState *s)
{
  if ((unsigned)m > 1u) {
    return false;
  }
  memset(&s_save[m], 0, sizeof s_save[m]);
  s_save[m].valid   = 1u;
  s_save[m].version = (uint16_t)SAVE_VERSION;
  s_save[m].state   = *s;
  s_save[m].crc = store_crc32(&s_save[m], (uint32_t)(sizeof(SavedGame) - sizeof(s_save[m].crc)));
  return flash_write_all();
}

bool store_load_game(PlayMode m, GameState *out)
{
  if ((unsigned)m > 1u || s_save[m].valid != 1u) {
    return false;
  }
  *out = s_save[m].state;
  return true;
}

void store_clear_save(PlayMode m)
{
  if ((unsigned)m > 1u || s_save[m].valid == 0u) {
    return;
  }
  s_save[m].valid = 0u;
  s_save[m].crc = store_crc32(&s_save[m], (uint32_t)(sizeof(SavedGame) - sizeof(s_save[m].crc)));
  (void)flash_write_all();
}
