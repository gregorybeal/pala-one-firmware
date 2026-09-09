#include "wifi_list_codec.h"

#include <cstring>

// Copy at most `cap` bytes of a NUL-terminated source into a `cap + 1` buffer,
// always NUL-terminating. Truncates rather than overflowing.
static void copyBounded(char* dst, const char* src, size_t cap) {
  size_t n = 0;
  if (src) {
    // Bound first, then index: the other order reads src[cap] once before
    // stopping, which is exactly what cppcheck's arrayIndexThenCheck warns
    // about even though a NUL-terminated source makes it harmless here.
    while (n < cap && src[n] != '\0') {
      dst[n] = src[n];
      n++;
    }
  }
  dst[n] = '\0';
}

size_t encodeWifiList(const WifiList& list, uint8_t* outBuf) {
  uint8_t count = list.count;
  if (count > MAX_WIFI_NETWORKS) count = MAX_WIFI_NETWORKS;

  std::memset(outBuf, 0, WIFI_LIST_ENCODED_MAX_SIZE);
  outBuf[0] = WIFI_LIST_BLOB_VERSION;
  outBuf[1] = count;

  size_t p = 2;
  for (uint8_t i = 0; i < count; i++) {
    size_t sl = std::strlen(list.nets[i].ssid);
    if (sl > (size_t)MAX_WIFI_SSID) sl = MAX_WIFI_SSID;
    outBuf[p++] = (uint8_t)sl;
    std::memcpy(outBuf + p, list.nets[i].ssid, sl);
    p += sl;

    size_t pl = std::strlen(list.nets[i].pass);
    if (pl > (size_t)MAX_WIFI_PASS) pl = MAX_WIFI_PASS;
    outBuf[p++] = (uint8_t)pl;
    std::memcpy(outBuf + p, list.nets[i].pass, pl);
    p += pl;
  }
  return p;
}

bool decodeWifiList(const uint8_t* buf, size_t got, WifiList& out) {
  out.count = 0;
  if (!buf || got < 2) return false;
  if (buf[0] != WIFI_LIST_BLOB_VERSION) return false;

  uint8_t count = buf[1];
  if (count > MAX_WIFI_NETWORKS) return false;

  // Decode into a scratch list and only publish it once every entry has been
  // read whole — a half-filled credential list would look connectable and
  // never work.
  WifiList tmp;
  size_t p = 2;
  for (uint8_t i = 0; i < count; i++) {
    if (p >= got) return false;
    size_t sl = buf[p++];
    if (sl > (size_t)MAX_WIFI_SSID || p + sl > got) return false;
    std::memcpy(tmp.nets[i].ssid, buf + p, sl);
    tmp.nets[i].ssid[sl] = '\0';
    p += sl;

    if (p >= got) return false;
    size_t pl = buf[p++];
    if (pl > (size_t)MAX_WIFI_PASS || p + pl > got) return false;
    std::memcpy(tmp.nets[i].pass, buf + p, pl);
    tmp.nets[i].pass[pl] = '\0';
    p += pl;

    // An entry with no SSID can never be connected to; treat the blob as
    // corrupt rather than carrying a dead row around.
    if (tmp.nets[i].ssid[0] == '\0') return false;
  }

  tmp.count = count;
  out = tmp;
  return true;
}

int wifiListFind(const WifiList& list, const char* ssid) {
  if (!ssid || ssid[0] == '\0') return -1;
  uint8_t count = list.count;
  if (count > MAX_WIFI_NETWORKS) count = MAX_WIFI_NETWORKS;
  for (uint8_t i = 0; i < count; i++) {
    if (std::strcmp(list.nets[i].ssid, ssid) == 0) return (int)i;
  }
  return -1;
}

bool wifiListUpsert(WifiList& list, const char* ssid, const char* pass) {
  if (!ssid || ssid[0] == '\0') return false;
  if (std::strlen(ssid) > (size_t)MAX_WIFI_SSID) return false;

  int at = wifiListFind(list, ssid);
  if (at >= 0) {
    // Known network — update the passphrase in place. Re-adding an SSID from
    // the web form is how a changed password gets in, so this must not append
    // a duplicate.
    copyBounded(list.nets[at].pass, pass, MAX_WIFI_PASS);
    return true;
  }

  if (list.count >= MAX_WIFI_NETWORKS) {
    // Full — evict the oldest, matching addBookmark's behaviour. Silently
    // failing the add would be worse: the user has no idea which of five
    // invisible entries to remove first.
    for (uint8_t i = 1; i < MAX_WIFI_NETWORKS; i++) {
      list.nets[i - 1] = list.nets[i];
    }
    list.count = MAX_WIFI_NETWORKS - 1;
  }

  copyBounded(list.nets[list.count].ssid, ssid, MAX_WIFI_SSID);
  copyBounded(list.nets[list.count].pass, pass, MAX_WIFI_PASS);
  list.count++;
  return true;
}

bool wifiListRemoveAt(WifiList& list, int index) {
  if (index < 0 || index >= (int)list.count) return false;
  for (uint8_t i = (uint8_t)(index + 1); i < list.count; i++) {
    list.nets[i - 1] = list.nets[i];
  }
  list.count--;
  return true;
}

bool migrateLegacyCreds(const String& ssid, const String& pass, WifiList& out) {
  out = WifiList();
  if (ssid.length() == 0) return false;
  return wifiListUpsert(out, ssid.c_str(), pass.c_str());
}
