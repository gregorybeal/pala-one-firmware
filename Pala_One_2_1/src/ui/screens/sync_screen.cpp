#include "src/ui/screens/sync_screen.h"

#include "src/config.h"
#include "src/hal/display.h"
#include "src/hal/input.h"
#include "src/hal/kosync.h"
#include "src/hal/wifi.h"
#include "src/hal/wifi_provisioning.h"
#include "src/pure/hashing.h"              // prefKeyForBook
#include "src/pure/kosync_codec.h"
#include "src/pure/paths.h"                // bookLeafLabel
#include "src/pure/xpointer.h"
#include "src/state.h"
#include "src/storage/book_metadata.h"
#include "src/storage/kosync_settings.h"
#include "src/storage/preferences_store.h"
#include "src/storage/sync_map.h"
#include "src/storage/wifi_creds.h"
#include "src/ui/font.h"
#include "src/ui/reader.h"                 // g_bookview, findPageForOffset
#include "src/ui/screens/library_screen.h"
#include "src/ui/screens/reader_screen.h"
#include "src/ui/widgets.h"


// ----------------------------------------------------------------------------
//  Helpers
// ----------------------------------------------------------------------------
static int pctInt(float pct) {
  int v = (int)(pct * 100.0f + 0.5f);
  if (v < 0)   v = 0;
  if (v > 100) v = 100;
  return v;
}

float SyncScreen::currentLocalPct() const {
  if (!g_bookview.book.isOpen()) return 0.0f;
  const PageOffsetTable& pages = g_bookview.pages;
  int idx = g_bookview.cursor.pageIndex;
  if (idx < 0 || idx >= pages.count) return 0.0f;
  // Same numerator the status bar uses, so what the screen reports matches
  // what the reader shows. (g_bookview is a global — this method being const
  // says nothing about it.)
  return percentageForOffset(pages.offsets[idx],
                             (uint32_t)g_bookview.book.size());
}

String SyncScreen::localXPointer() const {
  if (!g_bookview.book.isOpen()) return String("");

  const PageOffsetTable& pages = g_bookview.pages;
  int idx = g_bookview.cursor.pageIndex;
  if (idx < 0 || idx >= pages.count) return String("");

  SyncMap map;
  if (!map.open(g_bookview.book.path(), (uint32_t)g_bookview.book.size())) {
    return String("");
  }
  return map.xpointerForOffset(pages.offsets[idx]);
}

void SyncScreen::teardownWifi() {
  if (!wifiStarted_) return;
  wifiEnd();
  WifiProvisioning::notifyUploadSession(false);
  wifiStarted_ = false;
}

void SyncScreen::exitToReader() {
  teardownWifi();
  if (g_bookview.book.isOpen()) {
    // This screen painted over the page; a partial refresh would ghost.
    forceNextRenderFull();
    nextScreen = &g_readerScreen;
  } else {
    nextScreen = &g_libraryScreen;
  }
}

// ----------------------------------------------------------------------------
//  Lifecycle
// ----------------------------------------------------------------------------
void SyncScreen::onEnter() {
  focusItem_    = 0;
  wifiStarted_  = false;
  remoteDevice_ = "";
  message_      = "";
  localPct_     = currentLocalPct();
  remotePct_    = 0.0f;
  remoteOffset_ = 0;
  remoteOffsetValid_ = false;
  jumpedShort_  = false;
  docHex_       = "";

  if (!g_bookview.book.isOpen()) {
    // Nothing to sync against — the reader menu should never let this
    // happen, but the screen must not act on a stale book either way.
    phase_ = Phase::NotConfigured;
    draw();
    return;
  }

  if (!Kosync::enabled()) {
    phase_ = Phase::NotConfigured;
    draw();
    return;
  }

  PreferencesStore kv(prefs);
  uint8_t doc[KOSYNC_DOC_BYTES];
  loadKosyncDoc(kv, g_bookview.book.key(), doc);
  if (!kosyncDocIsSet(doc)) {
    phase_ = Phase::NoDocId;
    draw();
    return;
  }
  docHex_ = kosyncDocToHex(doc);

  if (!WifiCreds::has()) {
    phase_ = Phase::NoCreds;
  } else if (wifiStaBegin()) {
    wifiStarted_ = true;
    WifiProvisioning::notifyUploadSession(true);
    phase_      = Phase::Connecting;
    staStartMs_ = millis();
  } else {
    phase_ = Phase::ConnFailed;
  }
  draw();
}

