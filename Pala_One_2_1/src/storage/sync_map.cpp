#include "src/storage/sync_map.h"

#include "src/pure/hashing.h"   // prefKeyForBook

// Longest pool string we will materialize. Parent paths are a few steps of
// `name[ord]`; anything past this is a malformed or hostile map.
static const size_t kMaxPoolString = 192;

// Block records read per seek while scanning a fragment.
static const uint32_t kBlockChunk = 32;

String syncMapPathForBook(const String& bookPath) {
  return String("/sm_") + prefKeyForBook(bookPath) + ".bin";
}

bool hasSyncMapForBook(const String& bookPath) {
  return FS.exists(syncMapPathForBook(bookPath));
}

void deleteSyncMapForBook(const String& bookPath) {
  String p = syncMapPathForBook(bookPath);
  if (FS.exists(p)) FS.remove(p);
}

void renameSyncMapForBook(const String& oldPath, const String& newPath) {
  String oldMap = syncMapPathForBook(oldPath);
  if (!FS.exists(oldMap)) return;
  String newMap = syncMapPathForBook(newPath);
  if (FS.exists(newMap)) FS.remove(newMap);
  FS.rename(oldMap, newMap);
}

// ----------------------------------------------------------------------------
//  Open / close
// ----------------------------------------------------------------------------
bool SyncMap::open(const String& bookPath, uint32_t expectedTextSize) {
  close();

  File f = FS.open(syncMapPathForBook(bookPath), "r");
  if (!f) return false;

  uint8_t hdr[SYNC_MAP_HEADER_BYTES];
  if (f.read(hdr, sizeof(hdr)) != sizeof(hdr)) { f.close(); return false; }

  SyncMapHeader h;
  if (!parseSyncMapHeader(hdr, sizeof(hdr), h)) { f.close(); return false; }

  // The stamp is what makes a re-uploaded or edited book fail safe: a map
  // built for different text would seek confidently to the wrong place.
  if (h.textSize != expectedTextSize)            { f.close(); return false; }
  if ((size_t)f.size() < syncMapTotalBytes(h))   { f.close(); return false; }

  uint32_t* idx = nullptr;
  if (h.poolCount > 0) {
    idx = static_cast<uint32_t*>(malloc((size_t)h.poolCount * sizeof(uint32_t)));
    if (!idx) { f.close(); return false; }

    if (!f.seek(syncMapPoolIndexAt(h))) { free(idx); f.close(); return false; }
    uint32_t prev = 0;
    for (uint16_t i = 0; i < h.poolCount; i++) {
      uint8_t b[4];
      if (f.read(b, sizeof(b)) != sizeof(b)) { free(idx); f.close(); return false; }
      idx[i] = syncMapReadU32(b);
      // Offsets are ascending and inside the blob, or the file is corrupt.
      if (idx[i] < prev || idx[i] > h.poolBlobBytes) {
        free(idx); f.close(); return false;
      }
      prev = idx[i];
    }
  }

  f_         = f;
  h_         = h;
  poolIndex_ = idx;
  open_      = true;
  return true;
}

void SyncMap::close() {
  if (poolIndex_) { free(poolIndex_); poolIndex_ = nullptr; }
  if (open_) f_.close();
  open_ = false;
  h_    = SyncMapHeader();
}

// ----------------------------------------------------------------------------
//  Raw record access
// ----------------------------------------------------------------------------
bool SyncMap::readAt(size_t pos, uint8_t* buf, size_t n) {
  if (!open_) return false;
  if (!f_.seek(pos)) return false;
  // read() reports size_t; a short or failed read simply won't equal `n`.
  return f_.read(buf, n) == n;
}

bool SyncMap::fragmentAt(uint16_t index, SyncMapFragment& out) {
  if (index >= h_.fragCount) return false;
  uint8_t rec[SYNC_MAP_FRAG_BYTES];
  size_t at = syncMapFragTableAt(h_) + (size_t)index * SYNC_MAP_FRAG_BYTES;
  if (!readAt(at, rec, sizeof(rec))) return false;
  out = decodeSyncMapFragment(rec);
  // A fragment that claims blocks outside the table is corrupt; treat it as
  // having none rather than reading someone else's records.
  if ((uint64_t)out.blockBase + out.blockLen > h_.blockCount) out.blockLen = 0;
  return true;
}

