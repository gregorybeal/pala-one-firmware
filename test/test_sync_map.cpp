#include "test_framework.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>

#include "src/state.h"                  // host File / FS shim (test/hostfs)
#include "pure/hashing.h"
#include "pure/xpointer.h"
#include "storage/sync_map.h"

// ============================================================================
//  SyncMap against a real map file.
//
//  The bytes below are not hand-written: they are what web/epub_js.h's
//  buildSyncMap() actually produced for a three-chapter EPUB whose spine also
//  carries a linear="no" cover and a navigation document, flattened to the
//  563 bytes the firmware would store. Offsets in the expectations are the
//  real paragraph positions in that text.
//
//    frag[0]  cover, linear="no"   start 0    linearIndex 0   (no text)
//    frag[1]  nav document         start 0    linearIndex 1
//    frag[2]  chapter one          start 59   linearIndex 2
//    frag[3]  chapter two          start 235  linearIndex 3
//    frag[4]  chapter three        start 420  linearIndex 4
//
//  Chapter bodies are <div class="chap"><h1>..</h1><p>..</p>x3</div>, so the
//  canonical parent path is /body[1]/div[1].
// ============================================================================

static const unsigned char kMapBytes[] = {
  0x31, 0x4d, 0x53, 0x50, 0x33, 0x02, 0x00, 0x00, 0x05, 0x00, 0x05, 0x00,
  0x10, 0x00, 0x00, 0x00, 0x1d, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00,
  0x3b, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x05, 0x00, 0x02, 0x00,
  0xeb, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x05, 0x00, 0x03, 0x00,
  0xa4, 0x01, 0x00, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x05, 0x00, 0x04, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00,
  0x0c, 0x00, 0x00, 0x00, 0x1b, 0x00, 0x00, 0x00, 0x2f, 0x62, 0x6f, 0x64,
  0x79, 0x5b, 0x31, 0x5d, 0x70, 0x64, 0x69, 0x76, 0x2f, 0x62, 0x6f, 0x64,
  0x79, 0x5b, 0x31, 0x5d, 0x2f, 0x64, 0x69, 0x76, 0x5b, 0x31, 0x5d, 0x68,
  0x31, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x3b,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x02, 0x00, 0x3b, 0x00, 0x00,
  0x00, 0x03, 0x00, 0x01, 0x00, 0x04, 0x00, 0x48, 0x00, 0x00, 0x00, 0x03,
  0x00, 0x01, 0x00, 0x01, 0x00, 0x85, 0x00, 0x00, 0x00, 0x03, 0x00, 0x02,
  0x00, 0x01, 0x00, 0xb1, 0x00, 0x00, 0x00, 0x03, 0x00, 0x03, 0x00, 0x01,
  0x00, 0xeb, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x02, 0x00, 0xeb,
  0x00, 0x00, 0x00, 0x03, 0x00, 0x01, 0x00, 0x04, 0x00, 0xf8, 0x00, 0x00,
  0x00, 0x03, 0x00, 0x01, 0x00, 0x01, 0x00, 0x3a, 0x01, 0x00, 0x00, 0x03,
  0x00, 0x02, 0x00, 0x01, 0x00, 0x6c, 0x01, 0x00, 0x00, 0x03, 0x00, 0x03,
  0x00, 0x01, 0x00, 0xa4, 0x01, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x02,
  0x00, 0xa4, 0x01, 0x00, 0x00, 0x03, 0x00, 0x01, 0x00, 0x04, 0x00, 0xb3,
  0x01, 0x00, 0x00, 0x03, 0x00, 0x01, 0x00, 0x01, 0x00, 0xde, 0x01, 0x00,
  0x00, 0x03, 0x00, 0x02, 0x00, 0x01, 0x00, 0x0c, 0x02, 0x00, 0x00, 0x03,
  0x00, 0x03, 0x00, 0x01, 0x00,};

static const uint32_t kTextSize = 563;
static const char*    kBookPath = "/books/The Wind in the Willows - Kenneth Grahame.txt";

// Offsets of the paragraphs the expectations below refer to.
static const uint32_t kCh1Start = 59;    // <div>, and the <h1> that opens it
static const uint32_t kCh1P1    = 72;    // MARK1A
static const uint32_t kCh1P2    = 133;   // MARK1B
static const uint32_t kCh1P3    = 177;   // MARK1C
static const uint32_t kCh2P2    = 314;   // MARK2B
static const uint32_t kCh3Start = 420;
static const uint32_t kNavP1    = 0;

