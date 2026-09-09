#include "test_framework.h"
#include "map_kv_store.h"
#include "storage/book_metadata.h"

TEST_CASE("loadBookmarks returns empty when key is absent") {
  MapKvStore kv;
  Bookmarks bm = loadBookmarks(kv, "b_deadbeef");
  CHECK_EQ(bm.count, 0);
}

TEST_CASE("saveBookmarks + loadBookmarks round-trip") {
  MapKvStore kv;
  Bookmarks bm;
  bm.count = 2;
  bm.pages[0] = 5;   bm.offsets[0] = 500;
  bm.pages[1] = 42;  bm.offsets[1] = 4200;

  saveBookmarks(kv, "b_test", bm);

  Bookmarks back = loadBookmarks(kv, "b_test");
  CHECK_EQ(back.count, 2);
  CHECK_EQ(back.pages[0], 5);
  CHECK_EQ(back.offsets[0], 500u);
  CHECK_EQ(back.pages[1], 42);
  CHECK_EQ(back.offsets[1], 4200u);
}

TEST_CASE("loadSavedPage defaults to 0") {
  MapKvStore kv;
  CHECK_EQ(loadSavedPage(kv, "b_new"), 0);
}

TEST_CASE("saveSavedPage + loadSavedPage round-trip") {
  MapKvStore kv;
  saveSavedPage(kv, "b_test", 137);
  CHECK_EQ(loadSavedPage(kv, "b_test"), 137);
}

TEST_CASE("loadSavedPage clamps negative stored values to 0") {
  MapKvStore kv;
  saveSavedPage(kv, "b_test", -99);
  CHECK_EQ(loadSavedPage(kv, "b_test"), 0);
}

TEST_CASE("clearBookMetadata removes progress and bookmarks") {
  MapKvStore kv;
  saveSavedPage(kv, "b_test", 42);
  Bookmarks bm;
  bm.count = 1;
  bm.pages[0] = 5;
  bm.offsets[0] = 100;
  saveBookmarks(kv, "b_test", bm);

  CHECK(kv.has("b_test_p"));
  CHECK(kv.has("b_test_bm"));

  clearBookMetadata(kv, "b_test");
  CHECK(!kv.has("b_test_p"));
  CHECK(!kv.has("b_test_bm"));
}

TEST_CASE("renameBookMetadata moves progress and bookmarks") {
  MapKvStore kv;
  saveSavedPage(kv, "b_old", 50);
  Bookmarks bm;
  bm.count = 1;
  bm.pages[0] = 7;
  bm.offsets[0] = 700;
  saveBookmarks(kv, "b_old", bm);

  renameBookMetadata(kv, "b_old", "b_new");

  // Old keys gone
  CHECK(!kv.has("b_old_p"));
  CHECK(!kv.has("b_old_bm"));
  // New keys hold the data
  CHECK_EQ(loadSavedPage(kv, "b_new"), 50);
  Bookmarks moved = loadBookmarks(kv, "b_new");
  CHECK_EQ(moved.count, 1);
  CHECK_EQ(moved.pages[0], 7);
}

TEST_CASE("renameBookMetadata is a no-op when neither key is set") {
  MapKvStore kv;
  size_t before = kv.entryCount();
  renameBookMetadata(kv, "missing_old", "missing_new");
  CHECK_EQ(kv.entryCount(), before);
}

TEST_CASE("saveBookmarks overwrites prior content cleanly") {
  MapKvStore kv;
  Bookmarks first;
  first.count = 3;
  for (int i = 0; i < 3; i++) {
    first.pages[i] = (uint16_t)(i + 1);
    first.offsets[i] = (uint32_t)((i + 1) * 10);
  }
  saveBookmarks(kv, "b_x", first);

  Bookmarks second;
  second.count = 1;
  second.pages[0] = 99;
  second.offsets[0] = 9900;
  saveBookmarks(kv, "b_x", second);

  Bookmarks back = loadBookmarks(kv, "b_x");
  CHECK_EQ(back.count, 1);
  CHECK_EQ(back.pages[0], 99);
}

