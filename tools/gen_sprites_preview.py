#!/usr/bin/env python3
"""Preview mỹ thuật sprite 16x16 cho LeafMuncher+ (lá/vàng/độc/power-up/chướng ngại/sâu).

Đây là CÔNG CỤ THIẾT KẾ: vẽ phóng to các sprite ASCII (nguồn chuẩn dưới đây) ra PNG
để duyệt bằng mắt TRƯỚC khi port sang C (Core/Src/sprites.c). Không cần thư viện ngoài
(encoder PNG bằng stdlib zlib, mượn từ gen_level_maps.py).

Chạy:  py tools/gen_sprites_preview.py   ->  docs/ui/sprites/preview.png
"""
import os, zlib, struct, math

SQRT2 = math.sqrt(2.0)

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT_DIR = os.path.join(ROOT, "docs", "ui", "sprites")

# ── nền theme (khớp theme.c) ─────────────────────────────────────────────────
BG_FOREST = (11, 26, 11)
BG_DESERT = (40, 30, 16)

# ── bảng màu dùng chung cho ASCII sprite ─────────────────────────────────────
PAL = {
    '.': None,                 # trong suốt
    # lá rừng
    'o': (18, 92, 34),         # viền/đậm lá
    'G': (46, 204, 64),        # xanh nền lá (= theme leaf_normal forest)
    'L': (130, 240, 130),      # xanh sáng (mặt hứng sáng)
    'V': (28, 134, 52),        # gân lá — xanh ĐẬM để nổi trên nửa lá sáng
    'm': (96, 64, 22),         # cuống (nâu)
    # cỏ lăn (tumbleweed)
    'T': (196, 162, 96),       # cát nền
    'H': (228, 202, 150),      # cát sáng
    'D': (118, 84, 40),        # nhánh khô đậm
    # vàng
    'y': (190, 150, 0),        # viền vàng đậm
    'Y': (255, 215, 0),        # vàng
    'W': (255, 246, 178),      # lóa sáng
    # độc (đầu lâu tím)
    'p': (92, 12, 112),        # viền/hốc tím đậm
    'P': (206, 138, 226),      # xương tím sáng
    'q': (168, 84, 198),       # xương tím trung
}

# ── SINH SPRITE 16x16 bằng thuật toán (tự căn giữa, đối xứng chuẩn) ──────────
# Hand-đếm pixel dễ lệch/không cân; sinh từ đĩa + đường chéo rồi in ASCII để port C.
N = 16

def blank():
    return [['.' for _ in range(N)] for _ in range(N)]

def to_rows(g):
    return ["".join(r) for r in g]

def indisc(x, y, cx, cy, rad):
    dx = x + 0.5 - cx; dy = y + 0.5 - cy
    return dx * dx + dy * dy <= rad * rad

def neigh_outside(g, x, y):
    for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
        nx, ny = x + dx, y + dy
        if nx < 0 or ny < 0 or nx >= N or ny >= N or g[ny][nx] == '.':
            return True
    return False

def gen_gold():
    """Đồng xu: đĩa vàng căn giữa, viền tối, lóa sáng trên-trái."""
    g = blank(); cx = cy = 7.5; R = 6.2
    for y in range(N):
        for x in range(N):
            if indisc(x, y, cx, cy, R):
                g[y][x] = 'Y'
    for y in range(N):
        for x in range(N):
            if g[y][x] == 'Y' and neigh_outside(g, x, y):
                g[y][x] = 'y'
    for y in range(N):                                   # lóa sáng (cung trên-trái)
        for x in range(N):
            if g[y][x] == 'Y' and indisc(x, y, 5.3, 5.3, 2.3):
                g[y][x] = 'W'
    return g

