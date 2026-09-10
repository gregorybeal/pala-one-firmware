#include "src/hal/wifi.h"

#include <ESPmDNS.h>
#include <WiFi.h>
#include <esp_bt.h>
#include <esp_wifi.h>

#include "src/state.h"               // AP_SSID, AP_PASS
#include "src/storage/wifi_creds.h"

static constexpr const char* kMdnsHost = "pala-one";

// Association is invisible from the device — the screen only ever says
// "Connecting". When it ends in the SoftAP fallback there is no way to tell
// which networks were tried, which the scan saw, or why each one failed, so
// the walk narrates itself over USB. Compiled out of release builds.
#if DEBUG_BUILD
  #define WIFI_LOG(...) Serial.printf("[wifi] " __VA_ARGS__)
#else
  #define WIFI_LOG(...) do {} while (0)
#endif

// How long one association attempt gets before we move on. Short enough that
// a wrong stored password doesn't dominate the sequence, long enough for a
// slow router to finish a DHCP handshake.
static constexpr uint32_t kCandidateMs = 6000;

// Budget for the async scan itself, used both as a local timeout and as part
// of wifiStaBudgetMs().
static constexpr uint32_t kScanMs = 5000;

// Grace period after WiFi.begin() before a failure status is believed.
// WiFi.status() can still be reporting the *previous* attempt's
// WL_CONNECT_FAILED for a moment after a new begin(), and without this the
// whole candidate list would be discarded inside a single poll.
static constexpr uint32_t kSettleMs = 700;

// How long to let the radio go quiet after a disconnect before asking it to
// scan or associate again. WiFi.disconnect() only *requests* a disconnect;
// issuing the next WiFi.begin() or scanNetworks() straight afterwards finds
// the station still in its connecting state, and the driver rejects the call
// outright ("sta is connecting, cannot set config"). The rejected attempt
// then looks exactly like a network that failed to associate — so a
// perfectly good saved network gets skipped without its credentials ever
// having been applied.
static constexpr uint32_t kQuietMs = 400;

// ----------------------------------------------------------------------------
//  Attempt sequence state
//
//  Private to this file: callers only ever see Connecting / Connected /
//  Failed, so the whole multi-network walk is invisible above the HAL.
// ----------------------------------------------------------------------------
namespace {

enum class StaPhase {
  Idle,
  Quiescing,        // disconnect issued; waiting for the radio to go idle
  TryingLastGood,   // associating with the network that worked last time
  Scanning,         // async scan in flight
  TryingCandidate,  // associating with candidates_[candidateAt_]
  Exhausted
};

// What Quiescing hands over to once the radio has settled.
enum class Pending { None, Scan, Attempt };

struct StaState {
  StaPhase phase = StaPhase::Idle;
  String   currentSsid;
  uint32_t phaseStartedMs = 0;

  // Queued across the quiet period.
  Pending  pending      = Pending::None;
  int      pendingIndex = -1;
  StaPhase pendingPhase = StaPhase::Idle;

  // Saved-list indices that the scan found on the air, strongest first.
  uint8_t candidates[MAX_WIFI_NETWORKS];
  uint8_t candidateCount = 0;
  uint8_t candidateAt = 0;

  void reset() {
    phase = StaPhase::Idle;
    currentSsid = "";
    phaseStartedMs = 0;
    pending = Pending::None;
    pendingIndex = -1;
    pendingPhase = StaPhase::Idle;
    candidateCount = 0;
    candidateAt = 0;
  }
};

StaState s_sta;

}  // namespace

// Start associating with one entry of the saved list.
static void beginAttempt(int listIndex, StaPhase phase) {
  const WifiList& list = WifiCreds::list();
  if (listIndex < 0 || listIndex >= (int)list.count) {
    s_sta.phase = StaPhase::Exhausted;
    return;
  }
  s_sta.currentSsid    = list.nets[listIndex].ssid;
  s_sta.phase          = phase;
  s_sta.phaseStartedMs = millis();
  WIFI_LOG("try %s \"%s\"\n",
           phase == StaPhase::TryingLastGood ? "last-good" : "candidate",
           list.nets[listIndex].ssid);
  WiFi.begin(list.nets[listIndex].ssid, list.nets[listIndex].pass);
}

