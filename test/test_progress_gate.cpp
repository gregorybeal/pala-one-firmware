#include "test_framework.h"
#include "pure/progress_gate.h"

// ----------------------------------------------------------------------------
//  Staying out of the way of fast operations
// ----------------------------------------------------------------------------
// Thresholds are pinned per-test rather than inherited, so retuning the
// defaults for a different panel does not silently rewrite what these mean.
static ProgressGate gate(uint32_t quietMs = 400, uint32_t minRepaintMs = 1000) {
  ProgressGate g;
  g.quietMs      = quietMs;
  g.minRepaintMs = minRepaintMs;
  return g;
}

TEST_CASE("the shipped defaults stay in the range the panel can afford") {
  // A repaint costs a few hundred ms of e-ink time and is spent inside the
  // walk being measured, so these are a budget, not a preference.
  ProgressGate d;
  CHECK(d.quietMs >= 300);
  CHECK(d.minRepaintMs >= 1000);
}

TEST_CASE("a fast operation never paints") {
  // The common case: the page table already covers the target, or the walk is
  // a handful of pages. Flashing a progress screen here would make every page
  // turn look like it stuttered.
  ProgressGate g = gate();
  g.begin(1000);
  CHECK(!g.shouldPaint(1050, 20));
  CHECK(!g.shouldPaint(1150, 60));
  CHECK(!g.shouldPaint(1399, 99));
  CHECK(!g.painted());
  CHECK_EQ(g.lastPct(), -1);
}

TEST_CASE("the screen appears only once the operation is provably slow") {
  ProgressGate g = gate();
  g.begin(1000);
  CHECK(!g.shouldPaint(1399, 30));   // still inside the quiet window
  CHECK(g.shouldPaint(1400, 31));    // quietMs reached
  CHECK(g.painted());
  CHECK_EQ(g.lastPct(), 31);
}

// ----------------------------------------------------------------------------
//  Not spending the wait on drawing
// ----------------------------------------------------------------------------
TEST_CASE("repaints are spaced out once visible") {
  ProgressGate g = gate();
  g.begin(0);
  REQUIRE(g.shouldPaint(400, 10));    // first paint
  CHECK(!g.shouldPaint(900, 20));     // 500 ms later — too soon
  CHECK(!g.shouldPaint(1399, 30));
  CHECK(g.shouldPaint(1400, 40));     // 1000 ms after the last paint
  CHECK_EQ(g.lastPct(), 40);
}

TEST_CASE("an unchanged percentage never paints") {
  ProgressGate g = gate();
  g.begin(0);
  REQUIRE(g.shouldPaint(400, 10));
  // A long walk through a dense stretch of the book can sit on the same
  // percentage for a while; there is nothing to redraw.
  CHECK(!g.shouldPaint(5000, 10));
  CHECK(!g.shouldPaint(9000, 10));
  CHECK(g.shouldPaint(9001, 11));
}

TEST_CASE("percentages are clamped rather than trusted") {
  ProgressGate g = gate();
  g.begin(0);
  CHECK(g.shouldPaint(400, 250));
  CHECK_EQ(g.lastPct(), 100);

  ProgressGate h = gate();
  h.begin(0);
  CHECK(h.shouldPaint(400, -7));
  CHECK_EQ(h.lastPct(), 0);
}

TEST_CASE("begin() resets a gate for the next operation") {
  ProgressGate g = gate();
  g.begin(0);
  REQUIRE(g.shouldPaint(400, 50));
  REQUIRE(g.painted());

  g.begin(10000);
  CHECK(!g.painted());
  CHECK_EQ(g.lastPct(), -1);
  CHECK(!g.shouldPaint(10100, 50));   // quiet window applies again
  CHECK(g.shouldPaint(10400, 50));
}

TEST_CASE("thresholds are configurable") {
  ProgressGate g = gate(0, 0);
  g.begin(0);
  CHECK(g.shouldPaint(0, 1));
  CHECK(g.shouldPaint(0, 2));
  CHECK(!g.shouldPaint(0, 2));   // still gated on the percentage moving
}

TEST_CASE("a millis() rollover does not unblock the quiet window") {
  // millis() wraps every ~49 days. Unsigned subtraction keeps the elapsed
  // time small across the wrap; comparing timestamps directly would read as
  // a huge elapsed time and paint immediately.
  ProgressGate g = gate();
  const uint32_t nearMax = 0xFFFFFF00u;
  g.begin(nearMax);
  CHECK(!g.shouldPaint(nearMax + 100, 10));   // wrapped, but only 100 ms in
  CHECK(g.shouldPaint(nearMax + 400, 20));
}

// ----------------------------------------------------------------------------
//  progressPercent
// ----------------------------------------------------------------------------
TEST_CASE("progressPercent clamps and never divides by zero") {
  CHECK_EQ(progressPercent(0, 0), 0);
  CHECK_EQ(progressPercent(50, 0), 0);
  CHECK_EQ(progressPercent(0, 100), 0);
  CHECK_EQ(progressPercent(50, 100), 50);
  CHECK_EQ(progressPercent(100, 100), 100);
  CHECK_EQ(progressPercent(150, 100), 100);
}

TEST_CASE("progressPercent does not overflow on book-sized byte counts") {
  // done * 100 exceeds 32 bits well before the 4.5 MB partition limit, so the
  // multiply has to widen first.
  CHECK_EQ(progressPercent(2000000u, 4000000u), 50);
  CHECK_EQ(progressPercent(3000000u, 4000000u), 75);
  CHECK_EQ(progressPercent(0xFFFFFFFEu, 0xFFFFFFFFu), 99);
}
