#ifndef PALA_STORAGE_KOSYNC_SETTINGS_H
#define PALA_STORAGE_KOSYNC_SETTINGS_H

#include <Arduino.h>

// ============================================================================
//  KOReader sync account settings, in the shared "ereader" NVS namespace
//  (`prefs`) under the ks_* key prefix — same convention as WifiCreds and the
//  cfg_* settings owned by the ui/ modules.
//
//  The plaintext password is never stored: the kosync protocol authenticates
//  with `x-auth-key: <md5 hex of password>`, so the web form hashes on submit
//  and only the digest is persisted. That digest is both all an attacker with
//  flash access would get and all they would need — no worse and no better
//  than the protocol itself allows.
// ============================================================================
namespace Kosync {

// Read every ks_* key into the module's cache. Call once from setup(),
// alongside Font::loadSettings() and friends.
void loadSettings();

// Configured == a username and key are present. Independent of `enabled()`:
// a user can turn sync off without discarding their credentials.
bool configured();

bool   enabled();
void   setEnabled(bool on);

// Base URL with no trailing slash, e.g. "https://sync.koreader.rocks".
// http:// is accepted so a self-hosted LAN server works without TLS.
String serverUrl();
void   setServerUrl(const String& url);

String username();
String authKey();                                  // md5 hex of the password
void   setAccount(const String& user, const String& authKeyHex);
void   clearAccount();

// Stable per-device id sent as `device_id`. Derived once from the eFuse MAC
// on first use and persisted.
String deviceId();

// Human-readable `device` field. Constant, not user-settable.
const char* deviceName();

// Normalize a user-entered base URL: trim, default the scheme to https://,
// strip any trailing slashes. Returns "" if nothing usable is left.
String normalizeServerUrl(const String& raw);

}  // namespace Kosync

#endif  // PALA_STORAGE_KOSYNC_SETTINGS_H
