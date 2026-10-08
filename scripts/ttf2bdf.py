"""Rasterize a (variable) TTF into a 1-bit BDF for u8g2's bdfconv.

Used to generate Pala_One_2_1/bitter_u8g2_fonts.h. Not part of the firmware
build — run it by hand when adding or regenerating a font family.

usage: ttf2bdf.py FONT.ttf OUT.bdf --cap 11 [--wght 400] [--mode mono|thresh]
                  [--thresh 110] [--embolden]

--cap picks the pixel size so that capital 'H' is exactly that many px tall.
Match it to u8g2 Helvetica's cap height for the body size (8/10/12/14 ->
8/11/12/14 px) so switching family doesn't change lines-per-page much.

--mode mono (default) renders with the font's own TrueType hinting, which
gave noticeably cleaner stems than thresholding antialiased output.
--embolden ORs each glyph with itself shifted 1px right — a synthetic bold
for sizes where the real bold weight breaks up (Bitter b08).

Recipe for Bitter (needs uv, plus bdfconv built from u8g2's
tools/font/bdfconv — plain `make`, no dependencies):

  curl -LO https://github.com/google/fonts/raw/main/ofl/bitter/Bitter%5Bwght%5D.ttf
  for pair in 08:8 10:11 12:12 14:14; do
    sz=${pair%%:*}; cap=${pair##*:}
    for w in r:400 b:700; do
      s=${w%%:*}; wt=${w##*:}; extra=
      # b08: synthetic bold from the regular weight (see above).
      [ "$s$sz" = b08 ] && { wt=400; extra=--embolden; }
      uv run --with freetype-py python scripts/ttf2bdf.py "Bitter[wght].ttf" \
        $s$sz.bdf --cap $cap --wght $wt $extra
      bdfconv -f 1 -m '32-126,160-255' -n u8g2_font_bitter_${s}${sz}_te \
        -o $s$sz.c $s$sz.bdf
    done
  done

then concatenate r08 b08 r10 b10 ... b14 .c files under the existing header
comment in bitter_u8g2_fonts.h.
"""
import argparse
import freetype

ap = argparse.ArgumentParser()
ap.add_argument("font"); ap.add_argument("out")
ap.add_argument("--wght", type=float, default=400)
ap.add_argument("--cap", type=int, required=True)
ap.add_argument("--mode", default="mono")
ap.add_argument("--thresh", type=int, default=110)
ap.add_argument("--name", default="Bitter")
ap.add_argument("--embolden", action="store_true", help="synthetic bold: OR each glyph with itself shifted 1px right")
a = ap.parse_args()

face = freetype.Face(a.font)
try:
    axes = face.get_variation_info().axes
    coords = [a.wght if ax.tag == "wght" else ax.default for ax in axes]
    face.set_var_design_coords(coords)
except Exception:
    pass

if a.mode == "mono":
    FLAGS = freetype.FT_LOAD_RENDER | freetype.FT_LOAD_TARGET_MONO
else:
    FLAGS = freetype.FT_LOAD_RENDER | freetype.FT_LOAD_TARGET_LIGHT | freetype.FT_LOAD_FORCE_AUTOHINT


def render(cp):
    face.load_char(chr(cp), FLAGS)
    g = face.glyph
    bm = g.bitmap
    w, h = bm.width, bm.rows
    rows = []
    for y in range(h):
        row = []
        for x in range(w):
            if a.mode == "mono":
                byte = bm.buffer[y * bm.pitch + (x >> 3)]
                row.append((byte >> (7 - (x & 7))) & 1)
            else:
                row.append(1 if bm.buffer[y * bm.pitch + x] >= a.thresh else 0)
        rows.append(row)
    adv = (g.advance.x + 32) >> 6
    if a.embolden and w:
        rows = [[r[x] | (r[x - 1] if x else 0) for x in range(w)] + [r[-1]] for r in rows]
        w += 1
        adv += 1
    return w, h, g.bitmap_left, g.bitmap_top, adv, rows


# Find the pixel size whose rendered 'H' height equals --cap.
px = None
for size64 in range(6 * 64, 40 * 64, 8):
    face.set_char_size(0, size64, 72, 72)
    _, h, _, _, _, _ = render(ord("H"))
    if h >= a.cap:
        px = size64
        break
face.set_char_size(0, px, 72, 72)

glyphs = []
for cp in list(range(32, 127)) + list(range(160, 256)):
    if face.get_char_index(cp) == 0 and cp != 32:
        continue
    w, h, left, top, adv, rows = render(cp)
    # Trim empty rows/cols so the BBX is tight (bdfconv likes that).
    while rows and not any(rows[0]):
        rows.pop(0); top -= 1; h -= 1
    while rows and not any(rows[-1]):
        rows.pop(); h -= 1
    if rows:
        while w and not any(r[0] for r in rows):
            rows = [r[1:] for r in rows]; left += 1; w -= 1
        while w and not any(r[-1] for r in rows):
            rows = [r[:-1] for r in rows]; w -= 1
    if not rows or w == 0:
        w = h = 0; rows = []; left = 0; top = 0
    glyphs.append((cp, w, h, left, top - h, adv, rows))

asc = max(g[4] + g[2] for g in glyphs)
desc = -min(g[4] for g in glyphs)
with open(a.out, "w") as f:
    weight = "Bold" if a.wght >= 600 else "Regular"
    f.write("STARTFONT 2.1\n")
    f.write(f"FONT -misc-{a.name}-{weight}-R-Normal--{a.cap}-{a.cap*10}-72-72-P-0-ISO10646-1\n")
    f.write(f"SIZE {px // 64} 72 72\n")
    f.write(f"FONTBOUNDINGBOX {max(g[1] for g in glyphs)} {asc + desc} 0 {-desc}\n")
    f.write(f"STARTPROPERTIES 3\nFONT_ASCENT {asc}\nFONT_DESCENT {desc}\nCOPYRIGHT \"Bitter, SIL Open Font License 1.1\"\nENDPROPERTIES\n")
    f.write(f"CHARS {len(glyphs)}\n")
    for cp, w, h, xo, yo, adv, rows in glyphs:
        f.write(f"STARTCHAR uni{cp:04X}\nENCODING {cp}\nSWIDTH {adv * 1000 // max(1, px // 64)} 0\nDWIDTH {adv} 0\n")
        f.write(f"BBX {w} {h} {xo} {yo}\nBITMAP\n")
        nbytes = (w + 7) // 8
        for r in rows:
            v = 0
            for x in range(nbytes * 8):
                v = (v << 1) | (r[x] if x < w else 0)
            f.write(f"{v:0{nbytes * 2}X}\n")
        f.write("ENDCHAR\n")
    f.write("ENDFONT\n")
print(f"{a.out}: px={px / 64:.2f} cap={a.cap} asc={asc} desc={desc} glyphs={len(glyphs)}")
