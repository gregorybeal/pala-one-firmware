#ifndef PALA_HAL_WIFI_H
#define PALA_HAL_WIFI_H

#include <Arduino.h>

// Upload-session network primitives. All non-blocking — the upload screen
// drives the STA-vs-AP decision itself with a small state machine so the
// main loop stays alive throughout (button events, Improv polling, screen
// redraws keep working through a slow STA association).
//
// Several networks can be stored (storage/wifi_creds.h). Choosing between
// them lives entirely inside wifiStaBegin/wifiStaPoll, so callers still see
// exactly three outcomes: `Connecting` for the whole attempt sequence, and
// `Failed` only once every candidate is exhausted. The sequence is:
//
//   1. the last network that actually connected, if it is still saved —
//      no scan, so the common case (you are at home) is immediate
//   2. otherwise an async scan, then the saved networks that are actually
//      on the air, strongest first
//
// A scan that finds none of your networks fails fast, which is what keeps
// "I am somewhere new" from turning into a string of association timeouts.
//
// Typical sequence:
//
//   if (wifiStaBegin()) {
//     // poll wifiStaPoll() each loop iteration; on Connected, take the
//     // session. On Failed (or after the caller's own timeout) call
//     // wifiStaAbort() and fall back to wifiBeginAccessPoint().
//   } else {
//     // no stored creds — straight to AP.
//     session = wifiBeginAccessPoint();
//   }
//   ... use session ...
//   wifiEnd();
//
// CPU clock: any of the begin functions bumps to 240 MHz (Wi-Fi needs it);
// wifiEnd() drops back to 80 MHz. wifiStaAbort() leaves the clock alone in
// case the caller is about to call wifiBeginAccessPoint().

enum class WifiMode { Station, AccessPoint };

struct WifiSession {
  WifiMode    mode = WifiMode::AccessPoint;
  String      primaryUrl;     // STA: http://pala-one.local  AP: http://192.168.4.1
  String      fallbackUrl;    // STA: http://<lan-ip>        AP: ""
  const char* apSsid = "";    // populated in AccessPoint mode only
  const char* apPass = "";    // populated in AccessPoint mode only
  String      staSsid;        // populated in Station mode only
};

enum class WifiStaResult {
  Connecting,   // still associating — keep polling
  Connected,    // associated; `out` is filled, stop polling
  Failed,       // hard error from the stack (no SSID, bad password, etc.)
};

// Kick off an STA association attempt against the stored credentials.
// Returns false if no creds are saved (caller should go straight to AP).
// Non-blocking; bumps CPU to 240 MHz.
bool          wifiStaBegin();

// Poll the in-flight STA attempt. On Connected, `out` is filled with the
// session and the caller takes ownership. On Failed, the caller should
// call wifiStaAbort() and fall back to AP. Per-candidate timing is handled
// internally; a caller's own timeout is a safety net and should come from
// wifiStaBudgetMs().
WifiStaResult wifiStaPoll(WifiSession& out);

// SSID currently being attempted, or "" when nothing is in flight. For the
// connecting splash — with several saved networks, "Connecting..." alone
// doesn't tell the user which one, or that it has moved on to another.
String wifiStaCurrentSsid();

// Upper bound on how long the whole attempt sequence can take, given how
// many networks are stored. Callers use it to size their safety timeout
// instead of a fixed constant that no longer fits a multi-network sequence.
// Normal completion is far quicker — this only bounds the pathological case
// where several saved networks are present and all reject us.
uint32_t wifiStaBudgetMs();

// Tear down an in-flight STA attempt, including any scan still running.
// Cheaper than wifiEnd() because no full session was ever brought up; leaves
// the CPU clock alone since the caller typically calls
// wifiBeginAccessPoint() next.
void          wifiStaAbort();

// Bring up SoftAP. Always succeeds. Bumps CPU to 240 MHz if not already.
WifiSession   wifiBeginAccessPoint();

// Tears down whichever mode is active and drops CPU back to 80 MHz.
void          wifiEnd();

#endif  // PALA_HAL_WIFI_H