namespace {

// Lays the fixture down where SyncMap will look for it, under a scratch root.
struct MapFixture {
  std::string root;

  explicit MapFixture(uint32_t textSize = kTextSize) {
    char tmpl[] = "/tmp/pala_syncmap_XXXXXX";
    root = mkdtemp(tmpl);
    hostFsSetRoot(root);

    std::string path = root + "/sm_" + std::string(prefKeyForBook(String(kBookPath)).c_str()) + ".bin";
    FILE* f = fopen(path.c_str(), "wb");
    if (f) {
      // The stamp is the first field the loader checks; vary it to exercise
      // rejection without rebuilding the whole map.
      unsigned char buf[sizeof(kMapBytes)];
      memcpy(buf, kMapBytes, sizeof(buf));
      buf[4] = (unsigned char)(textSize & 0xff);
      buf[5] = (unsigned char)((textSize >> 8) & 0xff);
      buf[6] = (unsigned char)((textSize >> 16) & 0xff);
      buf[7] = (unsigned char)((textSize >> 24) & 0xff);
      fwrite(buf, 1, sizeof(buf), f);
      fclose(f);
    }
    written = path;
  }
  ~MapFixture() {
    remove(written.c_str());
    rmdir(root.c_str());
  }
  std::string written;
};

uint32_t resolve(SyncMap& m, const char* pointer,
                 SyncMap::FragmentMode mode = SyncMap::FragmentMode::Opf) {
  XPointer xp;
  if (!parseXPointer(pointer, xp)) return 0xFFFFFFFFu;
  uint32_t out = 0;
  if (!m.offsetForXPointer(xp, mode, out)) return 0xFFFFFFFEu;
  return out;
}

}  // namespace

// ----------------------------------------------------------------------------
//  Opening
// ----------------------------------------------------------------------------
TEST_CASE("SyncMap opens a map built by the upload page") {
  MapFixture fx;
  SyncMap m;
  REQUIRE(m.open(kBookPath, kTextSize));
  CHECK(m.isOpen());
}

TEST_CASE("SyncMap rejects a map stamped for different text") {
  // The book was re-uploaded, or edited: the offsets no longer describe it.
  MapFixture fx(kTextSize + 1);
  SyncMap m;
  CHECK(!m.open(kBookPath, kTextSize));
  CHECK(!m.isOpen());
}

TEST_CASE("SyncMap reports no map rather than failing loudly") {
  char tmpl[] = "/tmp/pala_syncmap_XXXXXX";
  std::string root = mkdtemp(tmpl);
  hostFsSetRoot(root);
  SyncMap m;
  CHECK(!m.open(kBookPath, kTextSize));
  rmdir(root.c_str());
}

// ----------------------------------------------------------------------------
//  KOReader -> Pala
// ----------------------------------------------------------------------------
TEST_CASE("offsetForXPointer lands on the addressed paragraph") {
  MapFixture fx;
  SyncMap m;
  REQUIRE(m.open(kBookPath, kTextSize));

  CHECK_EQ(resolve(m, "/body/DocFragment[3]/body/div/p[1].0"), kCh1P1);
  CHECK_EQ(resolve(m, "/body/DocFragment[3]/body/div/p[2].0"), kCh1P2);
  CHECK_EQ(resolve(m, "/body/DocFragment[3]/body/div/p[3].0"), kCh1P3);
  // The <h1> opens the chapter, so it shares the <div>'s offset.
  CHECK_EQ(resolve(m, "/body/DocFragment[3]/body/div/h1[1].0"), kCh1Start);
}

TEST_CASE("offsetForXPointer accepts the text() form KOReader also sends") {
  MapFixture fx;
  SyncMap m;
  REQUIRE(m.open(kBookPath, kTextSize));
  CHECK_EQ(resolve(m, "/body/DocFragment[3]/body/div/p[2]/text().15"), kCh1P2);
}

TEST_CASE("offsetForXPointer walks up when the deepest steps are unknown") {
  MapFixture fx;
  SyncMap m;
  REQUIRE(m.open(kBookPath, kTextSize));
  // crengine autoboxes and wraps inline content; the map has no <span>, so
  // the lookup must fall back to the enclosing <p> rather than give up.
  CHECK_EQ(resolve(m, "/body/DocFragment[3]/body/div/p[2]/span[1]/text().4"), kCh1P2);
  CHECK_EQ(resolve(m, "/body/DocFragment[3]/body/div/p[2]/em[2]/i[1].0"), kCh1P2);
}

