#include "store.h"
#include "main.h"   /* HAL Flash (HAL_FLASH_*, HAL_FLASHEx_Erase, FLASH_SECTOR_4) */
#include <string.h>

/* store — phần chạm Flash (HAL). CHỈ file này đụng Flash (Nguyên tắc II). Giữ cache RAM của
 * cài đặt + 2 ô lưu ván; đọc lúc boot, ghi khi commit/save/clear. Hợp đồng: contracts/store.md.
 *
 * Backing: sector 4 single-bank @ 0x08010000 (64 KB), reserve trong STM32F429XX_FLASH.ld
 * (vùng STORE riêng) → code không bao giờ nằm đè. CẢ 3 record (PersistData + 2×SavedGame) nằm
 * trong CÙNG sector ⇒ mọi lần ghi = read-modify-write mức sector: erase cả sector rồi program lại
 * TOÀN BỘ từ cache RAM (xóa là cả sector — không thể chỉ ghi 1 record). Erase 64 KB dừng CPU
 * ~vài trăm ms–1 s → chỉ gọi lúc rời MENU/GAME_OVER/Lưu&Thoát (ngoài vòng tick GameTask).
 * Codec thuần (crc/validate/defaults) ở store_codec.c. */

#define STORE_ADDR    0x08010000u   /* đầu sector 4 (Bank 1) */
#define STORE_SECTOR  FLASH_SECTOR_4

/* Bố cục trong sector: cài đặt ở đầu, rồi 2 ô lưu liên tiếp (index = PlayMode). Mọi size chia hết 4. */
#define OFF_SETTINGS  0u
#define OFF_SAVE(m)   ((uint32_t)sizeof(PersistData) + (uint32_t)(m) * (uint32_t)sizeof(SavedGame))

_Static_assert(sizeof(PersistData) % 4u == 0u, "PersistData phai chia het 4 byte");
_Static_assert(sizeof(SavedGame)   % 4u == 0u, "SavedGame phai chia het 4 byte");
_Static_assert((sizeof(PersistData) + 2u * sizeof(SavedGame)) <= (64u * 1024u),
               "store khong vua sector 4 (64 KB)");

static PersistData s_settings;
static SavedGame   s_save[2];      /* index = PlayMode (MODE_LEVEL=0, MODE_ENDLESS=1) */

/* ── Đọc/khởi tạo cache từ Flash ────────────────────────────────────────────────── */
void store_init(void)
{
  const PersistData *f = (const PersistData *)(STORE_ADDR + OFF_SETTINGS);
  if (store_pd_valid(f)) {
    s_settings = *f;                  /* cài đặt hợp lệ → nạp */
  } else {
    store_pd_defaults(&s_settings);   /* trống/hỏng → mặc định (FR-027, không crash) */
  }

  for (uint32_t m = 0u; m < 2u; m++) {
    const SavedGame *sf = (const SavedGame *)(STORE_ADDR + OFF_SAVE(m));
    if (store_sg_valid(sf)) {
      s_save[m] = *sf;                /* ô lưu nguyên vẹn → nạp (valid 0 hoặc 1) */
    } else {
      memset(&s_save[m], 0, sizeof s_save[m]);
      s_save[m].version = (uint16_t)SAVE_VERSION;   /* trống: valid=0 */
    }
  }
}

const PersistData *store_get(void)
{
  return &s_settings;
}

void store_set_theme(ThemeId id)
{
  s_settings.theme_id = (uint16_t)id;
}

void store_set_endless_high(uint32_t s)
{
  if (s > s_settings.endless_high) {
    s_settings.endless_high = s;      /* chỉ giữ điểm cao nhất */
  }
}

/* ── Ghi xuống Flash: erase sector + program lại CẢ 3 record từ cache ──────────────── */
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

/* Tính lại crc cài đặt rồi xóa sector & ghi lại toàn bộ. Mọi đường ghi đi qua đây nên crc cài
 * đặt LUÔN đúng kể cả khi caller chỉ đổi ô lưu (settings cache rewrite cùng lúc). */
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
  erase.VoltageRange = FLASH_VOLTAGE_RANGE_3;   /* Vdd 2.7–3.6 V → ghi theo word */
  uint32_t sector_err = 0u;
  if (HAL_FLASHEx_Erase(&erase, &sector_err) != HAL_OK) {
    HAL_FLASH_Lock();
    return false;
  }

  bool ok = program_words(STORE_ADDR + OFF_SETTINGS, &s_settings,  sizeof s_settings) &&
            program_words(STORE_ADDR + OFF_SAVE(0),  &s_save[0],   sizeof s_save[0]) &&
            program_words(STORE_ADDR + OFF_SAVE(1),  &s_save[1],   sizeof s_save[1]);

  HAL_FLASH_Lock();

  /* Xác nhận cài đặt ghi đúng (đọc lại Flash). */
  if (ok) {
    const PersistData *f = (const PersistData *)(STORE_ADDR + OFF_SETTINGS);
    ok = store_pd_valid(f) && (f->theme_id == s_settings.theme_id) &&
         (f->endless_high == s_settings.endless_high);
  }
  return ok;
}

bool store_commit(void)
{
  return flash_write_all();
}

/* ── Ô lưu ván (US7) ─────────────────────────────────────────────────────────────── */
bool store_has_save(PlayMode m)
{
  return ((unsigned)m <= 1u) && (s_save[m].valid == 1u);
}

bool store_save_game(PlayMode m, const GameState *s)
{
  if ((unsigned)m > 1u) {
    return false;
  }
  memset(&s_save[m], 0, sizeof s_save[m]);   /* zero _rsv + mọi padding → crc xác định */
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
    return;                                   /* không có ô lưu → khỏi tốn 1 chu kỳ erase */
  }
  s_save[m].valid = 0u;
  s_save[m].crc = store_crc32(&s_save[m], (uint32_t)(sizeof(SavedGame) - sizeof(s_save[m].crc)));
  (void)flash_write_all();
}
