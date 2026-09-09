#ifndef PALA_PURE_KOSYNC_CODEC_H
#define PALA_PURE_KOSYNC_CODEC_H

#include "arduino_compat.h"

// ============================================================================
//  KOReader `kosync` protocol — the parts that are pure data transformation.
//
//  Networking lives in hal/kosync.cpp, the on-device flow in
//  ui/screens/sync_screen.cpp; everything here is host-testable and has no
//  Arduino or library dependency (see test/test_kosync_codec.cpp).
//
//  Position model. Pala One's canonical reading position is a byte offset
//  into the stored .txt (see storage/book_metadata.h). KOReader's is an
//  XPointer into its own rendering of the source document, which we cannot
//  synthesize or interpret. The one value both sides agree on is
//  `percentage`, so that is the sync currency and these helpers are the
//  only place the two position models meet.
// ============================================================================

static const size_t KOSYNC_DOC_BYTES = 16;   // raw partial-MD5 digest
static const size_t KOSYNC_DOC_HEX   = 32;   // its lowercase hex form

// Positions closer than this are treated as the same place. One page of a
// medium-length book is well under 0.5 %, so this keeps a sub-page rounding
// difference from prompting the user on every sync.
static const float KOSYNC_DEADBAND = 0.005f;

// ----------------------------------------------------------------------------
//  Document id — KOReader's "partial MD5", computed browser-side over the
//  original uploaded file (see web/epub_js.h for why it cannot be computed
//  on-device) and stored per book as 16 raw bytes.
// ----------------------------------------------------------------------------

// Lowercase hex, always 32 characters.
String kosyncDocToHex(const uint8_t doc[KOSYNC_DOC_BYTES]);

// Parse 32 hex characters (either case). Returns false and leaves `out`
// untouched on any wrong length or non-hex byte.
bool kosyncDocFromHex(const String& hex, uint8_t out[KOSYNC_DOC_BYTES]);

// An all-zero digest is the "no document id recorded" sentinel — books
// uploaded before this feature existed, or a cleared entry.
bool kosyncDocIsSet(const uint8_t doc[KOSYNC_DOC_BYTES]);

// ----------------------------------------------------------------------------
//  Request / response bodies
// ----------------------------------------------------------------------------

struct KosyncPush {
  String   document;             // 32 hex chars
  String   device;               // human-readable name, e.g. "Pala One"
  String   deviceId;             // stable per-device id
  float    percentage = 0.0f;    // [0, 1]
};

// Body for `PUT /syncs/progress`. `progress` carries the percentage as a
// string: KOReader puts an XPointer there for reflowable documents, which
// we have no way to produce, so the numeric form is the honest best effort.
String buildProgressBody(const KosyncPush& p);

struct KosyncRemote {
  bool   valid = false;          // a usable percentage was present
  float  percentage = 0.0f;
  String progress;               // opaque — displayed, never interpreted
  String device;                 // which device last wrote, for the prompt
};

// Parse a `GET /syncs/progress/:document` response. Returns false for an
// empty body, a malformed one, or a well-formed one with no `percentage`
// (which is what the server returns for a document it has never seen).
bool parseProgressResponse(const String& json, KosyncRemote& out);

// Minimal JSON string-body escaping for the free-text fields we send.
String kosyncJsonEscape(const String& s);

// ----------------------------------------------------------------------------
//  Position mapping
// ----------------------------------------------------------------------------

// Byte offset -> [0, 1]. A zero-size file reads as 0.
float percentageForOffset(uint32_t offset, uint32_t fileSize);

// [0, 1] -> byte offset, clamped into the file. The reader then snaps this
// to a real page boundary via findPageForOffset().
uint32_t offsetForPercentage(float pct, uint32_t fileSize);

enum SyncDecision {
  SYNC_IDENTICAL,      // within KOSYNC_DEADBAND — nothing to ask the user
  SYNC_REMOTE_AHEAD,   // the other device is further into the book
  SYNC_REMOTE_BEHIND   // this device is further into the book
};

SyncDecision decideSync(float localPct, float remotePct);

#endif  // PALA_PURE_KOSYNC_CODEC_H