// ----------------------------------------------------------------------------
//  KOReader sync document id — shares the per-book lifecycle with progress
//  and bookmarks, so it is tested against the same store.
// ----------------------------------------------------------------------------
static void fillDoc(uint8_t doc[KOSYNC_DOC_BYTES], uint8_t seed) {
  for (size_t i = 0; i < KOSYNC_DOC_BYTES; i++) doc[i] = (uint8_t)(seed + i);
}

TEST_CASE("kosync doc id round-trips through the store") {
  MapKvStore kv;
  uint8_t in[KOSYNC_DOC_BYTES];
  fillDoc(in, 0x10);
  saveKosyncDoc(kv, "b_1", in);

  uint8_t out[KOSYNC_DOC_BYTES];
  CHECK(loadKosyncDoc(kv, "b_1", out));
  for (size_t i = 0; i < KOSYNC_DOC_BYTES; i++) CHECK_EQ((int)out[i], (int)in[i]);
  CHECK(kosyncDocIsSet(out));
}

TEST_CASE("loading an absent kosync doc id yields the unset sentinel") {
  MapKvStore kv;
  uint8_t out[KOSYNC_DOC_BYTES];
  for (size_t i = 0; i < KOSYNC_DOC_BYTES; i++) out[i] = 0xAA;
  CHECK(!loadKosyncDoc(kv, "b_missing", out));
  // Must be zero-filled, not left as caller garbage, so kosyncDocIsSet works.
  CHECK(!kosyncDocIsSet(out));
}

TEST_CASE("saving an all-zero kosync doc id removes the key") {
  MapKvStore kv;
  uint8_t real_[KOSYNC_DOC_BYTES];
  fillDoc(real_, 0x40);
  saveKosyncDoc(kv, "b_2", real_);

  uint8_t zero[KOSYNC_DOC_BYTES] = {0};
  saveKosyncDoc(kv, "b_2", zero);

  uint8_t out[KOSYNC_DOC_BYTES];
  CHECK(!loadKosyncDoc(kv, "b_2", out));
  CHECK(!kosyncDocIsSet(out));
}

TEST_CASE("clearBookMetadata drops the kosync doc id too") {
  MapKvStore kv;
  uint8_t in[KOSYNC_DOC_BYTES];
  fillDoc(in, 0x21);
  saveKosyncDoc(kv, "b_3", in);
  saveSavedOffset(kv, "b_3", 4242);

  clearBookMetadata(kv, "b_3");

  uint8_t out[KOSYNC_DOC_BYTES];
  CHECK(!loadKosyncDoc(kv, "b_3", out));
  CHECK_EQ(loadSavedOffset(kv, "b_3"), kOffsetUnset);
}

TEST_CASE("renameBookMetadata carries the kosync doc id to the new key") {
  MapKvStore kv;
  uint8_t in[KOSYNC_DOC_BYTES];
  fillDoc(in, 0x77);
  saveKosyncDoc(kv, "b_old", in);

  renameBookMetadata(kv, "b_old", "b_new");

  uint8_t out[KOSYNC_DOC_BYTES];
  CHECK(loadKosyncDoc(kv, "b_new", out));
  for (size_t i = 0; i < KOSYNC_DOC_BYTES; i++) CHECK_EQ((int)out[i], (int)in[i]);
  // The old key must not linger — a later book hashing to it would inherit
  // someone else's sync identity.
  uint8_t stale[KOSYNC_DOC_BYTES];
  CHECK(!loadKosyncDoc(kv, "b_old", stale));
}

TEST_CASE("renameBookMetadata is a no-op when no doc id was stored") {
  MapKvStore kv;
  saveSavedOffset(kv, "b_a", 9);
  renameBookMetadata(kv, "b_a", "b_b");
  uint8_t out[KOSYNC_DOC_BYTES];
  CHECK(!loadKosyncDoc(kv, "b_b", out));
  CHECK_EQ(loadSavedOffset(kv, "b_b"), 9u);
}
