#include "test_framework.h"
#include "pure/sync_map_codec.h"

#include <vector>

// Mirrors what web/epub_js.h's buildSyncMap() writes, so the two encodings
// are checked against each other rather than against a restatement of the
// same field offsets.
namespace {

struct FragIn  { uint32_t start; uint32_t base; uint16_t len; uint16_t lin; };
struct BlockIn { uint32_t start; uint16_t path; uint16_t ord; uint16_t name; };

void putU16(std::vector<uint8_t>& v, size_t at, uint16_t x) {
  v[at]     = (uint8_t)(x & 0xff);
  v[at + 1] = (uint8_t)((x >> 8) & 0xff);
}
void putU32(std::vector<uint8_t>& v, size_t at, uint32_t x) {
  v[at]     = (uint8_t)(x & 0xff);
  v[at + 1] = (uint8_t)((x >> 8) & 0xff);
  v[at + 2] = (uint8_t)((x >> 16) & 0xff);
  v[at + 3] = (uint8_t)((x >> 24) & 0xff);
}

std::vector<uint8_t> buildMap(uint32_t textSize,
                              const std::vector<FragIn>& frags,
                              const std::vector<std::string>& pool,
                              const std::vector<BlockIn>& blocks) {
  size_t blobLen = 0;
  for (size_t i = 0; i < pool.size(); i++) blobLen += pool[i].size();

  size_t total = SYNC_MAP_HEADER_BYTES
               + frags.size() * SYNC_MAP_FRAG_BYTES
               + pool.size() * 4
               + blobLen
               + blocks.size() * SYNC_MAP_BLOCK_BYTES;

  std::vector<uint8_t> v(total, 0);
  putU32(v, 0,  SYNC_MAP_MAGIC);
  putU32(v, 4,  textSize);
  putU16(v, 8,  (uint16_t)frags.size());
  putU16(v, 10, (uint16_t)pool.size());
  putU32(v, 12, (uint32_t)blocks.size());
  putU32(v, 16, (uint32_t)blobLen);
  putU32(v, 20, 0);

  size_t p = SYNC_MAP_HEADER_BYTES;
  for (size_t i = 0; i < frags.size(); i++) {
    putU32(v, p,     frags[i].start);
    putU32(v, p + 4, frags[i].base);
    putU16(v, p + 8, frags[i].len);
    putU16(v, p + 10, frags[i].lin);
    p += SYNC_MAP_FRAG_BYTES;
  }

  size_t blobAt = p + pool.size() * 4;
  size_t at = 0;
  for (size_t i = 0; i < pool.size(); i++) {
    putU32(v, p, (uint32_t)at);
    p += 4;
    for (size_t k = 0; k < pool[i].size(); k++) v[blobAt + at + k] = (uint8_t)pool[i][k];
    at += pool[i].size();
  }

  p = blobAt + blobLen;
  for (size_t i = 0; i < blocks.size(); i++) {
    putU32(v, p,     blocks[i].start);
    putU16(v, p + 4, blocks[i].path);
    putU16(v, p + 6, blocks[i].ord);
    putU16(v, p + 8, blocks[i].name);
    p += SYNC_MAP_BLOCK_BYTES;
  }
  return v;
}

}  // namespace

// ----------------------------------------------------------------------------
TEST_CASE("parseSyncMapHeader reads a well-formed map") {
  std::vector<FragIn>  frags  = { {0, 0, 2, 1}, {500, 2, 1, 2} };
  std::vector<std::string> pool = { "/body[1]", "p", "h1" };
  std::vector<BlockIn> blocks = { {0, 0, 1, 1}, {120, 0, 2, 1}, {500, 0, 1, 2} };
  std::vector<uint8_t> v = buildMap(4096, frags, pool, blocks);

  SyncMapHeader h;
  REQUIRE(parseSyncMapHeader(v.data(), v.size(), h));
  CHECK_EQ(h.textSize, (uint32_t)4096);
  CHECK_EQ(h.fragCount, (uint16_t)2);
  CHECK_EQ(h.poolCount, (uint16_t)3);
  CHECK_EQ(h.blockCount, (uint32_t)3);
  CHECK_EQ(h.poolBlobBytes, (uint32_t)11);   // 8 + 1 + 2
  CHECK_EQ(syncMapTotalBytes(h), v.size());
}

TEST_CASE("section offsets follow the counts") {
  std::vector<FragIn>  frags  = { {0, 0, 1, 1} };
  std::vector<std::string> pool = { "/body[1]", "p" };
  std::vector<BlockIn> blocks = { {7, 0, 1, 1} };
  std::vector<uint8_t> v = buildMap(64, frags, pool, blocks);

  SyncMapHeader h;
  REQUIRE(parseSyncMapHeader(v.data(), v.size(), h));
  CHECK_EQ(syncMapFragTableAt(h),  (size_t)SYNC_MAP_HEADER_BYTES);
  CHECK_EQ(syncMapPoolIndexAt(h),  (size_t)(SYNC_MAP_HEADER_BYTES + 12));
  CHECK_EQ(syncMapPoolBlobAt(h),   (size_t)(SYNC_MAP_HEADER_BYTES + 12 + 8));
  CHECK_EQ(syncMapBlockTableAt(h), (size_t)(SYNC_MAP_HEADER_BYTES + 12 + 8 + 9));
}