void SyncScreen::onIdleTick() {
  if (phase_ != Phase::Connecting) return;

  WifiStaResult r = wifiStaPoll(net_);
  if (r == WifiStaResult::Connected) {
    phase_ = Phase::Syncing;
    draw();               // paint "Syncing..." before the blocking calls
    runSync();
    clearButtonQueue();   // discard presses that piled up while blocked
    draw();
    return;
  }
  if (r == WifiStaResult::Failed ||
      (uint32_t)(millis() - staStartMs_) > wifiStaBudgetMs()) {
    teardownWifi();
    phase_ = Phase::ConnFailed;
    draw();
  }
}

// ----------------------------------------------------------------------------
//  The sync itself
// ----------------------------------------------------------------------------
void SyncScreen::runSync() {
  localPct_ = currentLocalPct();

  KosyncRemote remote;
  Kosync::CallResult r = Kosync::pull(docHex_, remote);

  if (r.result == Kosync::Result::NotFound) {
    // The server has never seen this book. Nothing to reconcile — just
    // publish where we are.
    pushLocal();
    return;
  }
  if (r.result != Kosync::Result::Ok || !remote.valid) {
    phase_   = Phase::Failed;
    message_ = (r.result == Kosync::Result::Unauthorized)
                 ? String(D_SYNC_ERR_AUTH)
                 : String(D_SYNC_ERR_SERVER) + " " + String(r.httpCode);
    return;
  }

  remotePct_    = remote.percentage;
  remoteDevice_ = remote.device;

  // Prefer the structural position when the book's spine map can resolve the
  // XPointer KOReader sent. This both removes the front-matter bias and
  // makes the percentages on screen agree with where the jump will land.
  resolveRemoteOffset(remote);

  if (decideSync(localPct_, remotePct_) == SYNC_IDENTICAL) {
    // Already in step. Push anyway so the server records this device as the
    // most recent reader.
    pushLocal();
    return;
  }

  phase_     = Phase::Conflict;
  focusItem_ = 0;
}

