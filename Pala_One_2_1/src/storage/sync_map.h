#ifndef PALA_STORAGE_SYNC_MAP_H
#define PALA_STORAGE_SYNC_MAP_H

#include "src/config.h"
#include "src/state.h"
#include "src/pure/sync_map_codec.h"
#include "src/pure/xpointer.h"

// ============================================================================
//  Per-book spine map (sm_<hash>.bin in LittleFS root)
//
//  Translates between crengine XPointers and byte offsets in the stored .txt,
//  which is what lets KOReader sync land on a paragraph rather than on a
//  percentage. Built browser-side at upload (web/epub_js.h), received by
//  POST /kosync-map (web/kosync.cpp), read here.
//
//  Lifecycle mirrors the page cache: one file per book, keyed by the same
//  path hash, deleted and renamed alongside the rest of the book's metadata
//  (storage/book_metadata.cpp). A map is stamped with the flattened text size
//  it was built for and is rejected if the book on disk no longer matches.
//
//  Everything here is best-effort by design. A book with no map, a map that
//  fails its stamp, or a pointer whose path crengine shaped differently than
//  the browser did, all fall back to the percentage model in
//  pure/kosync_codec.h — worse, but never wrong in a way that loses a
//  reading position.
//
//  Reads seek rather than slurp: the block table is the big section and is
//  scanned in chunks. The one heap allocation is the pool index (4 bytes per
//  entry, capped at 8 KB by SYNC_MAP_MAX_POOL), held only between open() and
//  close().
// ============================================================================

class SyncMap {
public:
  // Which numbering a pointer's DocFragment[N] is assumed to use. crengine
  // builds one fragment per spine itemref, but whether it counts itemrefs
  // marked linear="no" is not something this firmware can know, so callers
  // resolve both ways and keep whichever lands closer to the percentage the
  // server also sent. See SyncScreen::runSync.
  enum class FragmentMode {
    Opf,      // N indexes every itemref, in OPF order
    Linear    // N indexes only the itemrefs that contributed text
  };

  SyncMap() = default;
  ~SyncMap() { close(); }

  // Holds an open File and owns a malloc'd pool index; copying either would
  // double-free. Callers only ever use it as a short-lived local.
  SyncMap(const SyncMap&) = delete;
  SyncMap& operator=(const SyncMap&) = delete;

  // Open the map for `bookPath`. Fails if there is no map, the file is
  // truncated, or it was built for a different text size.
  bool open(const String& bookPath, uint32_t expectedTextSize);
  void close();
  bool isOpen() const { return open_; }

  // Resolve `xp` to a byte offset. Tries the pointer's full path first, then
  // successively shallower prefixes, so extra inline levels in crengine's DOM
  // (or ours) degrade to the nearest enclosing block instead of failing. A
  // pointer that names only a fragment resolves to that fragment's start.
  bool offsetForXPointer(const XPointer& xp, FragmentMode mode, uint32_t& out);

  // Compose a pointer addressing `offset`. Empty string if the offset falls
  // outside the map. Always uses OPF fragment numbering.
  String xpointerForOffset(uint32_t offset);

private:
  File          f_;
  SyncMapHeader h_;
  bool          open_     = false;
  uint32_t*     poolIndex_ = nullptr;   // poolCount entries, blob-relative

  bool   readAt(size_t pos, uint8_t* buf, size_t n);
  bool   fragmentAt(uint16_t index, SyncMapFragment& out);
  bool   blockAt(uint32_t index, SyncMapBlock& out);
  String poolString(uint16_t id);

  // One pass over the pool, resolving `count` strings to ids at once (-1 for
  // absent). Cheaper than a scan per lookup when the caller has a whole
  // ladder of candidate paths to try.
  void   poolResolve(const String* wanted, int count, int* outIds);

  // Index of the fragment record for `xp` under `mode`, or -1.
  int    fragmentIndexFor(const XPointer& xp, FragmentMode mode);

  // Last block in [base, base+len) whose textStart <= offset, or -1.
  int    blockContaining(uint32_t base, uint16_t len, uint32_t offset);
};

// Lifecycle helpers, called from storage/book_metadata.cpp alongside the
// page-cache equivalents.
String syncMapPathForBook(const String& bookPath);
bool   hasSyncMapForBook(const String& bookPath);
void   deleteSyncMapForBook(const String& bookPath);
void   renameSyncMapForBook(const String& oldPath, const String& newPath);

#endif  // PALA_STORAGE_SYNC_MAP_H
