#include "src/storage/wifi_creds.h"

#include "src/state.h"  // prefs

namespace WifiCreds {

static constexpr const char* kKeyList     = "wifi_nets";
static constexpr const char* kKeyLastGood = "wifi_last";

// Pre-list scheme. Read once at migration time, then removed.
static constexpr const char* kLegacyKeySsid = "wifi_ssid";
static constexpr const char* kLegacyKeyPass = "wifi_pass";

static WifiList s_list;
static String   s_lastGood;

static void persist() {
  uint8_t buf[WIFI_LIST_ENCODED_MAX_SIZE];
  size_t n = encodeWifiList(s_list, buf);
  prefs.putBytes(kKeyList, buf, n);
}

void loadSettings() {
  s_list = WifiList();
  s_lastGood = prefs.getString(kKeyLastGood, "");

  uint8_t buf[WIFI_LIST_ENCODED_MAX_SIZE];
  size_t got = prefs.getBytes(kKeyList, buf, sizeof(buf));
  if (got > 0 && decodeWifiList(buf, got, s_list)) return;

  // No usable list. Before concluding there are no credentials, check for the
  // single-credential scheme this replaced — an upgrading device has a working
  // network stored there and must not be silently cut off from it. The legacy
  // keys are only removed once the new blob is safely written.
  String legacySsid = prefs.getString(kLegacyKeySsid, "");
  String legacyPass = prefs.getString(kLegacyKeyPass, "");
  if (!migrateLegacyCreds(legacySsid, legacyPass, s_list)) return;

  persist();
  prefs.remove(kLegacyKeySsid);
  prefs.remove(kLegacyKeyPass);

  // The migrated network is by definition the one that was working.
  if (s_lastGood.length() == 0) setLastGoodSsid(legacySsid);
}

bool has()    { return s_list.count > 0; }
uint8_t count() { return s_list.count; }
const WifiList& list() { return s_list; }

bool add(const String& ssid, const String& pass) {
  if (!wifiListUpsert(s_list, ssid.c_str(), pass.c_str())) return false;
  persist();
  return true;
}

bool removeAt(int index) {
  if (index < 0 || index >= (int)s_list.count) return false;
  String goingAway = s_list.nets[index].ssid;
  if (!wifiListRemoveAt(s_list, index)) return false;
  persist();

  // Don't leave the fast path pointing at a network we just forgot.
  if (goingAway == s_lastGood) setLastGoodSsid("");
  return true;
}

void clear() {
  s_list = WifiList();
  prefs.remove(kKeyList);
  setLastGoodSsid("");
}

String lastGoodSsid() { return s_lastGood; }

void setLastGoodSsid(const String& ssid) {
  if (s_lastGood == ssid) return;   // avoid a pointless NVS write per connect
  s_lastGood = ssid;
  if (ssid.length() == 0) prefs.remove(kKeyLastGood);
  else                    prefs.putString(kKeyLastGood, ssid);
}

}  // namespace WifiCreds