static void beginScan() {
  s_sta.currentSsid    = "";
  s_sta.phase          = StaPhase::Scanning;
  s_sta.phaseStartedMs = millis();
  WIFI_LOG("scanning\n");
  WiFi.scanNetworks(/*async=*/true);
}

// Every scan and every association goes through here first: drop whatever the
// radio was doing, then park in Quiescing until it has actually stopped doing
// it. Without the pause the next call is issued against a station that is
// still connecting and is rejected by the driver.
static void quiesceThen(Pending action, int listIndex, StaPhase phase) {
  WiFi.disconnect(false, false);
  s_sta.pending        = action;
  s_sta.pendingIndex   = listIndex;
  s_sta.pendingPhase   = phase;
  s_sta.phase          = StaPhase::Quiescing;
  s_sta.phaseStartedMs = millis();
}

// Build the candidate list from a finished scan: saved networks that are
// actually on the air, strongest first, skipping the last-good one we have
// already tried. Returns the number of candidates.
static uint8_t collectCandidates(int16_t found, const String& alreadyTried) {
  const WifiList& list = WifiCreds::list();
  int8_t rssi[MAX_WIFI_NETWORKS] = {0};
  s_sta.candidateCount = 0;
  s_sta.candidateAt    = 0;

  for (int16_t i = 0; i < found; i++) {
    String ssid = WiFi.SSID(i);
    if (ssid.length() == 0) continue;
    if (ssid == alreadyTried) continue;

    int idx = wifiListFind(list, ssid.c_str());
    if (idx < 0) continue;

    // The same SSID can appear once per band or per access point; keep only
    // the strongest sighting so a weak one can't push a real option out.
    bool seen = false;
    for (uint8_t c = 0; c < s_sta.candidateCount; c++) {
      if (s_sta.candidates[c] == (uint8_t)idx) {
        seen = true;
        if (WiFi.RSSI(i) > rssi[c]) rssi[c] = (int8_t)WiFi.RSSI(i);
        break;
      }
    }
    if (seen) continue;
    if (s_sta.candidateCount >= MAX_WIFI_NETWORKS) continue;

    rssi[s_sta.candidateCount] = (int8_t)WiFi.RSSI(i);
    s_sta.candidates[s_sta.candidateCount] = (uint8_t)idx;
    s_sta.candidateCount++;
  }

  // Insertion sort by RSSI, descending. At most MAX_WIFI_NETWORKS entries.
  for (uint8_t i = 0; i < s_sta.candidateCount; i++) {
    for (uint8_t j = (uint8_t)(i + 1); j < s_sta.candidateCount; j++) {
      if (rssi[j] > rssi[i]) {
        int8_t tr = rssi[i]; rssi[i] = rssi[j]; rssi[j] = tr;
        uint8_t tc = s_sta.candidates[i];
        s_sta.candidates[i] = s_sta.candidates[j];
        s_sta.candidates[j] = tc;
      }
    }
  }
  return s_sta.candidateCount;
}

// Append the saved networks the scan did NOT report, in list order, skipping
// anything already queued or already tried.
//
// A hidden access point never announces its SSID, so it cannot appear in scan
// results however close you are standing to it — matching one against the
// saved list is simply not possible. The only way to reach it is to call
// WiFi.begin() with the name and let the AP answer. The same applies to a
// network the scan missed for duller reasons: a weak or busy channel, or the
// scan being cut short at kScanMs.
static void appendUnseenCandidates(const String& alreadyTried) {
  const WifiList& list = WifiCreds::list();
  for (uint8_t i = 0; i < list.count && s_sta.candidateCount < MAX_WIFI_NETWORKS; i++) {
    if (alreadyTried.length() > 0 && alreadyTried == list.nets[i].ssid) continue;

    bool queued = false;
    for (uint8_t c = 0; c < s_sta.candidateCount; c++) {
      if (s_sta.candidates[c] == i) { queued = true; break; }
    }
    if (queued) continue;

    s_sta.candidates[s_sta.candidateCount++] = i;
  }
}

