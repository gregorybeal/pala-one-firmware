"""Rasterize a (variable) TTF into a 1-bit BDF for u8g2's bdfconv.

Used by scripts/gen_bitter_fonts.sh to build Pala_One_2_1/bitter_u8g2_fonts.h.
Not part of the firmware build — run it by hand when adding or regenerating a
font family.

usage: ttf2bdf.py FONT.ttf OUT.bdf --px 14.75 [--wght 400] [--hint auto|native]
                  [--track 0] [--space N] [--embolden]

Lessons from getting this to look right on a 1-bit ~120 ppi panel — judge
any change by rendering a full page beside Helvetica, not single glyphs:

--px is the pixel size (em). Pick it by x-height, not cap height: x-height
is what reads as "size", and Bitter's is large for its caps. Prefer a size
where ascenders don't rise above the caps (see the ascent note below).

--hint auto (default) uses FreeType's autohinter. Bitter's own TrueType
hinting, applied to the variable font in mono, left blobs (a 2px block on
'b'), a malformed 8px 'I' and cramped, uneven spacing.

--track adds px to every advance; --min-gap instead widens only glyphs
whose ink would otherwise touch a neighbour, which tightens loose pairs
less than a uniform track. --space sets the word-space advance outright. Bitter's narrow space rounds to 2-3px at these sizes, barely
wider than the gaps between letters, so words run together without it.

--embolden ORs each glyph with itself shifted 1px right: a synthetic bold
for sizes where the real bold weight breaks up or hints down to 1px stems.
(Bitter no longer needs it: at the sizes gen_bitter_fonts.sh picks, 700
hints to clean stems one px wider than 400.)

Ascent: where ascenders rise above the caps, u8g2's getFontAscent() (the
height of 'A') under-reports; scripts/u8g2_set_ascent.py fixes that after
bdfconv. The "ascent" printed below is the value it will set.
"""
import argparse
import freetype

ap = argparse.ArgumentParser()
ap.add_argument("font"); ap.add_argument("out")
ap.add_argument("--px", type=float, required=True)
ap.add_argument("--wght", type=float, default=400)
ap.add_argument("--hint", choices=["auto", "native"], default="auto")
ap.add_argument("--track", type=int, default=0)
ap.add_argument("--min-gap", type=int, default=0)
ap.add_argument("--space", type=int)
ap.add_argument("--embolden", action="store_true")
ap.add_argument("--name", default="Bitter")
a = ap.parse_args()

face = freetype.Face(a.font)
try:
    axes = face.get_variation_info().axes
    face.set_var_design_coords([a.wght if ax.tag == "wght" else ax.default for ax in axes])
except Exception:
    pass  # static font
px64 = round(a.px * 64)
face.set_char_size(0, px64, 72, 72)

FLAGS = freetype.FT_LOAD_RENDER | freetype.FT_LOAD_TARGET_MONO
if a.hint == "auto":
    FLAGS |= freetype.FT_LOAD_FORCE_AUTOHINT


def render(cp):
    face.load_char(chr(cp), FLAGS)
    g = face.glyph
    bm = g.bitmap
    w, h = bm.width, bm.rows
    rows = [[(bm.buffer[y * bm.pitch + (x >> 3)] >> (7 - (x & 7))) & 1 for x in range(w)]
            for y in range(h)]
    left, top = g.bitmap_left, g.bitmap_top
    adv = (g.advance.x + 32) >> 6
    if a.embolden and w:
        rows = [[r[x] | (r[x - 1] if x else 0) for x in range(w)] + [r[-1]] for r in rows]
        w += 1
        adv += 1
    # Trim empty rows/cols so the BBX is tight (bdfconv likes that).
    while rows and not any(rows[0]):
        rows.pop(0); top -= 1
    while rows and not any(rows[-1]):
        rows.pop()
    while rows and rows[0] and not any(r[0] for r in rows):
        rows = [r[1:] for r in rows]; left += 1
    while rows and rows[0] and not any(r[-1] for r in rows):
        rows = [r[:-1] for r in rows]
    if not rows or not rows[0]:
        return dict(cp=cp, w=0, h=0, xo=0, yo=0, adv=adv, rows=[])
    adv += a.track
    if a.min_gap:
        # No ink left of the origin, and at least min_gap blank px on the
        # right, so neighbours never touch; only tight glyphs move.
        if left < 0:
            adv -= left; left = 0
        rsb = adv - (left + len(rows[0]))
        if rsb < a.min_gap:
            adv += a.min_gap - rsb
    if cp == 32 and a.space is not None:
        adv = a.space
    return dict(cp=cp, w=len(rows[0]), h=len(rows), xo=left, yo=top - len(rows), adv=adv, rows=rows)


glyphs = []
for cp in list(range(32, 127)) + list(range(160, 256)):
    if cp != 32 and face.get_char_index(cp) == 0:
        continue
    g = render(cp)
    if cp == 32 and a.space is not None:
        g["adv"] = a.space
    glyphs.append(g)
by_cp = {g["cp"]: g for g in glyphs}

top = lambda g: g["yo"] + g["h"]
tallest = max(top(by_cp[ord(c)]) for c in
              "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789")

asc = max(top(g) for g in glyphs)
desc = -min(g["yo"] for g in glyphs)
with open(a.out, "w") as f:
    weight = "Bold" if a.wght >= 600 or a.embolden else "Regular"
    f.write("STARTFONT 2.1\n")
    f.write(f"FONT -misc-{a.name}-{weight}-R-Normal--{round(a.px)}-{round(a.px * 10)}-72-72-P-0-ISO10646-1\n")
    f.write(f"SIZE {round(a.px)} 72 72\n")
    f.write(f"FONTBOUNDINGBOX {max(g['w'] for g in glyphs)} {asc + desc} 0 {-desc}\n")
    f.write(f"STARTPROPERTIES 3\nFONT_ASCENT {asc}\nFONT_DESCENT {desc}\n"
            f"COPYRIGHT \"Bitter, SIL Open Font License 1.1\"\nENDPROPERTIES\n")
    f.write(f"CHARS {len(glyphs)}\n")
    for g in glyphs:
        f.write(f"STARTCHAR uni{g['cp']:04X}\nENCODING {g['cp']}\n"
                f"SWIDTH {g['adv'] * 1000 // max(1, round(a.px))} 0\nDWIDTH {g['adv']} 0\n")
        f.write(f"BBX {g['w']} {g['h']} {g['xo']} {g['yo']}\nBITMAP\n")
        nbytes = (g["w"] + 7) // 8
        for r in g["rows"]:
            v = 0
            for x in range(nbytes * 8):
                v = (v << 1) | (r[x] if x < g["w"] else 0)
            f.write(f"{v:0{nbytes * 2}X}\n")
        f.write("ENDCHAR\n")
    f.write("ENDFONT\n")

x = by_cp[ord("x")]
print(f"{a.out}: px={a.px} x-height={top(x)} cap={top(by_cp[ord('H')])} "
      f"ascent={tallest} desc={desc} space={by_cp[32]['adv']}")
