#include "test_framework.h"
#include "pure/kosync_codec.h"

#include <cmath>

static bool nearly(float a, float b, float eps = 1e-4f) {
  return std::fabs(a - b) <= eps;
}

// ----------------------------------------------------------------------------
//  Document id
// ----------------------------------------------------------------------------
TEST_CASE("kosyncDocToHex renders 32 lowercase hex chars") {
  uint8_t doc[KOSYNC_DOC_BYTES] = {
    0x00, 0x01, 0x0f, 0x10, 0x7f, 0x80, 0xab, 0xcd,
    0xef, 0xfe, 0x12, 0x34, 0x56, 0x78, 0x9a, 0xff
  };
  CHECK_EQ(kosyncDocToHex(doc), String("00010f107f80abcdeffe123456789aff"));
  CHECK_EQ(kosyncDocToHex(doc).length(), (unsigned)KOSYNC_DOC_HEX);
}

TEST_CASE("kosyncDocFromHex round-trips and accepts either case") {
  uint8_t in[KOSYNC_DOC_BYTES];
  REQUIRE(kosyncDocFromHex("00010f107f80abcdeffe123456789aff", in));
  CHECK_EQ(kosyncDocToHex(in), String("00010f107f80abcdeffe123456789aff"));

  uint8_t upper[KOSYNC_DOC_BYTES];
  REQUIRE(kosyncDocFromHex("00010F107F80ABCDEFFE123456789AFF", upper));
  CHECK_EQ(kosyncDocToHex(upper), String("00010f107f80abcdeffe123456789aff"));
}

TEST_CASE("kosyncDocFromHex rejects bad input without touching the buffer") {
  uint8_t doc[KOSYNC_DOC_BYTES];
  for (size_t i = 0; i < KOSYNC_DOC_BYTES; i++) doc[i] = 0x5a;

  CHECK(!kosyncDocFromHex("", doc));
  CHECK(!kosyncDocFromHex("abc", doc));                                  // short
  CHECK(!kosyncDocFromHex("00010f107f80abcdeffe123456789af", doc));      // 31
  CHECK(!kosyncDocFromHex("00010f107f80abcdeffe123456789affa", doc));    // 33
  CHECK(!kosyncDocFromHex("00010f107f80abcdeffe12345678zzff", doc));     // non-hex
  // A rejected parse must leave the caller's buffer exactly as it was — the
  // last byte is where a naive left-to-right writer would have scribbled.
  for (size_t i = 0; i < KOSYNC_DOC_BYTES; i++) CHECK_EQ((int)doc[i], 0x5a);
}

TEST_CASE("kosyncDocIsSet treats all-zero as unset") {
  uint8_t zero[KOSYNC_DOC_BYTES] = {0};
  CHECK(!kosyncDocIsSet(zero));

  uint8_t last[KOSYNC_DOC_BYTES] = {0};
  last[KOSYNC_DOC_BYTES - 1] = 1;
  CHECK(kosyncDocIsSet(last));

  uint8_t first[KOSYNC_DOC_BYTES] = {0};
  first[0] = 1;
  CHECK(kosyncDocIsSet(first));
}

// ----------------------------------------------------------------------------
//  Request body
// ----------------------------------------------------------------------------
TEST_CASE("buildProgressBody emits the fields kosync expects") {
  KosyncPush p;
  p.document   = "00010f107f80abcdeffe123456789aff";
  p.device     = "Pala One";
  p.deviceId   = "A1B2C3D4";
  p.percentage = 0.4712f;

  String body = buildProgressBody(p);
  CHECK(body.indexOf("\"document\":\"00010f107f80abcdeffe123456789aff\"") >= 0);
  CHECK(body.indexOf("\"device\":\"Pala One\"") >= 0);
  CHECK(body.indexOf("\"device_id\":\"A1B2C3D4\"") >= 0);
  // percentage numeric, progress the same value as a string
  CHECK(body.indexOf("\"percentage\":0.4712") >= 0);
  CHECK(body.indexOf("\"progress\":\"0.4712\"") >= 0);
  CHECK(body.startsWith("{"));
  CHECK(body.endsWith("}"));
}

TEST_CASE("buildProgressBody clamps percentage into [0,1]") {
  KosyncPush p;
  p.document = "d";

  p.percentage = -3.0f;
  CHECK(buildProgressBody(p).indexOf("\"percentage\":0.0000") >= 0);

  p.percentage = 9.0f;
  CHECK(buildProgressBody(p).indexOf("\"percentage\":1.0000") >= 0);
}

