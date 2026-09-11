#ifndef PALA_UI_SCREENS_SYNC_SCREEN_H
#define PALA_UI_SCREENS_SYNC_SCREEN_H

#include <Arduino.h>

#include "src/hal/wifi.h"
#include "src/pure/kosync_codec.h"   // KosyncRemote
#include "src/ui/screen.h"

// ============================================================================
//  KOReader progress sync for the currently-open book.
//
//  Reached from the reader menu (never from the library), because it acts on
//  `g_bookview` and returns straight back to the page you were reading. The
//  book stays open across the whole session — this screen deliberately does
//  NOT call resetBookView().
//
//  Structure mirrors UpdateScreen: a phase machine, non-blocking Wi-Fi
//  bring-up driven from onIdleTick(), and blocking HTTPS work done inside a
//  button handler with the "working" phase drawn first.
//
//  Sync is always user-initiated. Nothing here runs in the background, and
//  the radio is torn down before the screen hands control back.
// ============================================================================
class SyncScreen : public Screen {
public:
  void onEnter() override;
  void onButton(const ButtonEvent& e) override;
  void draw() override;
  void onIdleTick() override;
  void onSleep() override;

  // A sleeping device cannot hold the Wi-Fi session up. Note this means the
  // device stays awake at the conflict prompt until the user answers it or
  // walks it back with 3x — same trade-off UpdateScreen makes while its own
  // session is live.
  bool allowSleep() const override { return !wifiStarted_; }

private:
  enum class Phase {
    NotConfigured,   // sync not set up in the web UI
    NoDocId,         // this book has no KOReader document identifier
    NoCreds,         // no Wi-Fi credentials stored
    Connecting,      // STA association in flight
    ConnFailed,
    Syncing,         // pull (+ push) running; drawn, then blocks
    Pushed,          // positions agreed, ours is on the server
    Conflict,        // remote differs — waiting for the user to choose
    Jumped,          // adopted the remote position
    Failed           // HTTP / auth error; message_ says which
  };

  Phase       phase_       = Phase::Connecting;
  uint32_t    staStartMs_  = 0;
  WifiSession net_;
  bool        wifiStarted_ = false;

  // Conflict state, as percentages of the book.
  float  localPct_  = 0.0f;
  float  remotePct_ = 0.0f;

  // Set when the remote XPointer resolved through this book's spine map. The
  // byte offset is exact where the percentage is only proportional, so it
  // wins for both the prompt and the jump.
  bool     remoteOffsetValid_ = false;
  uint32_t remoteOffset_      = 0;

  // Set when the jump could not actually reach the remote position — the
  // page table ran out before it got there. The reader still moves as far as
  // it can, but the position is not published back. See applyRemotePosition.
  bool     jumpedShort_       = false;

  String remoteDevice_;
  int    focusItem_ = 0;      // 0 = jump to remote, 1 = keep local

  String docHex_;             // this book's document id
  String message_;            // failure detail for the Failed phase

  void   runSync();           // blocking: pull, decide, maybe push
  void   applyRemotePosition();
  void   pushLocal();
  void   teardownWifi();
  void   exitToReader();

  // Turn the remote XPointer into a byte offset via the book's spine map,
  // refining remotePct_ when it succeeds. False leaves the percentage model
  // in charge.
  bool   resolveRemoteOffset(const KosyncRemote& remote);

  // Where the reader currently is, as a fraction of the file.
  float  currentLocalPct() const;

  // ...and as an XPointer, when the spine map can produce one. Empty string
  // otherwise, which makes the push fall back to the percentage.
  String localXPointer() const;
};

extern SyncScreen g_syncScreen;

#endif  // PALA_UI_SCREENS_SYNC_SCREEN_H
