#include "test_framework.h"
#include "pure/xpointer.h"

// ----------------------------------------------------------------------------
//  Parsing the forms KOReader actually sends
// ----------------------------------------------------------------------------
TEST_CASE("parseXPointer reads fragment, path and text offset") {
  XPointer xp;
  REQUIRE(parseXPointer("/body/DocFragment[11]/body/div/p[3].0", xp));
  CHECK(xp.valid);
  CHECK(xp.hasFragment);
  CHECK_EQ(xp.fragment, (uint16_t)11);
  CHECK_EQ(xp.stepCount, (uint8_t)3);
  CHECK_EQ(String(xp.steps[0].name), String("body"));
  CHECK_EQ(String(xp.steps[1].name), String("div"));
  CHECK_EQ(String(xp.steps[2].name), String("p"));
  CHECK_EQ(xp.steps[2].ordinal, (uint16_t)3);
  CHECK_EQ(xp.textOffset, (uint32_t)0);
}

TEST_CASE("parseXPointer normalizes an absent index to 1") {
  XPointer xp;
  REQUIRE(parseXPointer("/body/DocFragment[2]/body/div/p.0", xp));
  CHECK_EQ(xp.steps[1].ordinal, (uint16_t)1);
  CHECK_EQ(xp.steps[2].ordinal, (uint16_t)1);
  // ...which is exactly what makes it comparable to the map's stored paths.
  CHECK_EQ(canonicalPath(xp, 2), String("/body[1]/div[1]"));
}

TEST_CASE("parseXPointer drops the text() step but keeps its offset") {
  XPointer xp;
  REQUIRE(parseXPointer("/body/DocFragment[2]/body/p[1]/text().15", xp));
  CHECK_EQ(xp.stepCount, (uint8_t)2);
  CHECK_EQ(String(xp.steps[1].name), String("p"));
  CHECK_EQ(xp.textOffset, (uint32_t)15);
}

TEST_CASE("parseXPointer handles a pointer with no DocFragment") {
  XPointer xp;
  REQUIRE(parseXPointer("/body/div/p[5].0", xp));
  CHECK(!xp.hasFragment);
  CHECK_EQ(xp.fragment, (uint16_t)0);
  CHECK_EQ(xp.stepCount, (uint8_t)3);
}

TEST_CASE("parseXPointer accepts a bare DocFragment") {
  XPointer xp;
  REQUIRE(parseXPointer("/body/DocFragment[7]", xp));
  CHECK(xp.hasFragment);
  CHECK_EQ(xp.fragment, (uint16_t)7);
  CHECK_EQ(xp.stepCount, (uint8_t)0);
}

TEST_CASE("parseXPointer lowercases element names") {
  XPointer xp;
  REQUIRE(parseXPointer("/body/DocFragment[1]/BODY/DIV[2]/P[3].0", xp));
  CHECK_EQ(String(xp.steps[1].name), String("div"));
  CHECK_EQ(String(xp.steps[2].name), String("p"));
}

// ----------------------------------------------------------------------------
//  Things that are not pointers
// ----------------------------------------------------------------------------
TEST_CASE("parseXPointer rejects a percentage string") {
  // This firmware's own older push path put the percentage in `progress`,
  // and KOReader does the same for paged documents. Neither is a pointer.
  XPointer xp;
  CHECK(!parseXPointer("0.4712", xp));
  CHECK(!xp.valid);
  CHECK(!parseXPointer("1", xp));
  CHECK(!parseXPointer("", xp));
}

TEST_CASE("parseXPointer rejects malformed indices") {
  XPointer xp;
  CHECK(!parseXPointer("/body/DocFragment[2]/body/p[x].0", xp));
  CHECK(!parseXPointer("/body/DocFragment[2]/body/p[3.0", xp));
  CHECK(!parseXPointer("/body/DocFragment[2]/body/p[0].0", xp));   // 1-based
}

TEST_CASE("parseXPointer survives more steps than it can hold") {
  String deep = "/body/DocFragment[1]";
  for (int i = 0; i < 40; i++) deep += "/div";
  deep += ".0";

  XPointer xp;
  REQUIRE(parseXPointer(deep, xp));
  CHECK(xp.stepCount <= XPOINTER_MAX_STEPS);
}

// ----------------------------------------------------------------------------
//  Composing
// ----------------------------------------------------------------------------
TEST_CASE("canonicalPath always writes explicit ordinals") {
  XPointer xp;
  REQUIRE(parseXPointer("/body/DocFragment[3]/body/div[2]/p[9].0", xp));
  CHECK_EQ(canonicalPath(xp, 0), String(""));
  CHECK_EQ(canonicalPath(xp, 1), String("/body[1]"));
  CHECK_EQ(canonicalPath(xp, 2), String("/body[1]/div[2]"));
  CHECK_EQ(canonicalPath(xp, 3), String("/body[1]/div[2]/p[9]"));
  // Asking for more than there is clamps rather than reading past the end.
  CHECK_EQ(canonicalPath(xp, 99), String("/body[1]/div[2]/p[9]"));
}

TEST_CASE("buildXPointer produces something parseXPointer accepts back") {
  String s = buildXPointer(11, "/body[1]/div[1]", "p", 3, 0);
  CHECK_EQ(s, String("/body/DocFragment[11]/body[1]/div[1]/p[3].0"));

  XPointer xp;
  REQUIRE(parseXPointer(s, xp));
  CHECK_EQ(xp.fragment, (uint16_t)11);
  CHECK_EQ(xp.stepCount, (uint8_t)3);
  CHECK_EQ(String(xp.steps[2].name), String("p"));
  CHECK_EQ(xp.steps[2].ordinal, (uint16_t)3);
}

TEST_CASE("buildXPointer addresses a whole fragment when there is no leaf") {
  String s = buildXPointer(4, "", "", 0, 0);
  CHECK_EQ(s, String("/body/DocFragment[4].0"));

  XPointer xp;
  REQUIRE(parseXPointer(s, xp));
  CHECK_EQ(xp.fragment, (uint16_t)4);
  CHECK_EQ(xp.stepCount, (uint8_t)0);
}

TEST_CASE("buildXPointer carries the within-block offset") {
  String s = buildXPointer(2, "/body[1]", "p", 7, 143);
  CHECK_EQ(s, String("/body/DocFragment[2]/body[1]/p[7].143"));

  XPointer xp;
  REQUIRE(parseXPointer(s, xp));
  CHECK_EQ(xp.textOffset, (uint32_t)143);
}