bool SyncScreen::resolveRemoteOffset(const KosyncRemote& remote) {
  remoteOffsetValid_ = false;
  remoteOffset_      = 0;

  // Diagnostic record for the web UI, written on every path out of here so a
  // failed resolve is as visible as a successful one. Defaults describe the
  // percentage-only case; the branches below refine it.
  Kosync::LastPull diag;
  diag.present   = true;
  diag.book      = g_bookview.book.isOpen()
                     ? bookLeafLabel(g_bookview.book.path())
                     : String("");
  diag.pointer   = remote.progress;
  diag.remotePct = remote.percentage;
  diag.outcome   = Kosync::LAST_PULL_NO_POINTER;
  diag.landedPct = remote.percentage;

  if (!g_bookview.book.isOpen()) { Kosync::recordLastPull(diag); return false; }

  XPointer xp;
  if (!parseXPointer(remote.progress, xp)) {
    // A percentage string, not a pointer — KOReader sends one for paged
    // documents, and so did this firmware's own older push path.
    Kosync::recordLastPull(diag);
    return false;
  }

  uint32_t size = (uint32_t)g_bookview.book.size();
  if (size == 0) { Kosync::recordLastPull(diag); return false; }

  // Past this point the pointer parsed, so anything that still fails to
  // resolve is a rejection rather than an absent pointer.
  diag.outcome = Kosync::LAST_PULL_REJECTED;

  SyncMap map;
  if (!map.open(g_bookview.book.path(), size)) {
    // No spine map, or one stamped for different text: percentage fallback,
    // and the numbering setting has nothing to act on. Distinct from a
    // rejection because the fix is different — re-upload the book.
    diag.outcome = Kosync::LAST_PULL_NO_MAP;
    Kosync::recordLastPull(diag);
    return false;
  }

  // crengine numbers DocFragments over the spine; whether it counts itemrefs
  // marked linear="no" is not knowable from here. It is knowable from the
  // other end, though — it is a property of the KOReader build, identical for
  // every book — so when the user has pinned it we resolve that way and only
  // that way.
  //
  // On Auto we resolve both and keep whichever lands closer to the percentage
  // the server sent. Note what that referee is: KOReader's percentage counts
  // content the flattener drops, so it reads high, and a candidate that lands
  // too far into the book can score better than the correct one. It picks
  // right on a book with little front matter and can pick wrong on a book
  // with a lot. Kosync::FragmentNumbering is the way out of the guess.
  KosyncFragmentNumbering want[KOSYNC_MAX_FRAGMENT_CANDIDATES];
  int wantCount = kosyncNumberingsToTry(Kosync::fragmentNumbering(), want);

  // Resolve each numbering the setting asked for, then let the pure referee
  // pick. Candidates that the map could not resolve at all are dropped here
  // rather than passed along, so the indices the referee returns address
  // `offsets`, not `want`.
  uint32_t offsets[KOSYNC_MAX_FRAGMENT_CANDIDATES];
  float    pcts[KOSYNC_MAX_FRAGMENT_CANDIDATES];
  KosyncFragmentNumbering used[KOSYNC_MAX_FRAGMENT_CANDIDATES];
  int      count = 0;

  for (int i = 0; i < wantCount; i++) {
    SyncMap::FragmentMode mode = (want[i] == KOSYNC_FRAG_LINEAR)
                                   ? SyncMap::FragmentMode::Linear
                                   : SyncMap::FragmentMode::Opf;
    uint32_t off = 0;
    if (!map.offsetForXPointer(xp, mode, off)) continue;
    offsets[count] = off;
    pcts[count]    = percentageForOffset(off, size);
    used[count]    = want[i];
    count++;
  }

  int pick = kosyncChooseFragment(pcts, count, remote.percentage);
  if (pick < 0) {
    Kosync::recordLastPull(diag);
    return false;
  }

  remoteOffset_      = offsets[pick];
  remoteOffsetValid_ = true;
  remotePct_         = pcts[pick];

  diag.outcome   = (used[pick] == KOSYNC_FRAG_LINEAR) ? Kosync::LAST_PULL_LINEAR
                                                      : Kosync::LAST_PULL_OPF;
  diag.landedPct = pcts[pick];
  Kosync::recordLastPull(diag);
  return true;
}

void SyncScreen::pushLocal() {
  Kosync::CallResult r = Kosync::push(docHex_, localPct_, localXPointer());
  if (r.result == Kosync::Result::Ok) {
    phase_ = Phase::Pushed;
  } else {
    phase_   = Phase::Failed;
    message_ = (r.result == Kosync::Result::Unauthorized)
                 ? String(D_SYNC_ERR_AUTH)
                 : String(D_SYNC_ERR_SERVER) + " " + String(r.httpCode);
  }
}

