#include "test_framework.h"
#include "pure/wifi_list_codec.h"

#include <cstring>
#include <string>

static void addRaw(WifiList& l, const char* ssid, const char* pass) {
  CHECK(wifiListUpsert(l, ssid, pass));
}

static std::string repeat(char c, int n) { return std::string((size_t)n, c); }

// ----------------------------------------------------------------------------
//  Encode / decode
// ----------------------------------------------------------------------------
TEST_CASE("wifi list round-trips through the blob") {
  WifiList in;
  addRaw(in, "Home Network", "hunter2hunter2");
  addRaw(in, "Cafe", "");                        // open network
  addRaw(in, "Phone Hotspot", "abc123");

  uint8_t buf[WIFI_LIST_ENCODED_MAX_SIZE];
  size_t n = encodeWifiList(in, buf);

  WifiList out;
  REQUIRE(decodeWifiList(buf, n, out));
  CHECK_EQ((int)out.count, 3);
  CHECK_EQ(std::string(out.nets[0].ssid), std::string("Home Network"));
  CHECK_EQ(std::string(out.nets[0].pass), std::string("hunter2hunter2"));
  CHECK_EQ(std::string(out.nets[1].ssid), std::string("Cafe"));
  CHECK_EQ(std::string(out.nets[1].pass), std::string(""));
  CHECK_EQ(std::string(out.nets[2].ssid), std::string("Phone Hotspot"));
}

TEST_CASE("an empty list encodes and decodes as empty") {
  WifiList in;
  uint8_t buf[WIFI_LIST_ENCODED_MAX_SIZE];
  size_t n = encodeWifiList(in, buf);
  CHECK_EQ(n, (size_t)2);   // version + count only

  WifiList out;
  addRaw(out, "stale", "x");
  REQUIRE(decodeWifiList(buf, n, out));
  CHECK_EQ((int)out.count, 0);
}

TEST_CASE("maximum-length SSID and passphrase survive the round trip") {
  std::string ssid = repeat('S', MAX_WIFI_SSID);
  std::string pass = repeat('p', MAX_WIFI_PASS);
  WifiList in;
  addRaw(in, ssid.c_str(), pass.c_str());

  uint8_t buf[WIFI_LIST_ENCODED_MAX_SIZE];
  size_t n = encodeWifiList(in, buf);
  CHECK(n <= WIFI_LIST_ENCODED_MAX_SIZE);

  WifiList out;
  REQUIRE(decodeWifiList(buf, n, out));
  CHECK_EQ(std::string(out.nets[0].ssid), ssid);
  CHECK_EQ(std::string(out.nets[0].pass), pass);
}

TEST_CASE("a full list stays inside the declared max blob size") {
  WifiList in;
  std::string ssid = repeat('S', MAX_WIFI_SSID);
  std::string pass = repeat('p', MAX_WIFI_PASS);
  for (int i = 0; i < MAX_WIFI_NETWORKS; i++) {
    // Distinct SSIDs, all at the length limit.
    std::string s = ssid;
    s[0] = (char)('A' + i);
    addRaw(in, s.c_str(), pass.c_str());
  }
  CHECK_EQ((int)in.count, (int)MAX_WIFI_NETWORKS);

  uint8_t buf[WIFI_LIST_ENCODED_MAX_SIZE];
  size_t n = encodeWifiList(in, buf);
  CHECK_EQ(n, WIFI_LIST_ENCODED_MAX_SIZE);

  WifiList out;
  REQUIRE(decodeWifiList(buf, n, out));
  CHECK_EQ((int)out.count, (int)MAX_WIFI_NETWORKS);
}

// ----------------------------------------------------------------------------
//  Decode rejection — a half-read credential list would look connectable and
//  never work, so every one of these must leave the list empty.
// ----------------------------------------------------------------------------
TEST_CASE("decode rejects an absent or too-short blob") {
  WifiList out;
  CHECK(!decodeWifiList(nullptr, 0, out));
  CHECK_EQ((int)out.count, 0);

  uint8_t buf[1] = {WIFI_LIST_BLOB_VERSION};
  CHECK(!decodeWifiList(buf, 1, out));
  CHECK_EQ((int)out.count, 0);
}

TEST_CASE("decode rejects a wrong version byte") {
  WifiList in;
  addRaw(in, "Home", "pw");
  uint8_t buf[WIFI_LIST_ENCODED_MAX_SIZE];
  size_t n = encodeWifiList(in, buf);

  buf[0] = (uint8_t)(WIFI_LIST_BLOB_VERSION + 1);
  WifiList out;
  CHECK(!decodeWifiList(buf, n, out));
  CHECK_EQ((int)out.count, 0);
}