TEST_CASE("an unresolvable path still lands in the right document") {
  MapFixture fx;
  SyncMap m;
  REQUIRE(m.open(kBookPath, kTextSize));
  // Nothing in chapter one matches, but the fragment itself is known.
  CHECK_EQ(resolve(m, "/body/DocFragment[3]/body/section[9]/p[99].0"), kCh1Start);
}

TEST_CASE("a bare DocFragment resolves to that document's start") {
  MapFixture fx;
  SyncMap m;
  REQUIRE(m.open(kBookPath, kTextSize));
  CHECK_EQ(resolve(m, "/body/DocFragment[5]"), kCh3Start);
  CHECK_EQ(resolve(m, "/body/DocFragment[2]"), kNavP1);
}

TEST_CASE("Linear mode numbers over the itemrefs that carry text") {
  MapFixture fx;
  SyncMap m;
  REQUIRE(m.open(kBookPath, kTextSize));

  // DocFragment[3] under OPF numbering is chapter one (cover, nav, ch1).
  // Under linear-only numbering it is chapter two, because the linear="no"
  // cover does not get a number. Resolving both is exactly why SyncScreen
  // tries each and keeps whichever sits closer to the server's percentage.
  CHECK_EQ(resolve(m, "/body/DocFragment[3]/body/div/p[2].0",
                   SyncMap::FragmentMode::Opf), kCh1P2);
  CHECK_EQ(resolve(m, "/body/DocFragment[3]/body/div/p[2].0",
                   SyncMap::FragmentMode::Linear), kCh2P2);
}

TEST_CASE("a fragment number past the end resolves to nothing") {
  MapFixture fx;
  SyncMap m;
  REQUIRE(m.open(kBookPath, kTextSize));
  CHECK_EQ(resolve(m, "/body/DocFragment[99]/body/div/p[1].0"), 0xFFFFFFFEu);
}

// ----------------------------------------------------------------------------
//  Pala -> KOReader
// ----------------------------------------------------------------------------
TEST_CASE("xpointerForOffset names the block containing the offset") {
  MapFixture fx;
  SyncMap m;
  REQUIRE(m.open(kBookPath, kTextSize));

  CHECK_EQ(m.xpointerForOffset(kCh1P2),
           String("/body/DocFragment[3]/body[1]/div[1]/p[2].0"));
  // Mid-paragraph keeps the distance from the block start.
  CHECK_EQ(m.xpointerForOffset(kCh1P2 + 7),
           String("/body/DocFragment[3]/body[1]/div[1]/p[2].7"));
}

TEST_CASE("xpointerForOffset prefers the deepest block at a shared offset") {
  MapFixture fx;
  SyncMap m;
  REQUIRE(m.open(kBookPath, kTextSize));
  // The <div> and its opening <h1> both start here; the <h1> is the tighter
  // answer and is what crengine would point at.
  CHECK_EQ(m.xpointerForOffset(kCh1Start),
           String("/body/DocFragment[3]/body[1]/div[1]/h1[1].0"));
}

TEST_CASE("xpointerForOffset skips itemrefs that carry no text") {
  MapFixture fx;
  SyncMap m;
  REQUIRE(m.open(kBookPath, kTextSize));
  // The linear="no" cover shares offset 0 with the nav document. Pointing at
  // the cover would address a document KOReader renders but we never stored.
  CHECK_EQ(m.xpointerForOffset(0),
           String("/body/DocFragment[2]/body[1]/p[1].0"));
}

// ----------------------------------------------------------------------------
//  The two directions agree
// ----------------------------------------------------------------------------
TEST_CASE("every block round-trips through both directions") {
  MapFixture fx;
  SyncMap m;
  REQUIRE(m.open(kBookPath, kTextSize));

  const uint32_t offsets[] = { 0, 59, 72, 133, 177, 235, 248, 314, 364,
                               420, 435, 478, 524 };
  for (size_t i = 0; i < sizeof(offsets) / sizeof(offsets[0]); i++) {
    String ptr = m.xpointerForOffset(offsets[i]);
    CHECK(ptr.length() > 0);
    CHECK_EQ(resolve(m, ptr.c_str()), offsets[i]);
  }
}

TEST_CASE("an offset past the last block still yields a usable pointer") {
  MapFixture fx;
  SyncMap m;
  REQUIRE(m.open(kBookPath, kTextSize));
  String ptr = m.xpointerForOffset(kTextSize - 1);
  CHECK(ptr.length() > 0);
  // It must at least land in the final chapter.
  CHECK(resolve(m, ptr.c_str()) >= kCh3Start);
}
