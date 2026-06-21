#!/usr/bin/env python3
"""Tạo ảnh PNG cho từng bản đồ màn (ma trận chướng ngại 0/1) của LeafMuncher+.

Đọc THẲNG Core/Src/levels.c (nguồn chuẩn) — không cần thư viện ngoài (encoder PNG
viết bằng stdlib zlib). Xuất docs/ui/levels/levelN_<ten>.png + 1 ảnh tổng hợp.

Chạy:  py tools/gen_level_maps.py
"""

import os
import re
import sys
import zlib
import struct

# ── Hệ toạ độ game (khớp game.h) ───────────────────────────────────────────
COLS, ROWS = 20, 13
HEAD = (10, 6)            # đầu sâu khởi đầu (c, r)
BODY = [(9, 6), (8, 6)]   # thân
NEXT = (11, 6)            # ô tiến tới đầu tiên

CELL = 28                 # px mỗi ô
GAP = 1                   # đường lưới

# Bảng màu khớp render.c (RGB)
C_FIELD   = (11, 26, 11)
C_GRID    = (24, 42, 24)
C_OBST    = (95, 95, 110)
C_HEAD    = (230, 126, 0)
C_BODY    = (255, 180, 0)
C_NEXT    = (46, 204, 64)
C_EYE     = (20, 20, 20)
C_MARGIN  = (8, 14, 8)

NAMES = {0: "khoi-dong", 1: "hai-thanh", 2: "chu-thap", 3: "bon-goc", 4: "me-cung"}

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LEVELS_C = os.path.join(ROOT, "Core", "Src", "levels.c")
OUT_DIR = os.path.join(ROOT, "docs", "ui", "levels")


def parse_levels(src_path):
    """Trả về list (idx, target, step, bitmap[ROWS][COLS]) từ levels.c."""
    with open(src_path, "r", encoding="utf-8") as f:
        text = f.read()

    # target/step từ bảng LEVELS_TBL: { OBSn, target, step }
    meta = {}
    for m in re.finditer(r"\{\s*OBS(\d)\s*,\s*(\d+)\s*,\s*(\d+)\s*\}", text):
        meta[int(m.group(1))] = (int(m.group(2)), int(m.group(3)))

    levels = []
    for idx in range(5):
        pat = r"OBS%d\s*\[ROWS\]\[COLS\]\s*=\s*\{(.*?)\}\s*;" % idx
        block = re.search(pat, text, re.DOTALL)
        if not block:
            sys.exit("Không tìm thấy OBS%d trong %s" % (idx, src_path))
        rows = re.findall(r"\{([^{}]*)\}", block.group(1))
        if len(rows) != ROWS:
            sys.exit("OBS%d có %d hàng (cần %d)" % (idx, len(rows), ROWS))
        grid = []
        for r, row in enumerate(rows):
            vals = [int(x) for x in re.findall(r"\d+", row)]
            if len(vals) != COLS:
                sys.exit("OBS%d hàng %d có %d cột (cần %d)" % (idx, r, len(vals), COLS))
            grid.append(vals)
        target, step = meta.get(idx, (0, 0))
        levels.append((idx, target, step, grid))
    return levels


# ── Khung ảnh RGB đơn giản ──────────────────────────────────────────────────
class Image:
    def __init__(self, w, h, bg):
        self.w, self.h = w, h
        self.px = bytearray(bg * (w * h))

    def rect(self, x, y, w, h, color):
        c = bytes(color)
        for yy in range(max(0, y), min(self.h, y + h)):
            base = (yy * self.w + x) * 3
            for xx in range(w):
                if 0 <= x + xx < self.w:
                    o = base + xx * 3
                    self.px[o:o + 3] = c

    def write_png(self, path):
        raw = bytearray()
        stride = self.w * 3
        for y in range(self.h):
            raw.append(0)  # filter none
            raw.extend(self.px[y * stride:(y + 1) * stride])

        def chunk(typ, data):
            return (struct.pack(">I", len(data)) + typ + data
                    + struct.pack(">I", zlib.crc32(typ + data) & 0xffffffff))

        ihdr = struct.pack(">IIBBBBB", self.w, self.h, 8, 2, 0, 0, 0)
        with open(path, "wb") as f:
            f.write(b"\x89PNG\r\n\x1a\n")
            f.write(chunk(b"IHDR", ihdr))
            f.write(chunk(b"IDAT", zlib.compress(bytes(raw), 9)))
            f.write(chunk(b"IEND", b""))


