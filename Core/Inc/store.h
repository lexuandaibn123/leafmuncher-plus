#ifndef STORE_H
#define STORE_H

/* store — lưu cài đặt bền vững qua tắt nguồn (Flash nội). Hợp đồng: contracts/store.md.
 *
 * CHỈ store.c chạm Flash HAL; logic game (game.c) KHÔNG gọi store (Nguyên tắc II/III) — tasks/menu
 * đọc-ghi store rồi truyền giá trị vào render/logic. Đợt A: chỉ cài đặt (theme + điểm cao Vô tận);
 * ô lưu ván (save/resume, US7) để Đợt B.
 *
 * Backing: sector 4 single-bank @ 0x08010000 (64 KB) — xem store.c + STM32F429XX_FLASH.ld.
 * Phần codec (CRC32/validate/defaults) là THUẦN (store_codec.c) → host-test được (test_store.c). */

#include <stdint.h>
#include <stdbool.h>
#include "game.h"   /* ThemeId { THEME_FOREST, THEME_DESERT, THEME_COUNT } */

#define STORE_MAGIC    0x4C4D2B01u   /* 'LM+\1' — nhận diện record hợp lệ */
#define STORE_VERSION  1u            /* version schema (mở rộng field ở cuối → tăng số này) */

/* Record cài đặt bền vững (data-model §2.7). Layout cố định 16 byte, chia hết 4 → ghi Flash theo word.
 * `crc` phủ mọi field phía trước (magic..endless_high). */
typedef struct {
  uint32_t magic;          /* = STORE_MAGIC */
  uint16_t version;        /* = STORE_VERSION */
  uint16_t theme_id;       /* ThemeId đã chọn */
  uint32_t endless_high;   /* điểm cao chế độ Vô tận */
  uint32_t crc;            /* CRC32 của 12 byte đầu */
} PersistData;

/* ── Codec THUẦN (store_codec.c) — không chạm HAL, host-test được ─────────────────── */
uint32_t store_crc32(const void *data, uint32_t len);   /* CRC32 (poly phản chiếu 0xEDB88320) */
bool     store_pd_valid(const PersistData *pd);          /* magic + version + crc đúng? */
void     store_pd_defaults(PersistData *pd);             /* nạp mặc định (theme=FOREST, high=0) + crc */

/* ── API store (store.c — chạm Flash) ────────────────────────────────────────────── */
void               store_init(void);                     /* đọc Flash→cache; hỏng/trống → mặc định */
const PersistData *store_get(void);                      /* cache hiện hành (không chạm Flash) */
void               store_set_theme(ThemeId id);          /* cập nhật cache (chưa ghi) */
void               store_set_endless_high(uint32_t s);   /* cập nhật cache nếu s > giá trị cũ */
bool               store_commit(void);                   /* ghi cache xuống Flash; false nếu lỗi */

#endif /* STORE_H */
