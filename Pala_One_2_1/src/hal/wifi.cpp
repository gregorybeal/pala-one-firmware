#include "src/hal/wifi.h"

#include <ESPmDNS.h>
#include <WiFi.h>
#include <esp_bt.h>
#include <esp_wifi.h>

#include "src/state.h"               // AP_SSID, AP_PASS
#include "src/storage/wifi_creds.h"

static constexpr const char* kMdnsHost = "pala-one";

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

// ----------------------------------------------------------------------------
//  Attempt sequence state
//
//  Private to this file: callers only ever see Connecting / Connected /
//  Failed, so the whole multi-network walk is invisible above the HAL.
// ----------------------------------------------------------------------------
namespace {

enum class StaPhase {
  Idle,
  TryingLastGood,   // associating with the network that worked last time
  Scanning,         // async scan in flight
  TryingCandidate,  // associating with candidates_[candidateAt_]
  Exhausted
};

struct StaState {
  StaPhase phase = StaPhase::Idle;
  String   currentSsid;
  uint32_t phaseStartedMs = 0;

  // Saved-list indices that the scan found on the air, strongest first.
  uint8_t candidates[MAX_WIFI_NETWORKS];
  uint8_t candidateCount = 0;
  uint8_t candidateAt = 0;

  void reset() {
    phase = StaPhase::Idle;
    currentSsid = "";
    phaseStartedMs = 0;
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
  WiFi.begin(list.nets[listIndex].ssid, list.nets[listIndex].pass);
}

static void beginScan() {
  // Clear the failed association first — scanning on top of a half-open
  // connection attempt gives unreliable results.
  WiFi.disconnect(false, false);
  s_sta.currentSsid    = "";
  s_sta.phase          = StaPhase::Scanning;
  s_sta.phaseStartedMs = millis();
  WiFi.scanNetworks(/*async=*/true);
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
  if (idx >= 0) {
    beginAttempt(idx, StaPhase::TryingLastGood);
  } else {
    beginScan();
  }
  return true;
}

WifiStaResult wifiStaPoll(WifiSession& out) {
  switch (s_sta.phase) {
    case StaPhase::Idle:
    case StaPhase::Exhausted:
      return WifiStaResult::Failed;

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
        if (n == 0) {
          // None of the saved networks are here. Say so now instead of
          // burning a timeout per saved network.
          s_sta.phase = StaPhase::Exhausted;
          return WifiStaResult::Failed;
        }
      } else {
        candidatesFromWholeList(tried);
        if (s_sta.candidateCount == 0) {
          s_sta.phase = StaPhase::Exhausted;
          return WifiStaResult::Failed;
        }
      }

      beginAttempt(s_sta.candidates[s_sta.candidateAt], StaPhase::TryingCandidate);
      return WifiStaResult::Connecting;
    }

    case StaPhase::TryingLastGood:
    case StaPhase::TryingCandidate: {
      wl_status_t st = WiFi.status();
      if (st == WL_CONNECTED) {
        IPAddress ip = WiFi.localIP();
        MDNS.begin(kMdnsHost);
        MDNS.addService("http", "tcp", 80);

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
        beginScan();
        return WifiStaResult::Connecting;
      }

      s_sta.candidateAt++;
      if (s_sta.candidateAt >= s_sta.candidateCount) {
        s_sta.phase = StaPhase::Exhausted;
        return WifiStaResult::Failed;
      }
      WiFi.disconnect(false, false);
      beginAttempt(s_sta.candidates[s_sta.candidateAt], StaPhase::TryingCandidate);
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
  return kCandidateMs + kScanMs +
         (uint32_t)(WifiCreds::count() + 1) * kCandidateMs + 1000;
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
