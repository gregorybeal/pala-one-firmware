#include "src/pure/sync_map_codec.h"

uint16_t syncMapReadU16(const uint8_t* p) {
  return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

uint32_t syncMapReadU32(const uint8_t* p) {
  return (uint32_t)p[0]
       | ((uint32_t)p[1] << 8)
       | ((uint32_t)p[2] << 16)
       | ((uint32_t)p[3] << 24);
}

bool parseSyncMapHeader(const uint8_t* buf, size_t n, SyncMapHeader& out) {
  if (buf == nullptr || n < SYNC_MAP_HEADER_BYTES) return false;

  SyncMapHeader h;
  h.magic         = syncMapReadU32(buf + 0);
  h.textSize      = syncMapReadU32(buf + 4);
  h.fragCount     = syncMapReadU16(buf + 8);
  h.poolCount     = syncMapReadU16(buf + 10);
  h.blockCount    = syncMapReadU32(buf + 12);
  h.poolBlobBytes = syncMapReadU32(buf + 16);
  // bytes 20..23 reserved

  if (h.magic != SYNC_MAP_MAGIC) return false;
  if (h.fragCount == 0 || h.fragCount > SYNC_MAP_MAX_FRAGMENTS) return false;
  if (h.blockCount > SYNC_MAP_MAX_BLOCKS) return false;
  if (h.poolCount > SYNC_MAP_MAX_POOL) return false;
  if (h.poolBlobBytes > SYNC_MAP_MAX_POOL_BLOB) return false;
  // A pool index with no blob, or a blob with no index, is a truncated build.
  if ((h.poolCount == 0) != (h.poolBlobBytes == 0)) return false;

  out = h;
  return true;
}

size_t syncMapFragTableAt(const SyncMapHeader&) {
  return SYNC_MAP_HEADER_BYTES;
}

size_t syncMapPoolIndexAt(const SyncMapHeader& h) {
  return syncMapFragTableAt(h) + (size_t)h.fragCount * SYNC_MAP_FRAG_BYTES;
}

size_t syncMapPoolBlobAt(const SyncMapHeader& h) {
  return syncMapPoolIndexAt(h) + (size_t)h.poolCount * sizeof(uint32_t);
}

size_t syncMapBlockTableAt(const SyncMapHeader& h) {
  return syncMapPoolBlobAt(h) + (size_t)h.poolBlobBytes;
}

size_t syncMapTotalBytes(const SyncMapHeader& h) {
  return syncMapBlockTableAt(h) + (size_t)h.blockCount * SYNC_MAP_BLOCK_BYTES;
}

SyncMapFragment decodeSyncMapFragment(const uint8_t* p) {
  SyncMapFragment f;
  f.textStart   = syncMapReadU32(p + 0);
  f.blockBase   = syncMapReadU32(p + 4);
  f.blockLen    = syncMapReadU16(p + 8);
  f.linearIndex = syncMapReadU16(p + 10);
  return f;
}

SyncMapBlock decodeSyncMapBlock(const uint8_t* p) {
  SyncMapBlock b;
  b.textStart = syncMapReadU32(p + 0);
  b.pathId    = syncMapReadU16(p + 4);
  b.ordinal   = syncMapReadU16(p + 6);
  b.nameId    = syncMapReadU16(p + 8);
  return b;
}