TEST_CASE("decode rejects a blob truncated at every length") {
  WifiList in;
  addRaw(in, "Home Network", "hunter2");
  addRaw(in, "Hotspot", "abc");
  uint8_t buf[WIFI_LIST_ENCODED_MAX_SIZE];
  size_t n = encodeWifiList(in, buf);

  // Every proper prefix must be refused, not partially accepted.
  for (size_t cut = 2; cut < n; cut++) {
    WifiList out;
    bool ok = decodeWifiList(buf, cut, out);
    CHECK(!ok);
    if (ok) break;
    CHECK_EQ((int)out.count, 0);
  }
}

TEST_CASE("decode rejects an impossible count") {
  uint8_t buf[8] = {WIFI_LIST_BLOB_VERSION, (uint8_t)(MAX_WIFI_NETWORKS + 1), 0, 0, 0, 0, 0, 0};
  WifiList out;
  CHECK(!decodeWifiList(buf, sizeof(buf), out));
  CHECK_EQ((int)out.count, 0);
}

TEST_CASE("decode rejects an over-long declared field length") {
  // count 1, ssidLen claiming more than the cap
  uint8_t buf[8] = {WIFI_LIST_BLOB_VERSION, 1, (uint8_t)(MAX_WIFI_SSID + 1), 'a', 'b', 'c', 0, 0};
  WifiList out;
  CHECK(!decodeWifiList(buf, sizeof(buf), out));
  CHECK_EQ((int)out.count, 0);
}

TEST_CASE("decode rejects an entry with an empty SSID") {
  // version, count=1, ssidLen=0, passLen=0 — a row that can never connect.
  uint8_t buf[4] = {WIFI_LIST_BLOB_VERSION, 1, 0, 0};
  WifiList out;
  CHECK(!decodeWifiList(buf, sizeof(buf), out));
  CHECK_EQ((int)out.count, 0);
}

TEST_CASE("decode rejects arbitrary garbage") {
  uint8_t buf[16];
  for (size_t i = 0; i < sizeof(buf); i++) buf[i] = (uint8_t)(0xA5 ^ i);
  WifiList out;
  CHECK(!decodeWifiList(buf, sizeof(buf), out));
  CHECK_EQ((int)out.count, 0);
}

// ----------------------------------------------------------------------------
//  List operations
// ----------------------------------------------------------------------------
TEST_CASE("wifiListFind matches exactly and case-sensitively") {
  WifiList l;
  addRaw(l, "Home", "a");
  addRaw(l, "Work", "b");
  CHECK_EQ(wifiListFind(l, "Home"), 0);
  CHECK_EQ(wifiListFind(l, "Work"), 1);
  CHECK_EQ(wifiListFind(l, "home"), -1);      // SSIDs are octet strings
  CHECK_EQ(wifiListFind(l, "Hom"), -1);
  CHECK_EQ(wifiListFind(l, ""), -1);
  CHECK_EQ(wifiListFind(l, nullptr), -1);
}

TEST_CASE("upsert appends a new network") {
  WifiList l;
  CHECK(wifiListUpsert(l, "Home", "pw1"));
  CHECK(wifiListUpsert(l, "Work", "pw2"));
  CHECK_EQ((int)l.count, 2);
  CHECK_EQ(std::string(l.nets[1].ssid), std::string("Work"));
}

TEST_CASE("upsert replaces the password in place, never duplicating") {
  WifiList l;
  addRaw(l, "Home", "old");
  addRaw(l, "Work", "work");
  CHECK(wifiListUpsert(l, "Home", "new"));
  CHECK_EQ((int)l.count, 2);                                  // not 3
  CHECK_EQ(wifiListFind(l, "Home"), 0);                       // position kept
  CHECK_EQ(std::string(l.nets[0].pass), std::string("new"));
  CHECK_EQ(std::string(l.nets[1].pass), std::string("work")); // untouched
}

TEST_CASE("upsert rejects an unusable SSID") {
  WifiList l;
  CHECK(!wifiListUpsert(l, "", "pw"));
  CHECK(!wifiListUpsert(l, nullptr, "pw"));
  CHECK(!wifiListUpsert(l, repeat('x', MAX_WIFI_SSID + 1).c_str(), "pw"));
  CHECK_EQ((int)l.count, 0);
}

