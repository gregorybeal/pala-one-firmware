#include "src/web/kosync.h"

#include "src/config.h"
#include "src/state.h"
#include "src/hal/kosync.h"
#include "src/pure/hashing.h"              // prefKeyForBook
#include "src/pure/kosync_codec.h"
#include "src/pure/paths.h"                // sanitizeUploadedFilename
#include "src/storage/book_metadata.h"
#include "src/storage/kosync_settings.h"
#include "src/storage/library.h"           // g_library
#include "src/storage/preferences_store.h"
#include "src/storage/sync_map.h"
#include "src/storage/wifi_creds.h"
#include "src/web/chrome.h"

// ----------------------------------------------------------------------------
//  Helpers
// ----------------------------------------------------------------------------
static String docHexForPath(const String& path) {
  PreferencesStore kv(prefs);
  uint8_t doc[KOSYNC_DOC_BYTES];
  loadKosyncDoc(kv, prefKeyForBook(path), doc);
  if (!kosyncDocIsSet(doc)) return String("");
  return kosyncDocToHex(doc);
}

// Turn a call outcome into something a person can act on. The raw status is
// appended for the cases where it carries information the label doesn't.
static String resultMessage(const Kosync::CallResult& r) {
  switch (r.result) {
    case Kosync::Result::Ok:            return String(D_WEB_KS_MSG_OK);
    case Kosync::Result::NotConfigured: return String(D_WEB_KS_MSG_NOT_CONFIGURED);
    case Kosync::Result::NoNetwork:     return String(D_WEB_KS_MSG_NO_NETWORK);
    case Kosync::Result::Unauthorized:  return String(D_WEB_KS_MSG_UNAUTHORIZED);
    case Kosync::Result::NotFound:      return String(D_WEB_KS_MSG_NOT_FOUND);
    case Kosync::Result::Conflict:      return String(D_WEB_KS_MSG_TAKEN);
    case Kosync::Result::ServerError:
    default:
      return String(D_WEB_KS_MSG_SERVER_ERROR) + " (" + String(r.httpCode) + ")";
  }
}

