#include "src/pure/xpointer.h"

// ----------------------------------------------------------------------------
//  Parsing
// ----------------------------------------------------------------------------
static char lowerAscii(char c) {
  return (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
}

static bool allDigits(const String& s) {
  if (s.length() == 0) return false;
  for (unsigned i = 0; i < s.length(); i++) {
    if (s[i] < '0' || s[i] > '9') return false;
  }
  return true;
}

// `p[3]` -> name "p", ordinal 3. A missing index is crengine's shorthand for
// the first (and usually only) same-named sibling.
static bool parseStep(const String& seg, XPointerStep& out) {
  if (seg.length() == 0) return false;

  int br = seg.indexOf('[');
  String name = (br < 0) ? seg : seg.substring(0, (unsigned)br);
  if (name.length() == 0) return false;

  uint16_t ordinal = 1;
  if (br >= 0) {
    int close = seg.indexOf(']', br + 1);
    if (close < 0) return false;
    String num = seg.substring((unsigned)br + 1, (unsigned)close);
    if (!allDigits(num)) return false;
    long v = 0;
    for (unsigned i = 0; i < num.length(); i++) v = v * 10 + (num[i] - '0');
    if (v <= 0 || v > 65535) return false;
    ordinal = (uint16_t)v;
  }

  unsigned n = name.length();
  if (n >= sizeof(out.name)) n = sizeof(out.name) - 1;
  for (unsigned i = 0; i < n; i++) out.name[i] = lowerAscii(name[i]);
  out.name[n] = '\0';
  out.ordinal = ordinal;
  return true;
}

bool parseXPointer(const String& s, XPointer& out) {
  out = XPointer();

  String in = s;
  in.trim();
  // Every crengine pointer is absolute. Anything else — most importantly the
  // bare percentage string this firmware used to put in `progress`, and what
  // KOReader sends for paged documents — is not a pointer at all.
  if (in.length() < 2 || in[0] != '/') return false;

  XPointerStep steps[XPOINTER_MAX_STEPS];
  uint8_t count = 0;
  int fragmentAt = -1;
  uint16_t fragmentOrdinal = 0;
  uint32_t textOffset = 0;

  unsigned i = 1;   // skip the leading '/'
  while (i <= in.length()) {
    unsigned start = i;
    while (i < in.length() && in[i] != '/') i++;
    String seg = in.substring(start, i);
    i++;   // step past the '/'

    if (seg.length() == 0) continue;

    // A trailing `.N` is the character offset into the addressed node. It
    // rides on the last step (`p[3].0`, `text().15`).
    int dot = seg.lastIndexOf('.');
    if (dot > 0) {
      String tail = seg.substring((unsigned)dot + 1);
      if (allDigits(tail)) {
        long v = 0;
        for (unsigned k = 0; k < tail.length() && v < 100000000L; k++) {
          v = v * 10 + (tail[k] - '0');
        }
        textOffset = (uint32_t)v;
        seg = seg.substring(0, (unsigned)dot);
      }
    }

    // `text()` addresses the text node of the element before it; for our
    // purposes that is the same place as the element.
    if (seg == "text()" || seg.length() == 0) continue;

    if (count >= XPOINTER_MAX_STEPS) break;
    if (!parseStep(seg, steps[count])) return false;

    if (fragmentAt < 0) {
      String nm = String(steps[count].name);
      if (nm == "docfragment") {
        fragmentAt = count;
        fragmentOrdinal = steps[count].ordinal;
      }
    }
    count++;
  }

  if (count == 0) return false;

  // Keep only the path inside the addressed spine document. Everything up to
  // and including DocFragment[N] is crengine's own scaffolding.
  uint8_t first = 0;
  if (fragmentAt >= 0) {
    out.hasFragment = true;
    out.fragment    = fragmentOrdinal;
    first           = (uint8_t)(fragmentAt + 1);
  }

  out.stepCount = 0;
  for (uint8_t k = first; k < count; k++) {
    out.steps[out.stepCount++] = steps[k];
  }
  out.textOffset = textOffset;

  // A pointer that is nothing but `DocFragment[N]` still names a place — the
  // start of that document — so it stays valid with zero steps.
  out.valid = out.hasFragment || out.stepCount > 0;
  return out.valid;
}

// ----------------------------------------------------------------------------
//  Composing
// ----------------------------------------------------------------------------
String canonicalPath(const XPointer& xp, uint8_t count) {
  if (count > xp.stepCount) count = xp.stepCount;
  String out;
  out.reserve((size_t)count * 12);
  for (uint8_t i = 0; i < count; i++) {
    out += "/";
    out += xp.steps[i].name;
    out += "[";
    out += String((unsigned long)xp.steps[i].ordinal);
    out += "]";
  }
  return out;
}

String buildXPointer(uint16_t fragment, const String& parentPath,
                     const String& leafName, uint16_t leafOrdinal,
                     uint32_t textOffset) {
  String out;
  out.reserve(parentPath.length() + leafName.length() + 48);

  if (fragment > 0) {
    out += "/body/DocFragment[";
    out += String((unsigned long)fragment);
    out += "]";
  }
  out += parentPath;
  if (leafName.length() > 0) {
    out += "/";
    out += leafName;
    out += "[";
    out += String((unsigned long)leafOrdinal);
    out += "]";
  }
  out += ".";
  out += String((unsigned long)textOffset);
  return out;
}