def gen_leaf():
    """Lá nghiêng PHÌN: phiến hình thấu kính (vesica) xoay 45° — ngọn trên-phải nhọn,
    gốc dưới-trái; gân giữa V, mặt sáng trên-trái (L), mặt khuất dưới-phải (G),
    viền sel-out (o), cuống nâu (m). Không phải kim cương, không phải lông vũ."""
    g = blank()
    a = 7.2          # nửa chiều dài (dọc gân)
    b = 4.7          # nửa bề rộng (phình) — to để ra phiến lá
    for y in range(N):
        for x in range(N):
            dx = x + 0.5 - 7.5; dy = y + 0.5 - 7.5
            u = (dx - dy) / SQRT2        # trục dài: ngọn trên-phải (+), gốc dưới-trái (-)
            v = (dx + dy) / SQRT2        # trục ngang phiến
            if abs(u) > a:
                continue
            halfw = b * (1.0 - (u / a) ** 2)   # vesica: thót dần về 2 đầu → ngọn nhọn
            if abs(v) <= halfw:
                g[y][x] = 'L' if v < 0 else 'G'   # sáng trên-trái / khuất dưới-phải
    for y in range(N):                   # viền sel-out quanh phiến
        for x in range(N):
            if g[y][x] in 'LG' and neigh_outside(g, x, y):
                g[y][x] = 'o'
    # GÂN vẽ thành đường MẢNH 1px lên trên phiến (không tô loang)
    def uv_to_cell(u, v):
        dx = (u + v) / SQRT2; dy = (v - u) / SQRT2
        return int(round(dx + 7.5)), int(round(dy + 7.5))
    u = -a + 0.5                          # gân giữa: chạy dọc trục lá tại v=0
    while u < a - 0.5:
        cx2, cy2 = uv_to_cell(u, 0.0)
        if 0 <= cx2 < N and 0 <= cy2 < N and g[cy2][cx2] in 'LG':
            g[cy2][cx2] = 'V'
        u += 0.5
    for u0 in (-2.6, 0.0, 2.6):           # gân phụ xương cá, mỗi gân là 1 đoạn mảnh
        for sgn in (-1, 1):
            t = 0.7
            while t < b - 0.4:
                cx2, cy2 = uv_to_cell(u0 + t * 0.7, sgn * t)
                if 0 <= cx2 < N and 0 <= cy2 < N and g[cy2][cx2] in 'LG':
                    g[cy2][cx2] = 'V'
                t += 0.9
    for (x, y) in [(3, 12), (2, 13), (1, 14)]:   # cuống nâu nối xuống gốc dưới-trái
        g[y][x] = 'm'
    return g

def gen_tumbleweed():
    """Cỏ LĂN khô (tumbleweed): KHÔNG đặc như bóng — búi nhánh khô đan hở, nhiều lỗ
    nhìn xuyên qua. Dựng từ nhiều đoạn nhánh thẳng nhiều góc trong vòng tròn, để trống xen kẽ."""
    g = blank(); cx = cy = 7.5; R = 7.0
    def plot(x, y, ch):
        if 0 <= x < N and 0 <= y < N and indisc(x, y, cx, cy, R):
            g[y][x] = ch
    # nhánh chéo "\" và "/" ở nhiều offset → tạo búi đan, chừa khoảng hở giữa các đường
    for k in (-7, -4, -1, 2, 5):
        for x in range(N):
            plot(x, x + k, 'D'); plot(x, -x + 11 + k, 'D')
    # vài nhánh ngắn ngang/dọc cho rối, màu cát nhạt
    for x in range(2, 14):
        if x % 3 == 0:
            plot(x, 7, 'T'); plot(x, 8, 'T')
    for y in range(2, 14):
        if y % 4 == 0:
            plot(7, y, 'T'); plot(8, y, 'T')
    # rìa vòng (annulus) mảnh để gợi khối cầu nhưng vẫn hở
    for y in range(N):
        for x in range(N):
            if indisc(x, y, cx, cy, R) and not indisc(x, y, cx, cy, R - 1.3) and g[y][x] == '.' and (x + y) % 2 == 0:
                g[y][x] = 'T'
    # lóa sáng vài điểm trên-trái
    for y in range(N):
        for x in range(N):
            if g[y][x] == 'D' and indisc(x, y, 5.0, 5.0, 3.0) and (x + y) % 3 == 0:
                g[y][x] = 'H'
    return g

