#include "src/pure/progress_gate.h"

void ProgressGate::begin(uint32_t nowMs) {
  startedMs_   = nowMs;
  lastPaintMs_ = nowMs;
  lastPct_     = -1;
  painted_     = false;
}

bool ProgressGate::shouldPaint(uint32_t nowMs, int pct) {
  if (pct < 0)   pct = 0;
  if (pct > 100) pct = 100;

  // Nothing new to show. Also covers the stalled case: a loop that stops
  // making progress stops asking for refreshes.
  if (pct == lastPct_) return false;

  // Subtraction rather than comparison so a millis() rollover mid-operation
  // reads as a small elapsed time, not a huge one.
  const uint32_t sinceStart = nowMs - startedMs_;
  const uint32_t sincePaint = nowMs - lastPaintMs_;

  if (!painted_) {
    if (sinceStart < quietMs) return false;
  } else if (sincePaint < minRepaintMs) {
    return false;
  }

  lastPaintMs_ = nowMs;
  lastPct_     = pct;
  painted_     = true;
  return true;
}

int progressPercent(uint32_t done, uint32_t total) {
  if (total == 0) return 0;
  if (done >= total) return 100;
  return (int)(((uint64_t)done * 100u) / total);
}
