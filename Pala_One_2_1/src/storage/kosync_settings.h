#ifndef PALA_STORAGE_KOSYNC_SETTINGS_H
#define PALA_STORAGE_KOSYNC_SETTINGS_H

#include <Arduino.h>

#include "src/pure/kosync_codec.h"   // KosyncFragmentNumbering

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

// Which numbering KOReader's `DocFragment[N]` uses. Whether crengine counts
// spine itemrefs that carry no text is a property of the KOReader build on
// the other device rather than of any one book, so the answer is the same for
// every book the user owns and is worth settling once instead of inferring
// per sync.
//
// The values and the rule that turns one into candidates belong to
// pure/kosync_codec.h (KOSYNC_FRAG_*, kosyncNumberingsToTry,
// kosyncChooseFragment, and the note there on why the Auto referee is biased
// late). This layer only persists the choice.
KosyncFragmentNumbering fragmentNumbering();
void                    setFragmentNumbering(KosyncFragmentNumbering mode);

// ----------------------------------------------------------------------------
//  Last-pull diagnostic
//
//  What the server actually sent on the most recent sync, recorded so the web
//  UI can show it. This exists because the failure mode it diagnoses — a
//  pointer resolved under the wrong spine numbering — is otherwise invisible:
//  the device jumps somewhere confidently wrong and nothing on screen says
//  which numbering it used or what it was given to work with.
//
//  Purely informational. Nothing reads it back into the sync path, and a
//  missing or stale record changes no behaviour.
// ----------------------------------------------------------------------------

enum LastPullOutcome {
  LAST_PULL_NO_POINTER = 0,   // server sent a percentage, not an XPointer
  LAST_PULL_OPF        = 1,   // resolved under OPF numbering
  LAST_PULL_LINEAR     = 2,   // resolved under linear numbering
  LAST_PULL_REJECTED   = 3,   // parsed, but no candidate was usable
  LAST_PULL_NO_MAP     = 4,   // book has no spine map — percentage fallback
};

struct LastPull {
  bool   present   = false;
  String book;                // library name of the book that was synced
  String pointer;             // raw `progress` field, truncated to fit NVS
  float  remotePct = 0.0f;    // percentage the server sent alongside it
  int    outcome   = LAST_PULL_NO_POINTER;
  float  landedPct = 0.0f;    // where the device decided that is, in our text
};

LastPull lastPull();
void     recordLastPull(const LastPull& p);

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