bool SyncMap::blockAt(uint32_t index, SyncMapBlock& out) {
  if (index >= h_.blockCount) return false;
  uint8_t rec[SYNC_MAP_BLOCK_BYTES];
  size_t at = syncMapBlockTableAt(h_) + (size_t)index * SYNC_MAP_BLOCK_BYTES;
  if (!readAt(at, rec, sizeof(rec))) return false;
  out = decodeSyncMapBlock(rec);
  return true;
}

String SyncMap::poolString(uint16_t id) {
  if (!open_ || poolIndex_ == nullptr || id >= h_.poolCount) return String("");

  uint32_t start = poolIndex_[id];
  uint32_t end   = (id + 1 < h_.poolCount) ? poolIndex_[id + 1] : h_.poolBlobBytes;
  if (end <= start) return String("");

  size_t len = (size_t)(end - start);
  if (len > kMaxPoolString) return String("");

  char buf[kMaxPoolString + 1];
  if (!readAt(syncMapPoolBlobAt(h_) + start,
              reinterpret_cast<uint8_t*>(buf), len)) return String("");
  buf[len] = '\0';
  return String(buf);
}

void SyncMap::poolResolve(const String* wanted, int count, int* outIds) {
  for (int i = 0; i < count; i++) outIds[i] = -1;
  if (!open_ || poolIndex_ == nullptr) return;

  int remaining = count;
  for (uint16_t id = 0; id < h_.poolCount && remaining > 0; id++) {
    String s = poolString(id);
    if (s.length() == 0) continue;
    for (int i = 0; i < count; i++) {
      if (outIds[i] < 0 && wanted[i] == s) {
        outIds[i] = (int)id;
        remaining--;
      }
    }
  }
}

// ----------------------------------------------------------------------------
//  Lookup
// ----------------------------------------------------------------------------
int SyncMap::fragmentIndexFor(const XPointer& xp, FragmentMode mode) {
  if (!open_) return -1;

  // A pointer with no DocFragment step addresses a single-document book.
  if (!xp.hasFragment) return (h_.fragCount > 0) ? 0 : -1;
  if (xp.fragment == 0) return -1;

  if (mode == FragmentMode::Opf) {
    uint16_t idx = (uint16_t)(xp.fragment - 1);
    return (idx < h_.fragCount) ? (int)idx : -1;
  }

  for (uint16_t i = 0; i < h_.fragCount; i++) {
    SyncMapFragment f;
    if (!fragmentAt(i, f)) return -1;
    if (f.linearIndex == xp.fragment) return (int)i;
  }
  return -1;
}

int SyncMap::blockContaining(uint32_t base, uint16_t len, uint32_t offset) {
  if (len == 0) return -1;

  int best = -1;
  uint8_t chunk[kBlockChunk * SYNC_MAP_BLOCK_BYTES];

  // 32-bit counter: blockLen tops out at 65535 and a uint16_t `done` would
  // wrap past the last chunk instead of ending the loop.
  for (uint32_t done = 0; done < len; done += kBlockChunk) {
    uint32_t end = done + kBlockChunk;
    if (end > len) end = len;

    size_t at = syncMapBlockTableAt(h_)
              + (size_t)(base + done) * SYNC_MAP_BLOCK_BYTES;
    if (!readAt(at, chunk, (size_t)(end - done) * SYNC_MAP_BLOCK_BYTES)) break;

    for (uint32_t i = done; i < end; i++) {
      SyncMapBlock b = decodeSyncMapBlock(
          chunk + (size_t)(i - done) * SYNC_MAP_BLOCK_BYTES);
      if (b.textStart > offset) return best;   // ascending — nothing further fits
      best = (int)(base + i);                  // deepest wins: parents precede children
    }
  }
  return best;
}