// ----------------------------------------------------------------------------
//  GET /kosync
// ----------------------------------------------------------------------------
static void renderPage(const String& banner, bool bannerIsError) {
  String out = webPageStart(
    D_WEB_KS_TITLE,
    D_WEB_KS_SUBTITLE,
    "<a href='/'>" D_WEB_NAV_HOME "</a><a href='/files'>" D_WEB_NAV_FILES "</a><a href='/settings'>" D_WEB_NAV_SETTINGS "</a><a href='/wifi'>" D_WEB_NAV_WIFI "</a>"
  );
  out.reserve(out.length() + 4000);

  if (banner.length() > 0) {
    out += bannerIsError ? "<div class='banner-warn'>" : "<div class='banner-ok'>";
    out += htmlEscape(banner);
    out += "</div>";
  }

  // Syncing needs the radio, and the radio needs stored credentials. Say so
  // up front rather than letting the device fail at the reader menu.
  if (!WifiCreds::has()) {
    out += "<div class='banner-warn'>" D_WEB_KS_NO_WIFI
           " <a class='link' href='/wifi'>" D_WEB_KS_NO_WIFI_LINK "</a></div>";
  }

  // --- account ---
  out += "<div class='card'><h2>" D_WEB_KS_ACCOUNT_HEADING "</h2>";
  out += "<p class='muted'>" D_WEB_KS_ACCOUNT_INTRO "</p>";
  out += "<form method='POST' action='/kosync' accept-charset='UTF-8' style='margin-top:12px'>";

  out += "<div><label for='url'>" D_WEB_KS_SERVER_LABEL "</label>";
  out += "<input type='text' id='url' name='url' maxlength='120' value='";
  out += htmlEscape(Kosync::serverUrl());
  out += "' placeholder='https://sync.koreader.rocks'>";
  out += "<div class='hint'>" D_WEB_KS_SERVER_HINT "</div></div>";

  out += "<div style='margin-top:10px'><label for='user'>" D_WEB_KS_USER_LABEL "</label>";
  out += "<input type='text' id='user' name='user' maxlength='64' autocomplete='username' value='";
  out += htmlEscape(Kosync::username());
  out += "'></div>";

  out += "<div style='margin-top:10px'><label for='pass'>" D_WEB_KS_PASS_LABEL "</label>";
  out += "<input type='password' id='pass' name='pass' maxlength='64' autocomplete='current-password' placeholder='";
  out += (Kosync::configured() ? String(D_WEB_KS_PASS_KEEP) : String(""));
  out += "'>";
  out += "<div class='hint'>" D_WEB_KS_PASS_HINT "</div></div>";

  out += "<label style='display:flex;gap:8px;align-items:center;margin-top:12px;cursor:pointer'>";
  out += "<input type='checkbox' name='on' value='1' style='width:auto'";
  out += (Kosync::enabled() ? " checked" : "");
  out += "><span>" D_WEB_KS_ENABLE_LABEL "</span></label>";
  out += "<div class='hint'>" D_WEB_KS_ENABLE_HINT "</div>";

  out += "<div class='actions' style='margin-top:14px'>";
  out += "<button type='submit' name='do' value='save'>" D_WEB_KS_SAVE_BUTTON "</button>";
  out += "<button type='submit' name='do' value='test' class='btn secondary'>" D_WEB_KS_TEST_BUTTON "</button>";
  out += "<button type='submit' name='do' value='register' class='btn secondary'>" D_WEB_KS_REGISTER_BUTTON "</button>";
  out += "</div>";
  out += "<div class='hint'>" D_WEB_KS_REGISTER_HINT "</div>";
  out += "</form>";

  if (Kosync::configured()) {
    out += "<form method='POST' action='/kosync' style='margin-top:12px'>";
    out += "<input type='hidden' name='do' value='clear'>";
    out += "<button type='submit' class='btn secondary' onclick=\"return confirm('" D_WEB_KS_CONFIRM_CLEAR "')\">";
    out += D_WEB_KS_CLEAR_BUTTON "</button></form>";
  }
  out += "</div>";

  // --- per-book document ids ---
  out += "<div class='card'><h2>" D_WEB_KS_DOCS_HEADING "</h2>";
  out += "<p class='muted'>" D_WEB_KS_DOCS_INTRO "</p>";

  if (g_library.bookCount == 0) {
    out += "<p class='muted'>" D_WEB_KS_DOCS_EMPTY "</p>";
  } else {
    out += "<ul class='list'>";
    for (int i = 0; i < g_library.bookCount; i++) {
      String path = String(g_library.books[i].path);
      String hex  = docHexForPath(path);

      out += "<li><div><h3>";
      out += htmlEscape(String(g_library.books[i].name));
      out += "</h3><div class='meta'>";
      out += (hex.length() ? String(D_WEB_KS_DOC_SET) : String(D_WEB_KS_DOC_UNSET));
      out += "</div>";
      out += "<form method='POST' action='/kosync-book' accept-charset='UTF-8' style='margin-top:8px'>";
      out += "<input type='hidden' name='id' value='" + String(i) + "'>";
      out += "<div class='row' style='align-items:end;gap:10px'><div style='flex:1'>";
      out += "<input type='text' name='md5' value='" + htmlEscape(hex) + "' maxlength='32' "
             "placeholder='" D_WEB_KS_DOC_PLACEHOLDER "' spellcheck='false'>";
      out += "</div><div><button type='submit'>" D_WEB_KS_DOC_SAVE_BUTTON "</button></div></div>";
      out += "<div class='muted'>" D_WEB_KS_DOC_HINT "</div>";
      out += "</form></div></li>";
    }
    out += "</ul>";
  }
  out += "</div>";

  out += webPageEnd();
  server.send(200, "text/html; charset=utf-8", out);
}

static void handleKosyncGet() {
  renderPage("", false);
}

