"""Raise a bdfconv-generated u8g2 font's ascent to its tallest letter.

usage: u8g2_set_ascent.py FONT.bdf FONT.c

u8g2's getFontAscent() returns header byte 13, which bdfconv fills with the
height of 'A'. In faces whose ascenders (b d l) rise above the caps, the
firmware then lays lines out too tight: the first line clips at the top of
the screen and ascenders meet the descenders of the line above. This sets
byte 13 to the tallest letter or digit in FONT.bdf (brackets and accented
capitals overshoot in Helvetica too, and counting them would loosen every
line for glyphs that rarely meet), and rewrites FONT.c in place.

Only the header byte changes; glyph data is untouched. Padding 'A' in the
BDF instead doesn't work: bdfconv trims blank rows before measuring.
"""
import re
import sys

bdf_path, c_path = sys.argv[1], sys.argv[2]

tops = {}
enc = None
for line in open(bdf_path):
    t = line.split()
    if not t:
        continue
    if t[0] == "ENCODING":
        enc = int(t[-1])
    elif t[0] == "BBX" and enc is not None:
        w, h, xo, yo = map(int, t[1:5])
        tops[enc] = h + yo
chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"
ascent = max(tops[ord(c)] for c in chars if ord(c) in tops)

def c_unescape(lit):
    # The escapes bdfconv and emit() below produce: octal, and backslash
    # before " \\ ?. (Python's unicode_escape doesn't know \?.)
    out = bytearray()
    i = 0
    while i < len(lit):
        if lit[i] != "\\":
            out.append(ord(lit[i])); i += 1
            continue
        mo = re.match(r"[0-7]{1,3}", lit[i + 1:])
        if mo:
            out.append(int(mo.group(), 8)); i += 1 + len(mo.group())
        else:
            out.append(ord(lit[i + 1])); i += 2
    return out


src = open(c_path, encoding="latin-1").read()
m = re.search(r'(=\s*\n?)((?:\s*"(?:[^"\\]|\\.)*")+)\s*;', src)
literal ="".join(re.findall(r'"((?:[^"\\]|\\.)*)"', m.group(2)))
data = c_unescape(literal)
old = data[13] - 256 if data[13] > 127 else data[13]
if ascent > old:
    data[13] = ascent


def emit(b):
    # Every non-printable as a 3-digit octal escape, so a following digit
    # can never be read as part of it; '?' escaped to dodge trigraphs.
    out, line = [], ""
    for v in b:
        ch = chr(v)
        if ch in '"\\?':
            s = "\\" + ch
        elif 32 <= v < 127:
            s = ch
        else:
            s = f"\\{v:03o}"
        if len(line) + len(s) > 100:
            out.append(line)
            line = ""
        line += s
    out.append(line)
    return "\n".join(f'  "{l}"' for l in out)


src = src[: m.start(2)] + emit(data) + src[m.end(2):]
open(c_path, "w", encoding="latin-1").write(src)
print(f"{c_path}: ascent {old} -> {max(old, ascent)}")
