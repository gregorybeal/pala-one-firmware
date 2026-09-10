#ifndef PALA_HAL_KOSYNC_H
#define PALA_HAL_KOSYNC_H

#include <Arduino.h>

#include "src/pure/kosync_codec.h"   // KosyncRemote

// ============================================================================
//  KOReader `kosync` HTTP client.
//
//  Protocol (verified against KOReader master —
//  plugins/kosync.koplugin/{api.json,KOSyncClient.lua}):
//
//    POST /users/create           {username, password}
//    GET  /users/auth
//    PUT  /syncs/progress         {document, progress, percentage,
//                                  device, device_id}
//    GET  /syncs/progress/:document
//
//  Every authenticated call carries:
//    x-auth-user: <username>
//    x-auth-key:  <md5 hex of password>
//    accept:      application/vnd.koreader.v1+json
//
//  All calls BLOCK and require an established Wi-Fi STA connection. They are
//  driven from ui/screens/sync_screen.cpp, which owns the radio lifecycle.
// ============================================================================
namespace Kosync {

// Why a call ended. Kept separate from the HTTP status so the screen can say
// something useful without mapping status codes itself.
enum class Result {
  Ok,
  NotConfigured,   // no username / key / server URL stored
  NoNetwork,       // socket or TLS failure — never reached the server
  Unauthorized,    // 401 — wrong username or password
  NotFound,        // 404 / empty — server has no progress for this document
  Conflict,        // 402 on register — username already taken
  ServerError,     // anything else the server returned
};

struct CallResult {
  Result result   = Result::NoNetwork;
  int    httpCode = 0;      // raw status, for the error line on screen
};

// MD5 hex of `password`, i.e. the `x-auth-key` value. Uses mbedTLS from the
// IDF — no new dependency. Exposed because the web settings form hashes on
// submit so the plaintext is never persisted.
String md5Hex(const String& password);

// GET /users/auth — credential check for the web UI's "Test connection".
CallResult authenticate();

// POST /users/create. `password` is the plaintext; the server hashes it
// itself on this endpoint (unlike every other call, which takes the digest).
CallResult registerUser(const String& username, const String& password);

// PUT /syncs/progress for `docHex` at `percentage` in [0, 1]. `progress` is
// the crengine XPointer for that position when the book's spine map can
// produce one; pass an empty string to send the percentage instead.
CallResult push(const String& docHex, float percentage,
                const String& progress = String());

// GET /syncs/progress/:docHex. On Result::Ok `out` holds a valid remote
// position; Result::NotFound means the server simply has nothing for this
// document yet, which is not an error worth showing as one.
CallResult pull(const String& docHex, KosyncRemote& out);

}  // namespace Kosync

#endif  // PALA_HAL_KOSYNC_H