// ----------------------------------------------------------------------------
//  POST /kosync
// ----------------------------------------------------------------------------
static void handleKosyncPost() {
  String action = server.hasArg("do") ? server.arg("do") : String("save");

  if (action == "clear") {
    Kosync::clearAccount();
    renderPage(D_WEB_KS_MSG_CLEARED, false);
    return;
  }

  // Save first for every other action, so "Test" and "Register" operate on
  // what the user is looking at rather than on the previously stored values.
  if (server.hasArg("url")) Kosync::setServerUrl(server.arg("url"));

  String user = server.hasArg("user") ? server.arg("user") : String("");
  String pass = server.hasArg("pass") ? server.arg("pass") : String("");
  user.trim();

  if (action == "register") {
    if (user.length() == 0 || pass.length() == 0) {
      renderPage(D_WEB_KS_MSG_NEED_BOTH, true);
      return;
    }
    Kosync::CallResult r = Kosync::registerUser(user, pass);
    if (r.result == Kosync::Result::Ok) {
      // Registration succeeded — adopt the credentials so the user does not
      // have to type them a second time.
      Kosync::setAccount(user, Kosync::md5Hex(pass));
      Kosync::setEnabled(true);
      renderPage(D_WEB_KS_MSG_REGISTERED, false);
    } else {
      renderPage(resultMessage(r), true);
    }
    return;
  }

  // An empty password field means "keep the stored key" — the form never
  // round-trips the digest, so blanking it must not wipe the account.
  if (user.length() > 0 && pass.length() > 0) {
    Kosync::setAccount(user, Kosync::md5Hex(pass));
  } else if (user.length() > 0 && Kosync::authKey().length() > 0) {
    Kosync::setAccount(user, Kosync::authKey());
  }

  Kosync::setEnabled(server.hasArg("on") && Kosync::configured());

  if (action == "test") {
    if (!Kosync::configured()) {
      renderPage(D_WEB_KS_MSG_NEED_BOTH, true);
      return;
    }
    Kosync::CallResult r = Kosync::authenticate();
    renderPage(resultMessage(r), r.result != Kosync::Result::Ok);
    return;
  }

  renderPage(D_WEB_KS_MSG_SAVED, false);
}

// ----------------------------------------------------------------------------
//  POST /kosync-doc — called by the upload page's script right after a book
//  lands. Body: name=<filename as submitted>&md5=<32 hex>.
// ----------------------------------------------------------------------------
static void handleKosyncDoc() {
  if (!server.hasArg("name") || !server.hasArg("md5")) {
    server.send(400, "text/plain; charset=utf-8", D_WEB_ERR_MISSING_NAME);
    return;
  }

  uint8_t doc[KOSYNC_DOC_BYTES];
  if (!kosyncDocFromHex(server.arg("md5"), doc)) {
    server.send(400, "text/plain; charset=utf-8", D_WEB_KS_ERR_BAD_HASH);
    return;
  }

  // Re-run the same sanitizer the upload path used, so the name the browser
  // reports resolves to the path the device actually stored.
  String path = "/books/" + sanitizeUploadedFilename(server.arg("name"));
  if (!FS.exists(path)) {
    server.send(404, "text/plain; charset=utf-8", D_WEB_ERR_MISSING_BOOK);
    return;
  }

  PreferencesStore kv(prefs);
  saveKosyncDoc(kv, prefKeyForBook(path), doc);
  server.send(200, "text/plain; charset=utf-8", "ok");
}

// ----------------------------------------------------------------------------
//  POST /kosync-book — manual per-book edit from the table above.
// ----------------------------------------------------------------------------
static void handleKosyncBook() {
  if (!server.hasArg("id")) {
    server.send(400, "text/plain; charset=utf-8", D_WEB_ERR_MISSING_ID);
    return;
  }
  int idx = server.arg("id").toInt();
  const char* path = bookPath(idx);
  if (!path) {
    server.send(400, "text/plain; charset=utf-8", D_WEB_ERR_BAD_ID);
    return;
  }

  String hex = server.hasArg("md5") ? server.arg("md5") : String("");
  hex.trim();

  PreferencesStore kv(prefs);
  String bookKey = prefKeyForBook(String(path));

  if (hex.length() == 0) {
    clearKosyncDoc(kv, bookKey);
    renderPage(D_WEB_KS_MSG_DOC_CLEARED, false);
    return;
  }

  uint8_t doc[KOSYNC_DOC_BYTES];
  if (!kosyncDocFromHex(hex, doc)) {
    renderPage(D_WEB_KS_ERR_BAD_HASH, true);
    return;
  }
  saveKosyncDoc(kv, bookKey, doc);
  renderPage(D_WEB_KS_MSG_DOC_SAVED, false);
}

// ----------------------------------------------------------------------------
//  POST /kosync-map — the spine map for a book that has just been uploaded,
//  streamed as a file part whose filename is the book's stored name.
//
//  Written to a temp file and validated before it replaces whatever map the
//  book already had, so a half-transferred or malformed build can never
//  become the live map. Like /kosync-doc this is best-effort from the
//  browser's point of view: the book is already stored, and a book with no
//  map still syncs, just by percentage.
// ----------------------------------------------------------------------------
namespace {
struct MapUpload {
  File   tmp;
  String tmpPath;
  String finalPath;
  String error;
};
MapUpload s_mapUp;
}  // namespace