TEST_CASE("upsert truncates an over-long passphrase rather than refusing") {
  WifiList l;
  CHECK(wifiListUpsert(l, "Home", repeat('p', MAX_WIFI_PASS + 10).c_str()));
  CHECK_EQ((int)l.count, 1);
  CHECK_EQ(std::strlen(l.nets[0].pass), (size_t)MAX_WIFI_PASS);
}

TEST_CASE("upsert at capacity evicts the oldest entry") {
  WifiList l;
  for (int i = 0; i < MAX_WIFI_NETWORKS; i++) {
    addRaw(l, (std::string("net") + std::to_string(i)).c_str(), "pw");
  }
  CHECK_EQ((int)l.count, (int)MAX_WIFI_NETWORKS);

  CHECK(wifiListUpsert(l, "newcomer", "pw"));
  CHECK_EQ((int)l.count, (int)MAX_WIFI_NETWORKS);
  CHECK_EQ(wifiListFind(l, "net0"), -1);                       // oldest gone
  CHECK_EQ(wifiListFind(l, "net1"), 0);                        // shifted down
  CHECK_EQ(wifiListFind(l, "newcomer"), MAX_WIFI_NETWORKS - 1);
}

TEST_CASE("upsert of a known SSID at capacity does not evict anything") {
  WifiList l;
  for (int i = 0; i < MAX_WIFI_NETWORKS; i++) {
    addRaw(l, (std::string("net") + std::to_string(i)).c_str(), "pw");
  }
  CHECK(wifiListUpsert(l, "net0", "changed"));
  CHECK_EQ((int)l.count, (int)MAX_WIFI_NETWORKS);
  CHECK_EQ(wifiListFind(l, "net0"), 0);
  CHECK_EQ(std::string(l.nets[0].pass), std::string("changed"));
}

TEST_CASE("removeAt shifts the remainder down") {
  WifiList l;
  addRaw(l, "a", "1");
  addRaw(l, "b", "2");
  addRaw(l, "c", "3");
  CHECK(wifiListRemoveAt(l, 1));
  CHECK_EQ((int)l.count, 2);
  CHECK_EQ(std::string(l.nets[0].ssid), std::string("a"));
  CHECK_EQ(std::string(l.nets[1].ssid), std::string("c"));
}

TEST_CASE("removeAt bounds-checks") {
  WifiList l;
  addRaw(l, "a", "1");
  CHECK(!wifiListRemoveAt(l, -1));
  CHECK(!wifiListRemoveAt(l, 1));
  CHECK(!wifiListRemoveAt(l, 99));
  CHECK_EQ((int)l.count, 1);
  CHECK(wifiListRemoveAt(l, 0));
  CHECK_EQ((int)l.count, 0);
  CHECK(!wifiListRemoveAt(l, 0));
}

// ----------------------------------------------------------------------------
//  Legacy migration — users have a working network stored under the old
//  single-credential scheme and must not lose it on upgrade.
// ----------------------------------------------------------------------------
TEST_CASE("migrateLegacyCreds imports the single stored network") {
  WifiList out;
  REQUIRE(migrateLegacyCreds("Home Network", "hunter2", out));
  CHECK_EQ((int)out.count, 1);
  CHECK_EQ(std::string(out.nets[0].ssid), std::string("Home Network"));
  CHECK_EQ(std::string(out.nets[0].pass), std::string("hunter2"));
}

TEST_CASE("migrateLegacyCreds imports an open network with no password") {
  WifiList out;
  REQUIRE(migrateLegacyCreds("Cafe", "", out));
  CHECK_EQ((int)out.count, 1);
  CHECK_EQ(std::string(out.nets[0].pass), std::string(""));
}

TEST_CASE("migrateLegacyCreds reports nothing to import") {
  WifiList out;
  addRaw(out, "stale", "x");
  CHECK(!migrateLegacyCreds("", "pw", out));
  CHECK_EQ((int)out.count, 0);   // and clears whatever was there
}

TEST_CASE("a migrated list survives the blob round trip") {
  WifiList migrated;
  REQUIRE(migrateLegacyCreds("Home Network", "hunter2", migrated));

  uint8_t buf[WIFI_LIST_ENCODED_MAX_SIZE];
  size_t n = encodeWifiList(migrated, buf);
  WifiList out;
  REQUIRE(decodeWifiList(buf, n, out));
  CHECK_EQ((int)out.count, 1);
  CHECK_EQ(std::string(out.nets[0].ssid), std::string("Home Network"));
  CHECK_EQ(std::string(out.nets[0].pass), std::string("hunter2"));
}
