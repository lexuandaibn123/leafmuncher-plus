/* test_theme — unit-test module theme thuần (gcc). US6 / T070.
 * Theme là cosmetic nhưng vẫn thuần (const, không HAL) → host-test được. */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "theme.h"

int main(void)
{
  /* theme_count = THEME_COUNT, ≥ 2 (FOREST + DESERT). */
  assert(theme_count() == THEME_COUNT);
  assert(theme_count() >= 2);

  /* Mỗi theme hợp lệ: non-NULL, có tên. */
  for (int i = 0; i < THEME_COUNT; i++) {
    const Theme *t = theme_get((ThemeId)i);
    assert(t != 0);
    assert(t->name != 0 && t->name[0] != 0);
  }

  /* theme_next cuộn vòng. */
  assert(theme_next(THEME_FOREST) == THEME_DESERT);
  assert(theme_next(THEME_DESERT) == THEME_FOREST);   /* quay vòng về đầu */

  /* OOB → mặc định FOREST (không tràn). */
  assert(theme_get((ThemeId)999) == theme_get(THEME_FOREST));
  assert(theme_get((ThemeId)(-1)) == theme_get(THEME_FOREST));

  /* 2 theme khác nhau ở ít nhất nền sân (đổi theme phải thấy được). */
  const Theme *f = theme_get(THEME_FOREST);
  const Theme *d = theme_get(THEME_DESERT);
  assert(f->bg != d->bg);
  assert(f->obstacle != d->obstacle);

  printf("test_theme: all assertions passed (T070 theme: count/next/oob/distinct)\n");
  return 0;
}
