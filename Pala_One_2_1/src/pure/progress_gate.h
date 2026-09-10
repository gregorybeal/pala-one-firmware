#ifndef PALA_PURE_PROGRESS_GATE_H
#define PALA_PURE_PROGRESS_GATE_H

#include "arduino_compat.h"

// ============================================================================
//  When should a long blocking operation repaint a progress screen?
//
//  Rebuilding the page table walks the book sequentially, so the wait grows
//  with how far in you are: a layout change deep into a long book, or a sync
//  jump to where the other device is, can block for seconds with nothing on
//  screen. Users read that as a crash (see issue #118, and #137 for what an
//  unresponsive device looks like from outside).
//
//  Painting is not free, though. A partial e-ink refresh costs a few hundred
//  milliseconds, and it is spent inside the loop being measured — repaint too
//  eagerly and the progress screen is itself a large part of the wait. Worse,
//  most paginations finish in well under a second, and flashing a screen at
//  those would make every page turn feel like it stuttered.
//
//  Hence two rules, both about time rather than percentage:
//
//    - stay invisible until the operation has already proven slow (quietMs),
//      so fast paths never paint at all
//    - once visible, repaint no more often than minRepaintMs
//
//  Pure so the policy can be tested without a display; ui/reader.cpp owns the
//  drawing. See test/test_progress_gate.cpp.
// ============================================================================

struct ProgressGate {
  // Tuned against an assumed ~0.3-0.5 s partial refresh on this panel. The
  // repaint is spent inside the loop it is measuring, so minRepaintMs is the
  // knob that decides how much of the wait goes on reporting the wait: at
  // 1500 ms a five-second walk spends roughly a second drawing. Worth
  // re-checking against a real panel — see the note in ui/reader.cpp.
  uint32_t quietMs      = 500;
  uint32_t minRepaintMs = 1500;

  // Start (or restart) an operation. Nothing paints until quietMs has passed.
  void begin(uint32_t nowMs);

  // True when the caller should paint `pct` now. Clamps `pct` into [0, 100].
  // A percentage that has not moved never paints, so a stalled operation
  // does not burn refreshes.
  bool shouldPaint(uint32_t nowMs, int pct);

  // Whether anything has been painted for this operation — the caller needs
  // to know, because having drawn over the screen it must force a full
  // refresh afterwards rather than a partial one.
  bool painted() const { return painted_; }

  // Percentage last painted, or -1 if nothing has been.
  int lastPct() const { return lastPct_; }

private:
  uint32_t startedMs_   = 0;
  uint32_t lastPaintMs_ = 0;
  int      lastPct_     = -1;
  bool     painted_     = false;
};

// Percentage of `done` out of `total`, clamped to [0, 100]. A zero total
// reads as 0 rather than dividing.
int progressPercent(uint32_t done, uint32_t total);

#endif  // PALA_PURE_PROGRESS_GATE_H
