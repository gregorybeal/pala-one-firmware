#ifndef PALA_STORAGE_WIFI_CREDS_H
#define PALA_STORAGE_WIFI_CREDS_H

#include <Arduino.h>

#include "src/pure/wifi_list_codec.h"

// ============================================================================
//  Saved home-network Wi-Fi credentials — up to MAX_WIFI_NETWORKS of them.
//
//  Written by the Improv Serial flow (hal/wifi_provisioning.cpp) and by the
//  web UI (web/wifi.cpp); read by hal/wifi.cpp when choosing which network to
//  associate with. Stored in the shared "ereader" NVS namespace (`prefs`)
//  under the existing wifi_* key convention:
//
//    wifi_nets  blob    the encoded list (pure/wifi_list_codec.h)
//    wifi_last  string  SSID of the last network that actually connected
//
//  `wifi_last` is what makes the common case fast: hal/wifi.cpp tries it
//  first and only scans when it fails, so being at home costs no scan.
//
//  Upgrades from the pre-list scheme (`wifi_ssid` / `wifi_pass`) are migrated
//  on the first loadSettings(); see the note there.
// ============================================================================
namespace WifiCreds {

// Decode the stored list into the module cache, migrating the legacy keys if
// that is what is on flash. Call once from setup(), alongside
// Font::loadSettings() and friends.
void loadSettings();

// True iff at least one network is stored. This is the question every caller
// outside hal/wifi.cpp actually asks ("can we even try station mode?"), and
// it kept its name across the single -> list change so those call sites did
// not have to.
bool has();

uint8_t         count();
const WifiList& list();

// Add a network, or update the passphrase if that SSID is already stored.
// Persists on success. False for an unusable SSID (empty / over-long).
bool add(const String& ssid, const String& pass);

// Forget entry `index`. Persists. Also clears `wifi_last` when the forgotten
// network was the last-good one, so hal/wifi.cpp does not try to associate
// with an SSID that is no longer in the list.
bool removeAt(int index);

// Forget everything, including the last-good marker.
void clear();

String lastGoodSsid();
void   setLastGoodSsid(const String& ssid);

}  // namespace WifiCreds

#endif  // PALA_STORAGE_WIFI_CREDS_H
