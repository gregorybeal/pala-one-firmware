#include "src/hal/kosync.h"

#include <HTTPClient.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <MD5Builder.h>

#include "src/config.h"
#include "src/storage/kosync_settings.h"

// Same Mozilla root bundle the OTA path uses — embedded in libmbedtls.a by
// the IDF. See src/hal/ota.cpp for the original use.
extern const uint8_t x509_crt_imported_bundle_bin_start[] asm("_binary_x509_crt_bundle_start");
extern const uint8_t x509_crt_imported_bundle_bin_end[]   asm("_binary_x509_crt_bundle_end");

namespace Kosync {

static constexpr uint16_t kTimeoutMs = 8000;
static constexpr const char* kAcceptHeader = "application/vnd.koreader.v1+json";

// Bodies are small and fixed-shape; this caps what a hostile or broken
// server can make us allocate.
static constexpr size_t kMaxBodyBytes = 2048;

// ----------------------------------------------------------------------------
//  MD5
// ----------------------------------------------------------------------------
String md5Hex(const String& password) {
  // MD5Builder is part of the Arduino core, so this does not pin us to an
  // mbedTLS major version (the md5 entry points changed shape between 2.x
  // and 3.x). toString() is lowercase hex, which is what kosync expects.
  MD5Builder md5;
  md5.begin();
  md5.add(password);
  md5.calculate();
  return md5.toString();
}

// ----------------------------------------------------------------------------
//  Transport
// ----------------------------------------------------------------------------
//
// A connection is either a WiFiClient (self-hosted http://) or a
// WiFiClientSecure (https://). The secure one is heap-allocated for the same
// reason ota.cpp does it: its TLS context setup uses enough stack to overflow
// the 8 KB loopTask stack even when the connection fails.
namespace {
struct Connection {
  WiFiClient*       plain  = nullptr;
  WiFiClientSecure* secure = nullptr;

  bool open(bool tls) {
    if (tls) {
      secure = new WiFiClientSecure();
      if (!secure) return false;
      size_t len = (size_t)((uintptr_t)x509_crt_imported_bundle_bin_end
                          - (uintptr_t)x509_crt_imported_bundle_bin_start);
      secure->setCACertBundle(x509_crt_imported_bundle_bin_start, len);
      return true;
    }
    plain = new WiFiClient();
    return plain != nullptr;
  }

  // WiFiClientSecure publicly derives from WiFiClient, so both cases hand
  // HTTPClient the same interface.
  WiFiClient& client() { return secure ? *secure : *plain; }

  Connection() = default;
  ~Connection() {
    delete secure;
    delete plain;
  }
  Connection(const Connection&) = delete;
  Connection& operator=(const Connection&) = delete;
};
}  // namespace

static Result classify(int code) {
  if (code == 200 || code == 201 || code == 202) return Result::Ok;
  if (code == 401)                               return Result::Unauthorized;
  if (code == 402)                               return Result::Conflict;
  if (code == 404)                               return Result::NotFound;
  if (code <= 0)                                 return Result::NoNetwork;
  return Result::ServerError;
}

// Perform one request. `body` empty means no request body (GET).
// `outBody` receives the response body, capped at kMaxBodyBytes.
static CallResult request(const char* method, const String& path,
                          const String& body, bool authenticated,
                          String* outBody) {
  CallResult r;

  String base = Kosync::serverUrl();
  if (base.length() == 0) { r.result = Result::NotConfigured; return r; }
  bool tls = base.startsWith("https://");

  Connection conn;
  if (!conn.open(tls)) { r.result = Result::NoNetwork; return r; }

  HTTPClient http;
  if (!http.begin(conn.client(), base + path)) {
    r.result = Result::NoNetwork;
    return r;
  }
  http.setTimeout(kTimeoutMs);
  http.setConnectTimeout(kTimeoutMs);
  http.addHeader("accept", kAcceptHeader);
  if (body.length() > 0) http.addHeader("Content-Type", "application/json");
  if (authenticated) {
    http.addHeader("x-auth-user", Kosync::username());
    http.addHeader("x-auth-key",  Kosync::authKey());
  }

  // HTTPClient takes a non-const uint8_t* even though it only reads the
  // payload. Spell the two conversions out rather than hiding both behind one
  // C-style cast (cppcheck's cstyleCast, and it is clearer about what is
  // actually going on).
  uint8_t* payload = reinterpret_cast<uint8_t*>(const_cast<char*>(body.c_str()));
  int code = (body.length() > 0)
               ? http.sendRequest(method, payload, body.length())
               : http.sendRequest(method);

  if (outBody) {
    *outBody = "";
    if (code > 0) {
      // getString() rather than a raw stream read: the sync server may use
      // chunked transfer-encoding, and HTTPClient is what knows how to strip
      // the chunk framing. Skip the read outright when the declared length is
      // implausible for this protocol; a chunked reply reports -1, so also
      // truncate afterwards to bound what we hand the parser.
      int declared = http.getSize();
      if (declared <= (int)kMaxBodyBytes) {
        *outBody = http.getString();
        if (outBody->length() > kMaxBodyBytes) {
          outBody->remove(kMaxBodyBytes);
        }
      }
    }
  }

  http.end();

  r.httpCode = code;
  r.result   = classify(code);
  return r;
}

// ----------------------------------------------------------------------------
//  Endpoints
// ----------------------------------------------------------------------------
CallResult authenticate() {
  CallResult r;
  if (!Kosync::configured()) { r.result = Result::NotConfigured; return r; }
  return request("GET", "/users/auth", "", /*authenticated=*/true, nullptr);
}

CallResult registerUser(const String& username, const String& password) {
  CallResult r;
  if (username.length() == 0 || password.length() == 0) {
    r.result = Result::NotConfigured;
    return r;
  }
  // Registration is the one call that takes the plaintext password: the
  // server hashes it. Every later call sends the digest instead.
  String body = "{\"username\":\"" + kosyncJsonEscape(username)
              + "\",\"password\":\"" + kosyncJsonEscape(password) + "\"}";
  return request("POST", "/users/create", body, /*authenticated=*/false, nullptr);
}

CallResult push(const String& docHex, float percentage) {
  CallResult r;
  if (!Kosync::configured()) { r.result = Result::NotConfigured; return r; }
  if (docHex.length() != KOSYNC_DOC_HEX) { r.result = Result::NotFound; return r; }

  KosyncPush p;
  p.document   = docHex;
  p.device     = Kosync::deviceName();
  p.deviceId   = Kosync::deviceId();
  p.percentage = percentage;

  return request("PUT", "/syncs/progress", buildProgressBody(p),
                 /*authenticated=*/true, nullptr);
}

CallResult pull(const String& docHex, KosyncRemote& out) {
  out = KosyncRemote();

  CallResult r;
  if (!Kosync::configured()) { r.result = Result::NotConfigured; return r; }
  if (docHex.length() != KOSYNC_DOC_HEX) { r.result = Result::NotFound; return r; }

  String body;
  r = request("GET", "/syncs/progress/" + docHex, "", /*authenticated=*/true, &body);
  if (r.result != Result::Ok) return r;

  // A 200 with no usable percentage is the server saying it has never seen
  // this document — a normal first sync, not a failure.
  if (!parseProgressResponse(body, out)) r.result = Result::NotFound;
  return r;
}

}  // namespace Kosync