bool SyncMap::offsetForXPointer(const XPointer& xp, FragmentMode mode,
                                uint32_t& out) {
  if (!open_ || !xp.valid) return false;

  int fi = fragmentIndexFor(xp, mode);
  if (fi < 0) return false;

  SyncMapFragment frag;
  if (!fragmentAt((uint16_t)fi, frag)) return false;

  // Worst case we still know which document the pointer is in, which is
  // already far better than a whole-book percentage.
  out = frag.textStart;

  if (xp.stepCount == 0 || frag.blockLen == 0 || h_.poolCount == 0) return true;

  // Candidate ladder, deepest first: leaf steps[i] under parent steps[0..i).
  // Resolving every level's strings in one pass over the pool costs one scan
  // instead of one per level.
  String wanted[XPOINTER_MAX_STEPS * 2];
  int    ids[XPOINTER_MAX_STEPS * 2];
  int    levels = 0;
  for (int i = (int)xp.stepCount - 1; i >= 0; i--) {
    wanted[levels * 2]     = canonicalPath(xp, (uint8_t)i);
    wanted[levels * 2 + 1] = String(xp.steps[i].name);
    levels++;
  }
  poolResolve(wanted, levels * 2, ids);

  uint8_t chunk[kBlockChunk * SYNC_MAP_BLOCK_BYTES];

  for (int lvl = 0; lvl < levels; lvl++) {
    int pathId = ids[lvl * 2];
    int nameId = ids[lvl * 2 + 1];
    if (pathId < 0 || nameId < 0) continue;

    uint16_t ordinal = xp.steps[xp.stepCount - 1 - lvl].ordinal;

    for (uint32_t done = 0; done < frag.blockLen; done += kBlockChunk) {
      uint32_t end = done + kBlockChunk;
      if (end > frag.blockLen) end = frag.blockLen;

      size_t at = syncMapBlockTableAt(h_)
                + (size_t)(frag.blockBase + done) * SYNC_MAP_BLOCK_BYTES;
      if (!readAt(at, chunk, (size_t)(end - done) * SYNC_MAP_BLOCK_BYTES)) break;

      for (uint32_t i = done; i < end; i++) {
        SyncMapBlock b = decodeSyncMapBlock(
            chunk + (size_t)(i - done) * SYNC_MAP_BLOCK_BYTES);
        if ((int)b.pathId == pathId && (int)b.nameId == nameId
            && b.ordinal == ordinal) {
          out = b.textStart;
          return true;
        }
      }
    }
  }
  return true;
}

String SyncMap::xpointerForOffset(uint32_t offset) {
  if (!open_) return String("");

  // Last fragment starting at or before `offset`. Itemrefs that contributed
  // no text share the next one's start, so prefer a contributing fragment
  // when starts tie — a pointer into a skipped document resolves nowhere.
  int fi = -1;
  for (uint16_t i = 0; i < h_.fragCount; i++) {
    SyncMapFragment f;
    if (!fragmentAt(i, f)) break;
    if (f.textStart > offset) break;
    if (f.linearIndex != 0 || fi < 0) fi = (int)i;
  }
  if (fi < 0) return String("");

  SyncMapFragment frag;
  if (!fragmentAt((uint16_t)fi, frag)) return String("");

  uint16_t fragmentNumber = (uint16_t)(fi + 1);   // OPF numbering, 1-based

  int bi = blockContaining(frag.blockBase, frag.blockLen, offset);
  if (bi < 0) {
    // No block granularity here — address the document itself.
    return buildXPointer(fragmentNumber, String(""), String(""), 0, 0);
  }

  SyncMapBlock b;
  if (!blockAt((uint32_t)bi, b)) return String("");

  String path = poolString(b.pathId);
  String name = poolString(b.nameId);
  if (name.length() == 0) {
    return buildXPointer(fragmentNumber, String(""), String(""), 0, 0);
  }

  uint32_t within = (offset > b.textStart) ? (offset - b.textStart) : 0;
  return buildXPointer(fragmentNumber, path, name, b.ordinal, within);
}