// Scan came back unusable — fall back to walking every saved network in list
// order. Slower, but better than refusing to connect because the radio would
// not scan.
static void candidatesFromWholeList(const String& alreadyTried) {
  const WifiList& list = WifiCreds::list();
  s_sta.candidateCount = 0;
  s_sta.candidateAt    = 0;
  for (uint8_t i = 0; i < list.count; i++) {
    if (alreadyTried.length() > 0 && alreadyTried == list.nets[i].ssid) continue;
    s_sta.candidates[s_sta.candidateCount++] = i;
  }
}

// ----------------------------------------------------------------------------

bool wifiStaBegin() {
  if (!WifiCreds::has()) return false;

  setCpuFrequencyMhz(240);
  s_sta.reset();
  WiFi.mode(WIFI_STA);

  // Fast path: the network that worked last time, if it is still saved.
  // Skipping the scan is the whole point — it makes the ordinary "I am at
  // home" case as quick as it was when only one network could be stored.
  const String lastGood = WifiCreds::lastGoodSsid();
  int idx = (lastGood.length() > 0)
              ? wifiListFind(WifiCreds::list(), lastGood.c_str())
              : -1;
  WIFI_LOG("begin: %u saved, last-good \"%s\"\n",
           (unsigned)WifiCreds::count(), lastGood.c_str());

  if (idx >= 0) {
    quiesceThen(Pending::Attempt, idx, StaPhase::TryingLastGood);
  } else {
    quiesceThen(Pending::Scan, -1, StaPhase::Idle);
  }
  return true;
}

WifiStaResult wifiStaPoll(WifiSession& out) {
  switch (s_sta.phase) {
    case StaPhase::Idle:
    case StaPhase::Exhausted:
      return WifiStaResult::Failed;

    case StaPhase::Quiescing: {
      if ((uint32_t)(millis() - s_sta.phaseStartedMs) < kQuietMs) {
        return WifiStaResult::Connecting;
      }
      Pending what = s_sta.pending;
      s_sta.pending = Pending::None;

      if (what == Pending::Scan) {
        beginScan();
      } else {
        beginAttempt(s_sta.pendingIndex, s_sta.pendingPhase);
        if (s_sta.phase == StaPhase::Exhausted) return WifiStaResult::Failed;
      }
      return WifiStaResult::Connecting;
    }

    case StaPhase::Scanning: {
      int16_t found = WiFi.scanComplete();
      if (found == WIFI_SCAN_RUNNING) {
        if ((uint32_t)(millis() - s_sta.phaseStartedMs) <= kScanMs) {
          return WifiStaResult::Connecting;
        }
        WiFi.scanDelete();
        found = WIFI_SCAN_FAILED;   // treat a stuck scan as a failed one
      }

      const String tried = WifiCreds::lastGoodSsid();
      if (found >= 0) {
        uint8_t n = collectCandidates(found, tried);
        WiFi.scanDelete();
        WIFI_LOG("scan found %d ap(s), %u saved on the air\n", (int)found, (unsigned)n);
        // Zero access points anywhere means the scan did not run properly,
        // not that the airwaves are empty. Either way the saved list is the
        // only thing left to go on.
        if (n == 0) {
          // Nothing of ours in the results. Usually that means we really are
          // somewhere else — but a hidden network is invisible to a scan no
          // matter where we are, so try the saved list directly before
          // dropping to the access point. Costs one association timeout per
          // network, and only in the case that was about to fail outright.
          appendUnseenCandidates(tried);
          WIFI_LOG("no scan matches; trying %u saved network(s) directly\n",
                   (unsigned)s_sta.candidateCount);
        }
      } else {
        WIFI_LOG("scan unusable (%d); walking the saved list\n", (int)found);
        candidatesFromWholeList(tried);
      }

      if (s_sta.candidateCount == 0) {
        WIFI_LOG("no candidates left -> access point\n");
        s_sta.phase = StaPhase::Exhausted;
        return WifiStaResult::Failed;
      }

      quiesceThen(Pending::Attempt, s_sta.candidates[s_sta.candidateAt],
                  StaPhase::TryingCandidate);
      return WifiStaResult::Connecting;
    }

    case StaPhase::TryingLastGood:
    case StaPhase::TryingCandidate: {
      wl_status_t st = WiFi.status();
      if (st == WL_CONNECTED) {
        IPAddress ip = WiFi.localIP();
        MDNS.begin(kMdnsHost);
        MDNS.addService("http", "tcp", 80);

        WIFI_LOG("connected to \"%s\" as %s\n",
                 s_sta.currentSsid.c_str(), ip.toString().c_str());

        // Remember what worked so the next session can skip the scan.
        WifiCreds::setLastGoodSsid(s_sta.currentSsid);

        out.mode        = WifiMode::Station;
        out.staSsid     = s_sta.currentSsid;
        out.primaryUrl  = String("http://") + kMdnsHost + ".local";
        out.fallbackUrl = String("http://") + ip.toString();
        s_sta.phase     = StaPhase::Idle;
        return WifiStaResult::Connected;
      }

      uint32_t elapsed = (uint32_t)(millis() - s_sta.phaseStartedMs);
      bool hardFail = (elapsed > kSettleMs) &&
                      (st == WL_NO_SSID_AVAIL ||
                       st == WL_CONNECT_FAILED ||
                       st == WL_CONNECTION_LOST);
      bool timedOut = elapsed > kCandidateMs;
      if (!hardFail && !timedOut) return WifiStaResult::Connecting;

      if (s_sta.phase == StaPhase::TryingLastGood) {
        // The remembered network isn't reachable — find out what is.
        WIFI_LOG("last-good \"%s\" failed (status %d, %ums)\n",
                 s_sta.currentSsid.c_str(), (int)st, (unsigned)elapsed);
        quiesceThen(Pending::Scan, -1, StaPhase::Idle);
        return WifiStaResult::Connecting;
      }

      WIFI_LOG("\"%s\" failed (status %d, %ums)\n",
               s_sta.currentSsid.c_str(), (int)st, (unsigned)elapsed);

      s_sta.candidateAt++;
      if (s_sta.candidateAt >= s_sta.candidateCount) {
        WIFI_LOG("all %u candidate(s) exhausted -> access point\n",
                 (unsigned)s_sta.candidateCount);
        s_sta.phase = StaPhase::Exhausted;
        return WifiStaResult::Failed;
      }
      quiesceThen(Pending::Attempt, s_sta.candidates[s_sta.candidateAt],
                  StaPhase::TryingCandidate);
      return WifiStaResult::Connecting;
    }
  }
  return WifiStaResult::Failed;
}