void SyncScreen::applyRemotePosition() {
  if (!g_bookview.book.isOpen()) return;

  uint32_t size = (uint32_t)g_bookview.book.size();

  // A resolved XPointer names a paragraph outright. Only when there is no
  // map, or it could not be trusted, do we fall back to scaling the
  // percentage into the file — the approximation this feature exists to
  // avoid. See resolveRemoteOffset.
  uint32_t target = remoteOffsetValid_
                      ? remoteOffset_
                      : offsetForPercentage(remotePct_, size);
  if (size > 0 && target >= size) target = size - 1;

  // Either way the target lands mid-page; snap to the page that contains it
  // so the reader has a real boundary to render from. This paginates forward
  // as needed and can take a moment on a long book.
  int page = findPageForOffset(target);
  if (page < 0) page = 0;
  g_bookview.cursor.pageIndex = page;

  // findPageForOffset extends the page table until it covers `target`, but it
  // gives up quietly when it cannot: MAX_PAGES, or a paginator that stopped.
  // It then returns the furthest page it does have, which is nowhere near
  // where the other device is.
  //
  // The target was genuinely reached only if a later page exists (so `target`
  // falls inside `page`) or pagination walked all the way to EOF. Anything
  // else means we landed short.
  const PageOffsetTable& pages = g_bookview.pages;
  jumpedShort_ = !((page + 1 < pages.count) || pages.eofReached);

  // Persist through the same path a normal page turn uses, so the position
  // survives a reboot and the on-disk page cache stays current.
  persistReaderState();

  // Our position is now the remote one; tell the server this device is here
  // too, so a later sync from a third device sees a consistent answer.
  //
  // Not when we landed short, though: publishing a position the reader never
  // reached would overwrite the other device's correct one with a truncated
  // guess, and the next sync from there would silently pull the book
  // backwards. Leaving the server alone keeps the real position recoverable.
  localPct_ = currentLocalPct();
  if (!jumpedShort_) {
    Kosync::push(docHex_, localPct_, localXPointer());
  }

  phase_ = Phase::Jumped;
}

// ----------------------------------------------------------------------------
//  Drawing
// ----------------------------------------------------------------------------
void SyncScreen::draw() {
  prepareMenuFrame();
  Font::useBody();
  int ascent = u8g2.getFontAscent();
  int lineH  = (ascent - u8g2.getFontDescent()) + Font::currentLineGap() + 1;
  int y = drawSectionHeader(D_SYNC_HEADER);

  char buf[64];

  switch (phase_) {
    case Phase::NotConfigured:
      u8g2.setCursor(MARGIN_X, y);
      u8g2.print(D_SYNC_NOT_CONFIGURED_L1);
      y += lineH;
      u8g2.setCursor(MARGIN_X, y);
      u8g2.print(D_SYNC_NOT_CONFIGURED_L2);
      break;

    case Phase::NoDocId:
      u8g2.setCursor(MARGIN_X, y);
      u8g2.print(D_SYNC_NO_DOC_L1);
      y += lineH;
      u8g2.setCursor(MARGIN_X, y);
      u8g2.print(D_SYNC_NO_DOC_L2);
      break;

    case Phase::NoCreds:
      u8g2.setCursor(MARGIN_X, y);
      u8g2.print(D_SYNC_NO_CREDS);
      break;

    case Phase::Connecting:
      u8g2.setCursor(MARGIN_X, y);
      u8g2.print(D_SYNC_CONNECTING);
      break;

    case Phase::ConnFailed:
      u8g2.setCursor(MARGIN_X, y);
      u8g2.print(D_SYNC_CONN_FAILED);
      break;

    case Phase::Syncing:
      u8g2.setCursor(MARGIN_X, y);
      u8g2.print(D_SYNC_WORKING);
      break;

    case Phase::Pushed:
      Font::useBold();
      u8g2.setCursor(MARGIN_X, y);
      u8g2.print(D_SYNC_UP_TO_DATE);
      Font::useBody();
      y += lineH + 2;
      snprintf(buf, sizeof(buf), D_SYNC_HERE_FMT, pctInt(localPct_));
      u8g2.setCursor(MARGIN_X, y);
      u8g2.print(buf);
      break;

    case Phase::Conflict: {
      // Two positions and two choices. 1x moves, 2x selects, 3x leaves —
      // the same grammar every other menu on the device uses.
      snprintf(buf, sizeof(buf), D_SYNC_OTHER_FMT, pctInt(remotePct_));
      u8g2.setCursor(MARGIN_X, y);
      u8g2.print(buf);
      if (remoteDevice_.length() > 0) {
        int w = u8g2.getUTF8Width(buf);
        u8g2.setCursor(MARGIN_X + w + 4, y);
        u8g2.print(remoteDevice_.c_str());
      }
      y += lineH;

      snprintf(buf, sizeof(buf), D_SYNC_HERE_FMT, pctInt(localPct_));
      u8g2.setCursor(MARGIN_X, y);
      u8g2.print(buf);
      y += lineH + 3;

      if (focusItem_ == 0) Font::useBold(); else Font::useBody();
      u8g2.setCursor(MARGIN_X, y);
      u8g2.print(D_SYNC_ACTION_JUMP);
      y += lineH;

      if (focusItem_ == 1) Font::useBold(); else Font::useBody();
      u8g2.setCursor(MARGIN_X, y);
      u8g2.print(D_SYNC_ACTION_KEEP);
      Font::useBody();
      break;
    }

    case Phase::Jumped:
      Font::useBold();
      u8g2.setCursor(MARGIN_X, y);
      u8g2.print(D_SYNC_JUMPED);
      Font::useBody();
      y += lineH + 2;
      snprintf(buf, sizeof(buf), D_SYNC_HERE_FMT, pctInt(localPct_));
      u8g2.setCursor(MARGIN_X, y);
      u8g2.print(buf);
      if (jumpedShort_) {
        // Say both halves: where it stopped, and that the other device was
        // deliberately left alone so its position is still there to go back to.
        y += lineH + 2;
        u8g2.setCursor(MARGIN_X, y);
        u8g2.print(D_SYNC_JUMPED_SHORT_L1);
        y += lineH;
        u8g2.setCursor(MARGIN_X, y);
        u8g2.print(D_SYNC_JUMPED_SHORT_L2);
      }
      break;

    case Phase::Failed:
      u8g2.setCursor(MARGIN_X, y);
      u8g2.print(D_SYNC_FAILED);
      y += lineH;
      u8g2.setCursor(MARGIN_X, y);
      u8g2.print(message_.c_str());
      break;
  }

  // Footer hint — the conflict prompt is the only phase with a choice to make.
  Font::useBody();
  u8g2.setCursor(MARGIN_X, SCREEN_H - 2);
  u8g2.print(phase_ == Phase::Conflict ? D_SYNC_HINT_CHOOSE : D_SYNC_HINT_EXIT);

  display.update();
}

