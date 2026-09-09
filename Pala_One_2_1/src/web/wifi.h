#ifndef PALA_WEB_WIFI_H
#define PALA_WEB_WIFI_H

// ============================================================================
//  Saved Wi-Fi networks.
//
//    GET  /wifi         — the saved list + an add form
//    POST /wifi         — add a network, or replace a known SSID's password
//    POST /wifi-forget  — remove one by index
//
//  Before this page existed, credentials could only be set over USB through
//  Improv Serial (hal/wifi_provisioning.cpp) — which meant plugging the
//  device into a computer just to point it at a different network.
//
//  Mounted from registerWebRoutes() in web/web.cpp.
// ============================================================================
void registerWifiRoutes();

#endif  // PALA_WEB_WIFI_H
