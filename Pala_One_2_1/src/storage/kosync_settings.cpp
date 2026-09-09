#include "src/storage/kosync_settings.h"

#include "src/state.h"   // prefs

namespace Kosync {

static constexpr const char* kKeyUrl     = "ks_url";
static constexpr const char* kKeyUser    = "ks_user";
static constexpr const char* kKeyAuth    = "ks_key";
static constexpr const char* kKeyEnabled = "ks_on";
static constexpr const char* kKeyDevId   = "ks_devid";

static constexpr const char* kDefaultUrl = "https://sync.koreader.rocks";

// Cached so the sync screen and the reader menu can ask cheaply on every
// draw without hitting NVS.
static String s_url;
static String s_user;
static String s_auth;
static String s_devId;
static bool   s_enabled = false;

void loadSettings() {
  s_url     = prefs.getString(kKeyUrl, kDefaultUrl);
  s_user    = prefs.getString(kKeyUser, "");
  s_auth    = prefs.getString(kKeyAuth, "");
  s_devId   = prefs.getString(kKeyDevId, "");
  s_enabled = prefs.getInt(kKeyEnabled, 0) != 0;

  if (s_url.length() == 0) s_url = kDefaultUrl;
}

bool configured() {
  return s_user.length() > 0 && s_auth.length() > 0 && s_url.length() > 0;
}

bool enabled() { return s_enabled && configured(); }

void setEnabled(bool on) {
  s_enabled = on;
  prefs.putInt(kKeyEnabled, on ? 1 : 0);
}

String serverUrl() { return s_url; }

void setServerUrl(const String& url) {
  String clean = normalizeServerUrl(url);
  if (clean.length() == 0) clean = kDefaultUrl;
  s_url = clean;
  prefs.putString(kKeyUrl, clean);
}

String username() { return s_user; }
String authKey()  { return s_auth; }

void setAccount(const String& user, const String& authKeyHex) {
  s_user = user;
  s_auth = authKeyHex;
  prefs.putString(kKeyUser, user);
  prefs.putString(kKeyAuth, authKeyHex);
}

void clearAccount() {
  s_user = "";
  s_auth = "";
  prefs.remove(kKeyUser);
  prefs.remove(kKeyAuth);
  setEnabled(false);
}

String deviceId() {
  if (s_devId.length() > 0) return s_devId;

  // KOReader generates a random hex string here; the server treats it as an
  // opaque tag, so anything stable and unique will do. The eFuse MAC gives us
  // that without needing entropy at first boot.
  uint64_t mac = ESP.getEfuseMac();
  char buf[17];
  snprintf(buf, sizeof(buf), "%016llX", (unsigned long long)mac);
  s_devId = buf;
  prefs.putString(kKeyDevId, s_devId);
  return s_devId;
}

const char* deviceName() { return "Pala One"; }

String normalizeServerUrl(const String& raw) {
  String s = raw;
  s.trim();
  if (s.length() == 0) return String("");

  // A bare host is the common paste; assume TLS rather than silently
  // downgrading someone's credentials onto a plaintext connection.
  if (!s.startsWith("http://") && !s.startsWith("https://")) {
    s = "https://" + s;
  }
  while (s.length() > 0 && s[s.length() - 1] == '/') {
    s.remove(s.length() - 1);
  }
  // Scheme with nothing after it is not a server.
  if (s == "https:/" || s == "http:/" || s == "https:" || s == "http:") {
    return String("");
  }
  return s;
}

}  // namespace Kosync
