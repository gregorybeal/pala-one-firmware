#include "kosync_codec.h"

#include <stdlib.h>   // strtod

// ----------------------------------------------------------------------------
//  Document id
// ----------------------------------------------------------------------------
static char hexDigit(uint8_t nibble) {
  return (char)((nibble < 10) ? ('0' + nibble) : ('a' + (nibble - 10)));
}

static int hexValue(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

String kosyncDocToHex(const uint8_t doc[KOSYNC_DOC_BYTES]) {
  String out;
  out.reserve(KOSYNC_DOC_HEX);
  for (size_t i = 0; i < KOSYNC_DOC_BYTES; i++) {
    out += hexDigit((uint8_t)(doc[i] >> 4));
    out += hexDigit((uint8_t)(doc[i] & 0x0F));
  }
  return out;
}

bool kosyncDocFromHex(const String& hex, uint8_t out[KOSYNC_DOC_BYTES]) {
  if (hex.length() != KOSYNC_DOC_HEX) return false;

  // Validate fully before writing, so a rejected string never leaves the
  // caller's buffer half-updated.
  uint8_t tmp[KOSYNC_DOC_BYTES];
  for (size_t i = 0; i < KOSYNC_DOC_BYTES; i++) {
    int hi = hexValue(hex[(unsigned)(i * 2)]);
    int lo = hexValue(hex[(unsigned)(i * 2 + 1)]);
    if (hi < 0 || lo < 0) return false;
    tmp[i] = (uint8_t)((hi << 4) | lo);
  }
  for (size_t i = 0; i < KOSYNC_DOC_BYTES; i++) out[i] = tmp[i];
  return true;
}

bool kosyncDocIsSet(const uint8_t doc[KOSYNC_DOC_BYTES]) {
  for (size_t i = 0; i < KOSYNC_DOC_BYTES; i++) {
    if (doc[i] != 0) return true;
  }
  return false;
}

// ----------------------------------------------------------------------------
//  JSON
// ----------------------------------------------------------------------------
String kosyncJsonEscape(const String& s) {
  String out;
  out.reserve(s.length() + 8);
  for (unsigned i = 0; i < s.length(); i++) {
    char c = s[i];
    switch (c) {
      case '"':  out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\n': out += "\\n";  break;
      case '\r': out += "\\r";  break;
      case '\t': out += "\\t";  break;
      default:
        if ((uint8_t)c < 0x20) {
          // Control characters must be escaped; \u00XX is the only form
          // JSON allows for the ones without a short escape.
          const char* digits = "0123456789abcdef";
          out += "\\u00";
          out += digits[((uint8_t)c >> 4) & 0x0F];
          out += digits[(uint8_t)c & 0x0F];
        } else {
          out += c;   // UTF-8 continuation bytes pass through untouched
        }
        break;
    }
  }
  return out;
}

String buildProgressBody(const KosyncPush& p) {
  float pct = p.percentage;
  if (pct < 0.0f) pct = 0.0f;
  if (pct > 1.0f) pct = 1.0f;

  // 4 decimals is what KOReader's own rounding produces and is finer than a
  // page on any book this device can hold.
  String pctStr = String(pct, 4);

  // An XPointer when we have one, the percentage string otherwise. Both are
  // JSON strings on the wire, so the shape of the body never changes.
  String progress = (p.progress.length() > 0) ? p.progress : pctStr;

  String out;
  out.reserve(260 + progress.length());
  out += "{\"document\":\"";
  out += kosyncJsonEscape(p.document);
  out += "\",\"progress\":\"";
  out += kosyncJsonEscape(progress);
  out += "\",\"percentage\":";
  out += pctStr;
  out += ",\"device\":\"";
  out += kosyncJsonEscape(p.device);
  out += "\",\"device_id\":\"";
  out += kosyncJsonEscape(p.deviceId);
  out += "\"}";
  return out;
}

// Locate the value for `key` in a flat JSON object.
//
// The kosync progress response is flat, so this walks the text tracking
// string state (and backslash escapes) rather than building a parse tree.
// Tracking string state is the part that matters: it stops a key name that
// happens to appear inside a value — a device called `percentage`, say —
// from being mistaken for the real field.
//
// On success `out` holds the raw value text: for a JSON string that is the
// unescaped-but-for-quotes contents, for anything else the literal token.
static bool findJsonValue(const String& json, const char* key, String& out) {
  const unsigned n = json.length();
  unsigned i = 0;

  while (i < n) {
    char c = json[i];

    if (c != '"') { i++; continue; }

    // Read a string token, remembering whether it is a key or a value by
    // what follows it.
    i++;
    String token;
    bool closed = false;
    while (i < n) {
      char d = json[i];
      if (d == '\\') {
        // Keep escapes verbatim; we only need exact matching on key names,
        // which never contain them.
        if (i + 1 < n) { token += json[i + 1]; i += 2; }
        else i++;
        continue;
      }
      if (d == '"') { closed = true; i++; break; }
      token += d;
      i++;
    }
    if (!closed) return false;

    // Is this a key? Skip whitespace and look for ':'.
    unsigned j = i;
    while (j < n && (json[j] == ' ' || json[j] == '\t' ||
                     json[j] == '\n' || json[j] == '\r')) j++;
    if (j >= n || json[j] != ':') continue;   // it was a value, keep scanning
    j++;
    while (j < n && (json[j] == ' ' || json[j] == '\t' ||
                     json[j] == '\n' || json[j] == '\r')) j++;
    if (j >= n) return false;

    if (token != key) {
      // Not our key — resume scanning from the start of its value so the
      // value's own contents are still string-aware.
      i = j;
      continue;
    }

    // Our key. Capture the value.
    if (json[j] == '"') {
      j++;
      String val;
      while (j < n) {
        char d = json[j];
        if (d == '\\') {
          if (j + 1 < n) {
            char e = json[j + 1];
            switch (e) {
              case 'n': val += '\n'; break;
              case 'r': val += '\r'; break;
              case 't': val += '\t'; break;
              default:  val += e;    break;
            }
            j += 2;
          } else j++;
          continue;
        }
        if (d == '"') { out = val; return true; }
        val += d;
        j++;
      }
      return false;   // unterminated
    }

    String val;
    while (j < n) {
      char d = json[j];
      if (d == ',' || d == '}' || d == ']' ||
          d == ' ' || d == '\t' || d == '\n' || d == '\r') break;
      val += d;
      j++;
    }
    if (val.length() == 0) return false;

    // Insist on a real delimiter after the token. A body truncated mid-number
    // (a dropped connection) would otherwise yield a plausible-looking but
    // silently wrong value — 0.5 for what was really 0.5123 — and send the
    // reader to the wrong page.
    while (j < n && (json[j] == ' ' || json[j] == '\t' ||
                     json[j] == '\n' || json[j] == '\r')) j++;
    if (j >= n) return false;
    if (json[j] != ',' && json[j] != '}' && json[j] != ']') return false;

    out = val;
    return true;
  }
  return false;
}

bool parseProgressResponse(const String& json, KosyncRemote& out) {
  out = KosyncRemote();
  if (json.length() == 0) return false;

  String pctText;
  if (!findJsonValue(json, "percentage", pctText)) return false;

  // Accept both the numeric form and a quoted one — servers in the wild do
  // both, and findJsonValue hands back the raw token either way.
  const char* begin = pctText.c_str();
  char* end = nullptr;
  double v = strtod(begin, &end);
  if (end == begin) return false;

  if (v < 0.0) v = 0.0;
  if (v > 1.0) v = 1.0;

  out.percentage = (float)v;
  findJsonValue(json, "progress", out.progress);   // optional
  findJsonValue(json, "device", out.device);       // optional
  out.valid = true;
  return true;
}

// ----------------------------------------------------------------------------
//  Position mapping
// ----------------------------------------------------------------------------
float percentageForOffset(uint32_t offset, uint32_t fileSize) {
  if (fileSize == 0) return 0.0f;
  if (offset >= fileSize) return 1.0f;
  return (float)((double)offset / (double)fileSize);
}

uint32_t offsetForPercentage(float pct, uint32_t fileSize) {
  if (fileSize == 0) return 0;
  if (pct <= 0.0f) return 0;
  if (pct >= 1.0f) return fileSize - 1;

  double raw = (double)pct * (double)fileSize;
  uint32_t off = (uint32_t)(raw + 0.5);
  // Keep the result addressable: `fileSize` itself is one past the last byte
  // and would make the reader treat the position as EOF.
  if (off >= fileSize) off = fileSize - 1;
  return off;
}

SyncDecision decideSync(float localPct, float remotePct) {
  float diff = remotePct - localPct;
  if (diff < 0.0f) diff = -diff;
  if (diff <= KOSYNC_DEADBAND) return SYNC_IDENTICAL;
  return (remotePct > localPct) ? SYNC_REMOTE_AHEAD : SYNC_REMOTE_BEHIND;
}