def cell_at(grid, c, r):
    if (c, r) == HEAD:
        return "head"
    if (c, r) in BODY:
        return "body"
    if (c, r) == NEXT:
        return "next"
    return "obstacle" if grid[r][c] else "field"


def draw_grid(img, ox, oy, grid):
    """Vẽ lưới 20×13 vào img tại offset (ox,oy)."""
    for r in range(ROWS):
        for c in range(COLS):
            x, y = ox + c * CELL, oy + r * CELL
            img.rect(x, y, CELL, CELL, C_GRID)              # khe lưới
            t = cell_at(grid, c, r)
            inner = CELL - 2 * GAP
            if t == "field":
                img.rect(x + GAP, y + GAP, inner, inner, C_FIELD)
            elif t == "obstacle":
                img.rect(x + GAP, y + GAP, inner, inner, C_OBST)
            elif t == "head":
                img.rect(x + GAP, y + GAP, inner, inner, C_HEAD)
                img.rect(x + CELL - 9, y + 7, 4, 4, C_EYE)   # mắt
            elif t == "body":
                img.rect(x + GAP, y + GAP, inner, inner, C_BODY)
            elif t == "next":
                img.rect(x + GAP, y + GAP, inner, inner, C_FIELD)
                # viền xanh đánh dấu ô tiến đầu tiên
                t2 = 2
                img.rect(x + GAP, y + GAP, inner, t2, C_NEXT)
                img.rect(x + GAP, y + CELL - GAP - t2, inner, t2, C_NEXT)
                img.rect(x + GAP, y + GAP, t2, inner, C_NEXT)
                img.rect(x + CELL - GAP - t2, y + GAP, t2, inner, C_NEXT)


def main():
    levels = parse_levels(LEVELS_C)
    os.makedirs(OUT_DIR, exist_ok=True)

    pad = 12
    gw, gh = COLS * CELL, ROWS * CELL

    # Ảnh từng màn
    for idx, target, step, grid in levels:
        img = Image(gw + 2 * pad, gh + 2 * pad, C_MARGIN)
        draw_grid(img, pad, pad, grid)
        obs = sum(sum(row) for row in grid)
        name = NAMES.get(idx, "lv%d" % idx)
        path = os.path.join(OUT_DIR, "level%d_%s.png" % (idx, name))
        img.write_png(path)
        print("  level %d  %-10s target=%-2d step_ms=%-3d obstacles=%-3d -> %s"
              % (idx, name, target, step, obs, os.path.relpath(path, ROOT)))

    # Ảnh tổng hợp: 5 lưới xếp dọc, cách nhau dải mỏng
    sep = 10
    sheet = Image(gw + 2 * pad, (gh + sep) * len(levels) - sep + 2 * pad, C_MARGIN)
    for i, (idx, target, step, grid) in enumerate(levels):
        draw_grid(sheet, pad, pad + i * (gh + sep), grid)
    sheet_path = os.path.join(OUT_DIR, "all_levels.png")
    sheet.write_png(sheet_path)
    print("  tong hop                                                 -> %s"
          % os.path.relpath(sheet_path, ROOT))


if __name__ == "__main__":
    print("Tao anh ban do man choi tu", os.path.relpath(LEVELS_C, ROOT))
    main()
    print("Xong.")