# Đầu lâu tím — hardcode ĐỐI XỨNG (palindrome quanh cột 7.5), căn giữa ô.
POISON = [
    "................",
    ".....pppppp.....",
    "....pPPPPPPp....",
    "...pPPPPPPPPp...",
    "...pPPPPPPPPp...",
    "...pPppPPppPp...",
    "...pPppPPppPp...",
    "...pPPPPPPPPp...",
    "....pPPppPPp....",
    ".....pPPPPp.....",
    ".....pPppPp.....",
    ".....pPppPp.....",
    "......pppp......",
    "................",
    "................",
    "................",
]

LEAF_FOREST = to_rows(gen_leaf())
TUMBLEWEED  = to_rows(gen_tumbleweed())
GOLD        = to_rows(gen_gold())

# In ASCII để port nguyên si sang Core/Src/sprites.c
for _nm, _rows in [("LEAF_FOREST", LEAF_FOREST), ("TUMBLEWEED", TUMBLEWEED),
                   ("GOLD", GOLD), ("POISON", POISON)]:
    print("\n%s:" % _nm)
    for _r in _rows:
        print('    "%s",' % _r)

SPRITES = [
    ("leaf_forest", LEAF_FOREST, [BG_FOREST]),
    ("tumbleweed",  TUMBLEWEED,  [BG_DESERT]),
    ("gold",        GOLD,        [BG_FOREST, BG_DESERT]),
    ("poison",      POISON,      [BG_FOREST, BG_DESERT]),
]

# ── khung ảnh RGB + PNG (mượn gen_level_maps.py) ─────────────────────────────
class Image:
    def __init__(self, w, h, bg):
        self.w, self.h = w, h
        self.px = bytearray(bytes(bg) * (w * h))
    def rect(self, x, y, w, h, color):
        c = bytes(color)
        for yy in range(max(0, y), min(self.h, y + h)):
            base = yy * self.w * 3
            for xx in range(max(0, x), min(self.w, x + w)):
                o = base + xx * 3
                self.px[o:o+3] = c
    def write_png(self, path):
        raw = bytearray(); stride = self.w * 3
        for y in range(self.h):
            raw.append(0); raw.extend(self.px[y*stride:(y+1)*stride])
        def chunk(typ, data):
            return (struct.pack(">I", len(data)) + typ + data
                    + struct.pack(">I", zlib.crc32(typ + data) & 0xffffffff))
        ihdr = struct.pack(">IIBBBBB", self.w, self.h, 8, 2, 0, 0, 0)
        with open(path, "wb") as f:
            f.write(b"\x89PNG\r\n\x1a\n")
            f.write(chunk(b"IHDR", ihdr))
            f.write(chunk(b"IDAT", zlib.compress(bytes(raw), 9)))
            f.write(chunk(b"IEND", b""))


def validate(name, rows):
    if len(rows) != 16:
        raise SystemExit("%s: %d hang (can 16)" % (name, len(rows)))
    for i, r in enumerate(rows):
        if len(r) != 16:
            raise SystemExit("%s hang %d: %d cot (can 16)" % (name, i, len(r)))
        for ch in r:
            if ch not in PAL:
                raise SystemExit("%s hang %d: ky tu la '%s'" % (name, i, ch))


def shade(c, f):
    return tuple(max(0, min(255, int(v * f))) for v in c)


def draw_sprite_cell(img, ox, oy, rows, bg, z):
    """Vẽ 1 ô 16x16 phóng z lần trên nền bg tại (ox,oy)."""
    img.rect(ox, oy, 16 * z, 16 * z, bg)
    for r in range(16):
        for c in range(16):
            col = PAL[rows[r][c]]
            if col is not None:
                img.rect(ox + c * z, oy + r * z, z, z, col)


def draw_obstacle_cell(img, ox, oy, base, z):
    """Khối đá vát 3D suy từ màu base (xem trước cho render C)."""
    img.rect(ox, oy, 16 * z, 16 * z, base)
    hi = shade(base, 1.35); lo = shade(base, 0.6); dk = shade(base, 0.45)
    img.rect(ox, oy, 16 * z, 2 * z, hi)                 # mép trên sáng
    img.rect(ox, oy, 2 * z, 16 * z, hi)                 # mép trái sáng
    img.rect(ox, oy + 14 * z, 16 * z, 2 * z, lo)        # mép dưới tối
    img.rect(ox + 14 * z, oy, 2 * z, 16 * z, lo)        # mép phải tối
    # vết nứt gãy chéo (tránh giống chữ số) + vài đốm sạn lệch
    for (sx, sy) in [(10, 3), (10, 4), (9, 5), (9, 6), (8, 7), (8, 8)]:
        img.rect(ox + sx * z, oy + sy * z, z, z, dk)
    for (sx, sy) in [(4, 5), (6, 11), (12, 9), (5, 9)]:
        img.rect(ox + sx * z, oy + sy * z, z, z, dk)