String wifiStaCurrentSsid() { return s_sta.currentSsid; }

uint32_t wifiStaBudgetMs() {
  // Last-good attempt + scan + one attempt per saved network, with a little
  // slack. Callers use this as a safety net; the sequence normally reports
  // Failed well before it.
  const uint32_t attempts = (uint32_t)WifiCreds::count() + 1;
  return kCandidateMs + kScanMs
       + attempts * kCandidateMs
       + (attempts + 1) * kQuietMs      // one quiet period before each step
       + 1000;
}

void wifiStaAbort() {
  if (s_sta.phase == StaPhase::Scanning) WiFi.scanDelete();
  WiFi.disconnect(true, true);
  WiFi.mode(WIFI_OFF);
  s_sta.reset();
  // CPU clock stays at 240 MHz — the typical next call is
  // wifiBeginAccessPoint() which needs it anyway. wifiEnd() drops it.
}

WifiSession wifiBeginAccessPoint() {
  setCpuFrequencyMhz(240);

  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);
  IPAddress ip = WiFi.softAPIP();

  WifiSession s;
  s.mode       = WifiMode::AccessPoint;
  s.apSsid     = AP_SSID;
  s.apPass     = AP_PASS;
  s.primaryUrl = String("http://") + ip.toString();
  return s;
}

void wifiEnd() {
  if (s_sta.phase == StaPhase::Scanning) WiFi.scanDelete();
  MDNS.end();
  WiFi.softAPdisconnect(true);
  WiFi.disconnect(true, true);
  WiFi.mode(WIFI_OFF);
  delay(100);
  esp_wifi_stop();
  btStop();
  s_sta.reset();
  setCpuFrequencyMhz(80);  // back to low-power idle
}