static void discardMapUpload(MapUpload& s) {
  if (s.tmp) s.tmp.close();
  if (s.tmpPath.length() > 0 && FS.exists(s.tmpPath)) FS.remove(s.tmpPath);
  s.tmpPath = "";
}

// Read the header back off disk and run it through the same parser the
// reader uses, so what we accept and what SyncMap::open will accept cannot
// drift apart.
static bool storedMapIsValid(const String& path, uint32_t expectedTextSize) {
  File f = FS.open(path, "r");
  if (!f) return false;

  uint8_t hdr[SYNC_MAP_HEADER_BYTES];
  if (f.read(hdr, sizeof(hdr)) != sizeof(hdr)) { f.close(); return false; }

  SyncMapHeader h;
  bool ok = parseSyncMapHeader(hdr, sizeof(hdr), h)
            && h.textSize == expectedTextSize
            && (size_t)f.size() >= syncMapTotalBytes(h);
  f.close();
  return ok;
}

static void handleKosyncMapStream() {
  MapUpload& s = s_mapUp;
  HTTPUpload& up = server.upload();

  if (up.status == UPLOAD_FILE_START) {
    s.error     = "";
    s.tmpPath   = "";
    s.finalPath = "";

    // Same sanitizer the upload path used, so the name the browser reports
    // resolves to the path the device actually stored.
    String book = "/books/" + sanitizeUploadedFilename(up.filename);
    if (!FS.exists(book)) { s.error = D_WEB_ERR_MISSING_BOOK; return; }

    s.finalPath = syncMapPathForBook(book);
    s.tmpPath   = s.finalPath + ".tmp";
    if (FS.exists(s.tmpPath)) FS.remove(s.tmpPath);

    s.tmp = FS.open(s.tmpPath, "w");
    if (!s.tmp) {
      s.error   = D_WEB_ERR_CANT_CREATE_TEMP_BOOK;
      s.tmpPath = "";
    }
  }
  else if (up.status == UPLOAD_FILE_WRITE) {
    if (s.error.length() > 0 || !s.tmp) return;

    if (s.tmp.size() + up.currentSize > SYNC_MAP_MAX_FILE_BYTES) {
      discardMapUpload(s);
      s.error = D_WEB_ERR_NOT_ENOUGH_SPACE;
      return;
    }
    if (s.tmp.write(up.buf, up.currentSize) != up.currentSize) {
      discardMapUpload(s);
      s.error = D_WEB_ERR_WRITE_FAILED;
    }
  }
  else if (up.status == UPLOAD_FILE_END) {
    if (s.tmp) s.tmp.close();
    if (s.error.length() > 0 || s.tmpPath.length() == 0) return;

    String book = "/books/" + sanitizeUploadedFilename(up.filename);
    File bf = FS.open(book, "r");
    uint32_t textSize = bf ? (uint32_t)bf.size() : 0;
    if (bf) bf.close();

    if (textSize == 0 || !storedMapIsValid(s.tmpPath, textSize)) {
      discardMapUpload(s);
      s.error = D_WEB_KS_ERR_BAD_MAP;
      return;
    }

    if (FS.exists(s.finalPath)) FS.remove(s.finalPath);
    if (!FS.rename(s.tmpPath, s.finalPath)) {
      discardMapUpload(s);
      s.error = D_WEB_ERR_FINALIZE_UPLOAD;
      return;
    }
    s.tmpPath = "";
  }
  else if (up.status == UPLOAD_FILE_ABORTED) {
    discardMapUpload(s);
    s.error = D_WEB_ERR_UPLOAD_ABORTED;
  }
}

static void handleKosyncMapDone() {
  if (s_mapUp.error.length() > 0) {
    server.send(400, "text/plain; charset=utf-8", s_mapUp.error);
    return;
  }
  server.send(200, "text/plain; charset=utf-8", "ok");
}

// ----------------------------------------------------------------------------
void registerKosyncRoutes() {
  server.on("/kosync",      HTTP_GET,  handleKosyncGet);
  server.on("/kosync",      HTTP_POST, handleKosyncPost);
  server.on("/kosync-doc",  HTTP_POST, handleKosyncDoc);
  server.on("/kosync-book", HTTP_POST, handleKosyncBook);
  server.on("/kosync-map",  HTTP_POST, handleKosyncMapDone, handleKosyncMapStream);
}