def draw_worm(img, ox, oy, z, head_col, body_col, n=6):
    """Sâu nằm ngang: đầu trái + n đốt, có nối/bo/đổ bóng (xem trước cho render C)."""
    def seg(cx, is_head, dirx=1):
        x = ox + cx * 16 * z; y = oy
        col = head_col if is_head else body_col
        hi = shade(col, 1.3); lo = shade(col, 0.62)
        # thân bo góc: vẽ ô thụt 1px rồi bo 4 góc (xoá góc về nền? — preview nền đồng nhất)
        img.rect(x + 1*z, y + 1*z, 14*z, 14*z, col)
        # bo góc: cắt 4 pixel góc
        for (gx, gy) in [(1,1),(14,1),(1,14),(14,14)]:
            img.rect(x + gx*z, y + gy*z, z, z, BG_FOREST)
        # nối sang đốt phải (cầu nối) để đọc liền mạch
        if cx < n:
            img.rect(x + 14*z, y + 5*z, 3*z, 6*z, col)
        # highlight trên, bóng dưới
        img.rect(x + 3*z, y + 2*z, 9*z, 2*z, hi)
        img.rect(x + 3*z, y + 12*z, 9*z, 2*z, lo)
        if is_head:
            # mắt (trắng + đồng tử) + lỗ mũi về phía dirx
            white=(245,245,245); pup=(15,15,15)
            img.rect(x + 9*z, y + 3*z, 3*z, 3*z, white)
            img.rect(x + 9*z, y + 10*z, 3*z, 3*z, white)
            img.rect(x +10*z, y + 4*z, z, z, pup)
            img.rect(x +10*z, y + 11*z, z, z, pup)
    seg(0, True)
    for i in range(1, n + 1):
        seg(i, False)


def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    for name, rows, _ in SPRITES:
        validate(name, rows)

    z = 14          # px mỗi pixel-sprite
    pad = 16
    cell = 16 * z
    label_h = 22

    # layout: hàng item (mỗi item x số nền) + hàng obstacle + hàng worm
    cols = 6
    item_cells = []
    for name, rows, bgs in SPRITES:
        for bg in bgs:
            item_cells.append(("sprite", name, rows, bg))
    item_cells.append(("obst", "obstacle FOREST", BG_FOREST, (95, 95, 110)))
    item_cells.append(("obst", "obstacle DESERT", BG_DESERT, (150, 110, 70)))

    rows_n = (len(item_cells) + cols - 1) // cols
    worm_h = cell + label_h + pad
    W = pad + cols * (cell + pad)
    H = pad + rows_n * (cell + label_h + pad) + worm_h + pad
    img = Image(W, H, (18, 18, 22))

    for idx, item in enumerate(item_cells):
        cx = pad + (idx % cols) * (cell + pad)
        cy = pad + (idx // cols) * (cell + label_h + pad)
        if item[0] == "sprite":
            _, name, rows, bg = item
            draw_sprite_cell(img, cx, cy, rows, bg, z)
        else:
            _, name, bg, base = item
            img.rect(cx, cy, cell, cell, bg)
            draw_obstacle_cell(img, cx, cy, base, z)

    # hàng sâu
    wy = pad + rows_n * (cell + label_h + pad)
    img.rect(pad, wy, W - 2 * pad, cell, BG_FOREST)
    draw_worm(img, pad, wy, z, (230, 126, 0), (255, 180, 0), n=5)

    path = os.path.join(OUT_DIR, "preview.png")
    img.write_png(path)
    print("OK ->", os.path.relpath(path, ROOT))


if __name__ == "__main__":
    main()
