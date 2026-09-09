#include "src/web/wifi.h"

#include "src/config.h"
#include "src/state.h"
#include "src/pure/wifi_list_codec.h"
#include "src/storage/wifi_creds.h"
#include "src/web/chrome.h"

// ----------------------------------------------------------------------------
//  GET /wifi
// ----------------------------------------------------------------------------
static void renderPage(const String& banner, bool bannerIsError) {
  String out = webPageStart(
    D_WEB_WIFI_TITLE,
    D_WEB_WIFI_SUBTITLE,
    "<a href='/'>" D_WEB_NAV_HOME "</a><a href='/settings'>" D_WEB_NAV_SETTINGS "</a><a href='/kosync'>" D_WEB_NAV_SYNC "</a>"
  );
  out.reserve(out.length() + 3000);

  if (banner.length() > 0) {
    out += bannerIsError ? "<div class='banner-warn'>" : "<div class='banner-ok'>";
    out += htmlEscape(banner);
    out += "</div>";
  }

  // --- saved networks ---
  const WifiList& list = WifiCreds::list();
  const String lastGood = WifiCreds::lastGoodSsid();

  out += "<div class='card'><h2>" D_WEB_WIFI_SAVED_HEADING "</h2>";
  out += "<p class='muted'>" D_WEB_WIFI_SAVED_INTRO "</p>";

  if (list.count == 0) {
    out += "<p class='muted'>" D_WEB_WIFI_NONE "</p>";
  } else {
    out += "<ul class='list'>";
    for (uint8_t i = 0; i < list.count; i++) {
      String ssid = String(list.nets[i].ssid);
      out += "<li><div class='row'><div><h3>";
      out += htmlEscape(ssid);
      out += "</h3><div class='meta'>";
      // Passwords are never rendered back to the page — only whether one is set.
      out += (list.nets[i].pass[0] ? String(D_WEB_WIFI_SECURED)
                                   : String(D_WEB_WIFI_OPEN));
      if (ssid == lastGood) {
        out += D_WEB_HOME_MIDDOT_SEP;
        out += D_WEB_WIFI_LAST_USED;
      }
      out += "</div></div>";
      out += "<form method='POST' action='/wifi-forget'>";
      out += "<input type='hidden' name='idx' value='" + String((int)i) + "'>";
      out += "<button type='submit' class='btn secondary' onclick=\"return confirm('" D_WEB_WIFI_CONFIRM_FORGET "')\">";
      out += D_WEB_WIFI_FORGET_BUTTON "</button></form>";
      out += "</div></li>";
    }
    out += "</ul>";
  }
  out += "</div>";

  // --- add ---
  out += "<div class='card'><h2>" D_WEB_WIFI_ADD_HEADING "</h2>";
  out += "<p class='muted'>" D_WEB_WIFI_ADD_INTRO "</p>";
  out += "<form method='POST' action='/wifi' accept-charset='UTF-8' style='margin-top:12px'>";
  out += "<div><label for='ssid'>" D_WEB_WIFI_SSID_LABEL "</label>";
  out += "<input type='text' id='ssid' name='ssid' maxlength='" + String(MAX_WIFI_SSID) + "' "
         "autocomplete='off' spellcheck='false' required></div>";
  out += "<div style='margin-top:10px'><label for='pass'>" D_WEB_WIFI_PASS_LABEL "</label>";
  out += "<input type='password' id='pass' name='pass' maxlength='" + String(MAX_WIFI_PASS) + "' "
         "autocomplete='new-password'>";
  out += "<div class='hint'>" D_WEB_WIFI_PASS_HINT "</div></div>";
  out += "<div class='actions' style='margin-top:14px'><button type='submit'>" D_WEB_WIFI_ADD_BUTTON "</button></div>";
  out += "</form>";
  out += "<div class='hint'>" D_WEB_WIFI_CAPACITY_HINT "</div>";
  out += "</div>";

  out += webPageEnd();
  server.send(200, "text/html; charset=utf-8", out);
}

static void handleWifiGet() {
  renderPage("", false);
}

// ----------------------------------------------------------------------------
//  POST /wifi — add, or replace a known SSID's password
// ----------------------------------------------------------------------------
static void handleWifiPost() {
  String ssid = server.hasArg("ssid") ? server.arg("ssid") : String("");
  String pass = server.hasArg("pass") ? server.arg("pass") : String("");
  ssid.trim();

  if (ssid.length() == 0) {
    renderPage(D_WEB_WIFI_ERR_NO_SSID, true);
    return;
  }
  if (ssid.length() > (unsigned)MAX_WIFI_SSID) {
    renderPage(D_WEB_WIFI_ERR_SSID_LONG, true);
    return;
  }
  if (pass.length() > (unsigned)MAX_WIFI_PASS) {
    renderPage(D_WEB_WIFI_ERR_PASS_LONG, true);
    return;
  }

  // Whether this replaced an existing entry decides which message to show —
  // read it before the write, while the answer is still true.
  bool replaced = wifiListFind(WifiCreds::list(), ssid.c_str()) >= 0;
  bool wasFull  = WifiCreds::count() >= MAX_WIFI_NETWORKS;

  if (!WifiCreds::add(ssid, pass)) {
    renderPage(D_WEB_WIFI_ERR_NO_SSID, true);
    return;
  }

  if (replaced)      renderPage(D_WEB_WIFI_MSG_UPDATED, false);
  else if (wasFull)  renderPage(D_WEB_WIFI_MSG_ADDED_EVICTED, false);
  else               renderPage(D_WEB_WIFI_MSG_ADDED, false);
}

// ----------------------------------------------------------------------------
//  POST /wifi-forget
// ----------------------------------------------------------------------------
static void handleWifiForget() {
  if (!server.hasArg("idx")) {
    server.send(400, "text/plain; charset=utf-8", D_WEB_ERR_MISSING_ID);
    return;
  }
  if (!WifiCreds::removeAt(server.arg("idx").toInt())) {
    renderPage(D_WEB_ERR_BAD_ID, true);
    return;
  }
  renderPage(D_WEB_WIFI_MSG_FORGOTTEN, false);
}

// ----------------------------------------------------------------------------
void registerWifiRoutes() {
  server.on("/wifi",        HTTP_GET,  handleWifiGet);
  server.on("/wifi",        HTTP_POST, handleWifiPost);
  server.on("/wifi-forget", HTTP_POST, handleWifiForget);   // POST: destructive
}