TEST_CASE("records decode at the offsets the header implies") {
  std::vector<FragIn>  frags  = { {0, 0, 2, 1}, {500, 2, 1, 2} };
  std::vector<std::string> pool = { "/body[1]", "p", "h1" };
  std::vector<BlockIn> blocks = { {0, 0, 1, 1}, {120, 0, 2, 1}, {500, 0, 1, 2} };
  std::vector<uint8_t> v = buildMap(4096, frags, pool, blocks);

  SyncMapHeader h;
  REQUIRE(parseSyncMapHeader(v.data(), v.size(), h));

  SyncMapFragment f1 = decodeSyncMapFragment(
      v.data() + syncMapFragTableAt(h) + SYNC_MAP_FRAG_BYTES);
  CHECK_EQ(f1.textStart, (uint32_t)500);
  CHECK_EQ(f1.blockBase, (uint32_t)2);
  CHECK_EQ(f1.blockLen, (uint16_t)1);
  CHECK_EQ(f1.linearIndex, (uint16_t)2);

  SyncMapBlock b1 = decodeSyncMapBlock(
      v.data() + syncMapBlockTableAt(h) + SYNC_MAP_BLOCK_BYTES);
  CHECK_EQ(b1.textStart, (uint32_t)120);
  CHECK_EQ(b1.pathId, (uint16_t)0);
  CHECK_EQ(b1.ordinal, (uint16_t)2);
  CHECK_EQ(b1.nameId, (uint16_t)1);
}

TEST_CASE("a fragment with no text still occupies a slot") {
  // linearIndex 0 marks an itemref that contributed nothing — a linear="no"
  // cover page, say. It has to keep its position or OPF-order numbering
  // stops lining up with crengine's.
  std::vector<FragIn>  frags  = { {0, 0, 0, 0}, {0, 0, 1, 1} };
  std::vector<std::string> pool = { "/body[1]", "p" };
  std::vector<BlockIn> blocks = { {0, 0, 1, 1} };
  std::vector<uint8_t> v = buildMap(80, frags, pool, blocks);

  SyncMapHeader h;
  REQUIRE(parseSyncMapHeader(v.data(), v.size(), h));
  SyncMapFragment f0 = decodeSyncMapFragment(v.data() + syncMapFragTableAt(h));
  CHECK_EQ(f0.linearIndex, (uint16_t)0);
  CHECK_EQ(f0.blockLen, (uint16_t)0);
}

// ----------------------------------------------------------------------------
//  Rejection
// ----------------------------------------------------------------------------
TEST_CASE("parseSyncMapHeader rejects junk") {
  std::vector<FragIn>  frags  = { {0, 0, 0, 1} };
  std::vector<std::string> pool;
  std::vector<BlockIn> blocks;
  std::vector<uint8_t> good = buildMap(10, frags, pool, blocks);

  SyncMapHeader h;
  CHECK(!parseSyncMapHeader(nullptr, 0, h));
  CHECK(!parseSyncMapHeader(good.data(), SYNC_MAP_HEADER_BYTES - 1, h));

  std::vector<uint8_t> badMagic = good;
  badMagic[0] ^= 0xff;
  CHECK(!parseSyncMapHeader(badMagic.data(), badMagic.size(), h));

  std::vector<uint8_t> noFrags = good;
  putU16(noFrags, 8, 0);
  CHECK(!parseSyncMapHeader(noFrags.data(), noFrags.size(), h));

  std::vector<uint8_t> tooMany = good;
  putU16(tooMany, 8, (uint16_t)(SYNC_MAP_MAX_FRAGMENTS + 1));
  CHECK(!parseSyncMapHeader(tooMany.data(), tooMany.size(), h));
}

TEST_CASE("parseSyncMapHeader rejects a pool index with no blob") {
  std::vector<FragIn>  frags  = { {0, 0, 0, 1} };
  std::vector<std::string> pool = { "/body[1]" };
  std::vector<BlockIn> blocks;
  std::vector<uint8_t> v = buildMap(10, frags, pool, blocks);

  putU32(v, 16, 0);          // claim an empty blob while keeping the index
  SyncMapHeader h;
  CHECK(!parseSyncMapHeader(v.data(), v.size(), h));
}

TEST_CASE("little-endian readers agree with the writers") {
  uint8_t buf[4] = { 0x78, 0x56, 0x34, 0x12 };
  CHECK_EQ(syncMapReadU16(buf), (uint16_t)0x5678);
  CHECK_EQ(syncMapReadU32(buf), (uint32_t)0x12345678);
}
