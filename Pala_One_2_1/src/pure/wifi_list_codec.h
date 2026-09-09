#ifndef PALA_PURE_WIFI_LIST_CODEC_H
#define PALA_PURE_WIFI_LIST_CODEC_H

#include "arduino_compat.h"
#include "../config.h"

// ============================================================================
//  Saved Wi-Fi networks — pure list operations + the NVS byte blob codec.
//
//  Same shape as pure/bookmarks_codec.h: a versioned, length-prefixed blob
//  with a pure encoder/decoder, so the whole thing compiles into the host
//  test build and the storage layer stays a thin NVS shim.
//
//  The list is NOT a priority order. `hal/wifi.cpp` tries the last network
//  that worked, then scans and picks the strongest saved network actually on
//  the air — so position carries no meaning and there is deliberately no
//  reorder operation.
// ============================================================================

struct WifiNetwork {
  char ssid[MAX_WIFI_SSID + 1];
  char pass[MAX_WIFI_PASS + 1];
};

struct WifiList {
  WifiNetwork nets[MAX_WIFI_NETWORKS];
  uint8_t     count = 0;
};

// Blob layout v1:
//   [0] version (== WIFI_LIST_BLOB_VERSION)
//   [1] count
//   then per entry: ssidLen, ssid bytes, passLen, pass bytes
//
// Length-prefixed rather than fixed-width: a fixed layout would spend ~490
// bytes of a 20 KB NVS partition that already carries bookmarks and per-book
// progress, where real content is usually under 150.
static const uint8_t WIFI_LIST_BLOB_VERSION = 1;

// Largest possible encoded blob.
constexpr size_t WIFI_LIST_ENCODED_MAX_SIZE =
    2u + (size_t)MAX_WIFI_NETWORKS * (1u + (size_t)MAX_WIFI_SSID +
                                      1u + (size_t)MAX_WIFI_PASS);

// Encode `list` into `outBuf` (>= WIFI_LIST_ENCODED_MAX_SIZE bytes).
// Returns bytes written.
size_t encodeWifiList(const WifiList& list, uint8_t* outBuf);

// Decode `got` bytes into `out`. Returns false — leaving `out` empty rather
// than half-filled — for an empty, truncated, over-long or wrong-version
// blob. A partially decoded credential list is worse than none: it would look
// connectable and never work.
bool decodeWifiList(const uint8_t* buf, size_t got, WifiList& out);

// Index of `ssid` in the list, or -1. Exact, case-sensitive match: SSIDs are
// octet strings and two differing only in case are two different networks.
int wifiListFind(const WifiList& list, const char* ssid);

// Add `ssid`/`pass`, or update the password if that SSID is already known
// (keeping its position). When the list is full the oldest entry is evicted,
// the same way pure/bookmarks_codec.cpp's addBookmark does.
// Returns false only for an unusable SSID (empty, or over MAX_WIFI_SSID).
// An over-long passphrase is truncated rather than rejected.
bool wifiListUpsert(WifiList& list, const char* ssid, const char* pass);

// Remove entry `index`, shifting the rest down. Returns false if out of range.
bool wifiListRemoveAt(WifiList& list, int index);

// One-time import of the pre-list single-credential scheme (NVS `wifi_ssid` /
// `wifi_pass`). Returns false — and leaves `out` empty — when there is no
// legacy SSID to import. Pure so the migration is testable without NVS.
bool migrateLegacyCreds(const String& ssid, const String& pass, WifiList& out);

#endif  // PALA_PURE_WIFI_LIST_CODEC_H
