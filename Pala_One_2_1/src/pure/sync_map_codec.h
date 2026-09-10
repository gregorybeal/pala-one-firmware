#ifndef PALA_PURE_SYNC_MAP_CODEC_H
#define PALA_PURE_SYNC_MAP_CODEC_H

#include "arduino_compat.h"

// ============================================================================
//  Spine-map file format (sm_<hash>.bin in LittleFS root).
//
//  Built in the browser at upload time by the EPUB flattener (web/epub_js.h)
//  and read back by storage/sync_map.cpp. It answers, in both directions:
//
//      crengine XPointer  <->  byte offset in the stored .txt
//
//  which is what makes KOReader sync land on the right paragraph instead of
//  the right percentage. See pure/xpointer.h for the pointer syntax.
//
//  Everything is little-endian, matching the ESP32 and the DataView writes on
//  the browser side. Sections are contiguous and their offsets are derived
//  from the counts in the header rather than stored, so there is exactly one
//  description of the layout and it lives here.
//
//    header       24 bytes
//    fragments    fragCount * 12   — one per OPF itemref, in spine order
//    pool index   poolCount * 4    — offset of each string into the blob
//    pool blob    poolBlobBytes    — path and element-name strings, no NULs
//    blocks       blockCount * 10  — in document order within each fragment
//
//  The pool holds both parent paths (`/body[1]/div[2]`) and element names
//  (`p`), deduplicated across the whole book; a fragment's blocks reference
//  it by index. Novels land around 20-30 KB against a 4.5 MB partition.
//
//  `textSize` stamps the flattened .txt the map was built for. The reader
//  rejects a map whose stamp doesn't match the book on disk, which is what
//  makes a re-upload of a different edition fail safe rather than silently
//  seek to nonsense.
// ============================================================================

static const uint32_t SYNC_MAP_MAGIC        = 0x50534D31UL;   // 'PSM1'
static const size_t   SYNC_MAP_HEADER_BYTES = 24;
static const size_t   SYNC_MAP_FRAG_BYTES   = 12;
static const size_t   SYNC_MAP_BLOCK_BYTES  = 10;

// Guard rails for a file that arrived over HTTP. A book with more than this
// many fragments or blocks is not something the device can page through
// anyway, and the limits keep every derived offset inside size_t.
static const uint32_t SYNC_MAP_MAX_FRAGMENTS = 4096;
static const uint32_t SYNC_MAP_MAX_BLOCKS    = 200000UL;
// The loader buffers the pool index (4 bytes/entry) while resolving, so this
// cap is also a heap budget: 2048 entries = 8 KB, transient, freed on close.
static const uint32_t SYNC_MAP_MAX_POOL      = 2048UL;
static const uint32_t SYNC_MAP_MAX_POOL_BLOB = 131072UL;

// Hard ceiling on the whole file, enforced while it streams in over HTTP so
// a runaway upload cannot fill the partition before the header is checked.
static const uint32_t SYNC_MAP_MAX_FILE_BYTES = 524288UL;

struct SyncMapHeader {
  uint32_t magic         = 0;
  uint32_t textSize      = 0;   // flattened .txt size the map was built for
  uint16_t fragCount     = 0;   // one per OPF itemref, including linear="no"
  uint16_t poolCount     = 0;
  uint32_t blockCount    = 0;
  uint32_t poolBlobBytes = 0;
};

// One spine document.
struct SyncMapFragment {
  uint32_t textStart   = 0;   // byte offset where this fragment's text begins
  uint32_t blockBase   = 0;   // index of its first block in the block table
  uint16_t blockLen    = 0;
  uint16_t linearIndex = 0;   // 1-based position among linear itemrefs; 0 if
                              // this itemref contributed no text
};

// One block element that emitted text.
struct SyncMapBlock {
  uint32_t textStart = 0;
  uint16_t pathId    = 0;   // pool index of the canonical parent path
  uint16_t ordinal   = 0;   // 1-based among same-named siblings
  uint16_t nameId    = 0;   // pool index of the element's local name
};

// Little-endian readers over a caller-owned buffer. Bounds are the caller's
// business — these are used behind size checks in the loaders.
uint16_t syncMapReadU16(const uint8_t* p);
uint32_t syncMapReadU32(const uint8_t* p);

// Parse and validate a header. Returns false on a short buffer, a wrong
// magic, or counts outside the guard rails above.
bool parseSyncMapHeader(const uint8_t* buf, size_t n, SyncMapHeader& out);

// Section offsets, derived from the header's counts.
size_t syncMapFragTableAt(const SyncMapHeader& h);
size_t syncMapPoolIndexAt(const SyncMapHeader& h);
size_t syncMapPoolBlobAt(const SyncMapHeader& h);
size_t syncMapBlockTableAt(const SyncMapHeader& h);
size_t syncMapTotalBytes(const SyncMapHeader& h);

// Record decoders. `p` addresses SYNC_MAP_FRAG_BYTES / SYNC_MAP_BLOCK_BYTES
// of raw record.
SyncMapFragment decodeSyncMapFragment(const uint8_t* p);
SyncMapBlock    decodeSyncMapBlock(const uint8_t* p);

#endif  // PALA_PURE_SYNC_MAP_CODEC_H
