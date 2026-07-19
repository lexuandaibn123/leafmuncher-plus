#ifndef STORE_H
#define STORE_H

/* Lưu trữ và khôi phục cài đặt và trạng thái lưu trò chơi từ Flash nội của STM32.
 * Backing: sector 4 single-bank @ 0x08010000 (64 KB). */

#include <stdint.h>
#include <stdbool.h>
#include "game.h"

#define STORE_MAGIC    0x4C4D2B01u
#define STORE_VERSION  1u
#define SAVE_VERSION   2u

/* Record cài đặt bền vững. Layout cố định 16 byte, chia hết 4 → ghi Flash theo word.
 * `crc` phủ mọi field phía trước. */
typedef struct {
  uint32_t magic;
  uint16_t version;
  uint16_t _rsv;
  uint32_t endless_high;
  uint32_t crc;
} PersistData;

/* Ô lưu ván - snapshot để TIẾP TỤC ván sau khi tắt nguồn. Mỗi chế độ chơi 1 ô.
 * `crc` phủ mọi field phía trước. `GameState` là POD → chép thẳng byte. */
typedef struct {
  uint8_t   valid;
  uint8_t   _rsv;
  uint16_t  version;
  GameState state;
  uint32_t  crc;
} SavedGame;

/* ── Codec mã hóa ─────────────────────────────────────────────────────────── */
uint32_t store_crc32(const void *data, uint32_t len);
bool     store_pd_valid(const PersistData *pd);
void     store_pd_defaults(PersistData *pd);
bool     store_sg_valid(const SavedGame *sg);

/* ── API lưu cài đặt ──────────────────────────────────────────────────────── */
void               store_init(void);
const PersistData *store_get(void);
void               store_set_endless_high(uint32_t s);
bool               store_commit(void);

/* ── Ô lưu ván ────────────────────────────────────────────────────────────── */
bool store_has_save(PlayMode m);
bool store_save_game(PlayMode m, const GameState *s);
bool store_load_game(PlayMode m, GameState *out);
void store_clear_save(PlayMode m);

#endif /* STORE_H */