void SyncScreen::onSleep() {
  // Reachable: the phases that never brought Wi-Fi up (NotConfigured,
  // NoDocId, NoCreds) allow sleep, and so do the terminal ones after
  // teardown. The book is still open here, so mirror ReaderScreen::onSleep
  // exactly — otherwise progress since the last throttled save is lost and
  // the next boot lands in the library instead of back on the page.
  if (!g_bookview.book.isOpen()) return;
  persistReaderState();
  armResumeOnWake();        // captures path before resetBookView() drops it
  resetBookView();
}

// ----------------------------------------------------------------------------
//  Input
// ----------------------------------------------------------------------------
void SyncScreen::onButton(const ButtonEvent& e) {
  if (!e.any()) return;

  // 3x always leaves, in every phase — including the conflict prompt, where
  // it means "decide later" and changes nothing on either side.
  if (e.kind == ButtonEvent::Triple) {
    exitToReader();
    return;
  }

  if (phase_ == Phase::Conflict) {
    if (e.kind == ButtonEvent::Short) {
      focusItem_ = (focusItem_ + 1) % 2;
      draw();
      return;
    }
    if (e.kind == ButtonEvent::Double) {
      phase_ = Phase::Syncing;
      draw();                       // the jump can paginate for a while
      if (focusItem_ == 0) applyRemotePosition();
      else                 pushLocal();
      clearButtonQueue();
      draw();
    }
    return;
  }

  // Every other phase is terminal — any press that isn't the exit gesture
  // just leaves too, so the user never has to remember a second one.
  if (phase_ != Phase::Connecting && phase_ != Phase::Syncing) {
    exitToReader();
  }
}