TEST_CASE("kosyncJsonEscape escapes what would break the body") {
  CHECK_EQ(kosyncJsonEscape("plain"), String("plain"));
  CHECK_EQ(kosyncJsonEscape("say \"hi\""), String("say \\\"hi\\\""));
  CHECK_EQ(kosyncJsonEscape("back\\slash"), String("back\\\\slash"));
  CHECK_EQ(kosyncJsonEscape("a\nb"), String("a\\nb"));
  CHECK_EQ(kosyncJsonEscape("a\tb"), String("a\\tb"));
  // Control characters with no short escape take the \u00XX form.
  String ctrl;
  ctrl += (char)0x01;
  CHECK_EQ(kosyncJsonEscape(ctrl), String("\\u0001"));
  // Multi-byte UTF-8 passes through untouched.
  CHECK_EQ(kosyncJsonEscape("caf\xc3\xa9"), String("caf\xc3\xa9"));
}

TEST_CASE("a device name with quotes cannot break out of the body") {
  KosyncPush p;
  p.document = "d";
  p.device   = "evil\",\"percentage\":9.9,\"x\":\"";
  String body = buildProgressBody(p);
  // The injected text must be inert: the real percentage field still parses
  // as the value we set, not the one the attacker tried to smuggle in.
  KosyncRemote r;
  REQUIRE(parseProgressResponse(body, r));
  CHECK(nearly(r.percentage, 0.0f));
}

// ----------------------------------------------------------------------------
//  Response parsing
// ----------------------------------------------------------------------------
TEST_CASE("parseProgressResponse reads a real kosync payload") {
  String json =
    "{\"username\":\"greg\",\"document\":\"00010f107f80abcdeffe123456789aff\","
    "\"progress\":\"/body/DocFragment[11]/body/div/p[3].0\",\"percentage\":0.4712,"
    "\"device\":\"KOReader\",\"device_id\":\"ABC\",\"timestamp\":1730000000}";

  KosyncRemote r;
  REQUIRE(parseProgressResponse(json, r));
  CHECK(r.valid);
  CHECK(nearly(r.percentage, 0.4712f));
  CHECK_EQ(r.progress, String("/body/DocFragment[11]/body/div/p[3].0"));
  CHECK_EQ(r.device, String("KOReader"));
}

TEST_CASE("parseProgressResponse accepts a quoted percentage") {
  KosyncRemote r;
  REQUIRE(parseProgressResponse("{\"percentage\":\"0.25\"}", r));
  CHECK(nearly(r.percentage, 0.25f));
}

TEST_CASE("parseProgressResponse tolerates whitespace and field order") {
  KosyncRemote r;
  REQUIRE(parseProgressResponse(
    "{\n  \"device\" : \"KOReader\" ,\n  \"percentage\" : 0.5\n}", r));
  CHECK(nearly(r.percentage, 0.5f));
  CHECK_EQ(r.device, String("KOReader"));
}

TEST_CASE("parseProgressResponse rejects bodies with no usable percentage") {
  KosyncRemote r;
  CHECK(!parseProgressResponse("", r));
  CHECK(!parseProgressResponse("{}", r));
  CHECK(!parseProgressResponse("not json at all", r));
  // The server's "unknown document" and error shapes.
  CHECK(!parseProgressResponse("{\"code\":2003,\"message\":\"Document not found\"}", r));
  // Present but not a number.
  CHECK(!parseProgressResponse("{\"percentage\":\"abc\"}", r));
  // Truncated body.
  CHECK(!parseProgressResponse("{\"percentage\":0.5", r));
  CHECK(!r.valid);
}

TEST_CASE("parseProgressResponse is not fooled by a key name inside a value") {
  // A device that named itself after the field we are looking for must not
  // shadow the real one.
  String json = "{\"device\":\"\\\"percentage\\\":0.99\",\"percentage\":0.10}";
  KosyncRemote r;
  REQUIRE(parseProgressResponse(json, r));
  CHECK(nearly(r.percentage, 0.10f));
}

TEST_CASE("parseProgressResponse clamps out-of-range percentages") {
  KosyncRemote r;
  REQUIRE(parseProgressResponse("{\"percentage\":1.7}", r));
  CHECK(nearly(r.percentage, 1.0f));
  REQUIRE(parseProgressResponse("{\"percentage\":-0.4}", r));
  CHECK(nearly(r.percentage, 0.0f));
}

