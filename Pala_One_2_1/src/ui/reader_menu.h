#ifndef PALA_UI_READER_MENU_H
#define PALA_UI_READER_MENU_H

#include "src/hal/input.h"  // ButtonEvent

// ============================================================================
//  Reader menu — overlay shown over the reader screen when the user binds
//  ACTION_MENU to a hold gesture and triggers it.
//
//  Contents (one screen, no scrolling):
//    - Book title
//    - Page / progress line ("Page X of Y (Z%)" or "Page X (Z% of book)"
//      depending on whether pagination has reached EOF)
//    - Statusbar mode row
//    - "Sync progress" row, only when KOReader sync is configured+enabled
//    - Footer hint
//
//  Gestures depend on how many rows there are, so that a device without
//  sync configured behaves exactly as it did before sync existed:
//
//    one row  (sync off) — 1x cycles the statusbar, anything else closes
//    two rows (sync on)  — 1x moves the selection, 2x activates it,
//                          anything else closes
//
//  ReaderScreen owns the gesture dispatch; the menu module owns its own
//  visible/hidden state and drawing. While the menu is active, the
//  reader's normal page-turn input is suppressed.
// ============================================================================
namespace ReaderMenu {

// Is the menu currently overlaying the reader?
bool isActive();

// Make the menu visible and draw it.
void open();

// Hide the menu. Caller is responsible for the next render of the
// underlying reader page.
void close();

// Handle one button event while the menu is visible. Returns true iff the
// event was consumed by the menu (caller should NOT pass it to the reader).
// See the gesture table above; any unhandled click closes the menu (caller
// should re-render the reader page). Selecting "Sync progress" closes the
// menu and sets `g_readerScreen.nextScreen`, so the caller's post-close
// re-render is skipped by the screen switch in loop().
bool onButton(const ButtonEvent& ev);

// Re-render the menu — used after an in-menu state change (statusbar mode
// cycle) to repaint the row.
void draw();

}  // namespace ReaderMenu

#endif  // PALA_UI_READER_MENU_H