TEST_CASE("parseProgressResponse resets output between calls") {
  KosyncRemote r;
  REQUIRE(parseProgressResponse("{\"percentage\":0.5,\"device\":\"KOReader\"}", r));
  CHECK_EQ(r.device, String("KOReader"));
  // A failing parse must not leave the previous device name visible.
  CHECK(!parseProgressResponse("{}", r));
  CHECK(!r.valid);
  CHECK_EQ(r.device, String(""));
}

// ----------------------------------------------------------------------------
//  Position mapping
// ----------------------------------------------------------------------------
TEST_CASE("percentageForOffset covers the ends and the middle") {
  CHECK(nearly(percentageForOffset(0, 1000), 0.0f));
  CHECK(nearly(percentageForOffset(500, 1000), 0.5f));
  CHECK(nearly(percentageForOffset(999, 1000), 0.999f));
  // At or past EOF reads as fully read, never as >1.
  CHECK(nearly(percentageForOffset(1000, 1000), 1.0f));
  CHECK(nearly(percentageForOffset(5000, 1000), 1.0f));
}

TEST_CASE("percentageForOffset survives a zero-size file") {
  CHECK(nearly(percentageForOffset(0, 0), 0.0f));
  CHECK(nearly(percentageForOffset(42, 0), 0.0f));
}

TEST_CASE("offsetForPercentage stays inside the file") {
  CHECK_EQ(offsetForPercentage(0.0f, 1000), 0u);
  CHECK_EQ(offsetForPercentage(0.5f, 1000), 500u);
  // 1.0 must land on the last addressable byte, not one past the end —
  // fileSize itself would read as EOF to the reader.
  CHECK_EQ(offsetForPercentage(1.0f, 1000), 999u);
  CHECK_EQ(offsetForPercentage(2.0f, 1000), 999u);
  CHECK_EQ(offsetForPercentage(-1.0f, 1000), 0u);
  CHECK_EQ(offsetForPercentage(0.5f, 0), 0u);
}

TEST_CASE("offsetForPercentage rounds to nearest") {
  // 0.3336 * 1000 = 333.6 -> 334
  CHECK_EQ(offsetForPercentage(0.3336f, 1000), 334u);
  CHECK_EQ(offsetForPercentage(0.3334f, 1000), 333u);
}

TEST_CASE("offset -> percentage -> offset round-trips within a byte") {
  const uint32_t size = 850000;   // a plausible large book
  const uint32_t probes[] = {0, 1, 1024, 425000, 849998, 849999};
  for (uint32_t off : probes) {
    float pct = percentageForOffset(off, size);
    uint32_t back = offsetForPercentage(pct, size);
    uint32_t drift = (back > off) ? (back - off) : (off - back);
    // float has ~7 significant digits; over 850 KB that is a handful of
    // bytes, far inside one page. findPageForOffset() then snaps to a real
    // page boundary, so this only has to be close.
    CHECK(drift <= 64u);
  }
}

// ----------------------------------------------------------------------------
//  Conflict decision
// ----------------------------------------------------------------------------
TEST_CASE("decideSync ignores differences inside the dead band") {
  CHECK_EQ((int)decideSync(0.5f, 0.5f), (int)SYNC_IDENTICAL);
  CHECK_EQ((int)decideSync(0.5f, 0.5f + KOSYNC_DEADBAND * 0.5f), (int)SYNC_IDENTICAL);
  CHECK_EQ((int)decideSync(0.5f, 0.5f - KOSYNC_DEADBAND * 0.5f), (int)SYNC_IDENTICAL);
}

TEST_CASE("decideSync reports which side is further along") {
  CHECK_EQ((int)decideSync(0.10f, 0.90f), (int)SYNC_REMOTE_AHEAD);
  CHECK_EQ((int)decideSync(0.90f, 0.10f), (int)SYNC_REMOTE_BEHIND);
  // Just outside the dead band in each direction.
  CHECK_EQ((int)decideSync(0.5f, 0.5f + KOSYNC_DEADBAND * 2.0f), (int)SYNC_REMOTE_AHEAD);
  CHECK_EQ((int)decideSync(0.5f, 0.5f - KOSYNC_DEADBAND * 2.0f), (int)SYNC_REMOTE_BEHIND);
}

TEST_CASE("decideSync handles the ends of the book") {
  CHECK_EQ((int)decideSync(0.0f, 0.0f), (int)SYNC_IDENTICAL);
  CHECK_EQ((int)decideSync(1.0f, 1.0f), (int)SYNC_IDENTICAL);
  CHECK_EQ((int)decideSync(0.0f, 1.0f), (int)SYNC_REMOTE_AHEAD);
  CHECK_EQ((int)decideSync(1.0f, 0.0f), (int)SYNC_REMOTE_BEHIND);
}
