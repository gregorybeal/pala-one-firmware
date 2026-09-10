#ifndef PALA_HOSTFS_STATE_H
#define PALA_HOSTFS_STATE_H

// Host stand-in for the firmware's src/state.h, providing just the pieces
// storage/sync_map.cpp touches: `String`, a `File` and the `FS` object. Real
// files under a settable root, so the module under test does genuine seeks
// and short reads rather than talking to a mock.

#include <cstdio>
#include <cstring>
#include <string>
#include "pure/arduino_compat.h"

void        hostFsSetRoot(const std::string& root);
std::string hostFsResolve(const String& path);

class File {
public:
  File() = default;
  explicit File(std::FILE* f) : f_(f) {}

  explicit operator bool() const { return f_ != nullptr; }

  size_t read(uint8_t* buf, size_t n) {
    if (!f_) return 0;
    return std::fread(buf, 1, n, f_);
  }
  size_t write(const uint8_t* buf, size_t n) {
    if (!f_) return 0;
    return std::fwrite(buf, 1, n, f_);
  }
  bool seek(size_t pos) {
    return f_ && std::fseek(f_, (long)pos, SEEK_SET) == 0;
  }
  size_t size() const {
    if (!f_) return 0;
    long cur = std::ftell(f_);
    std::fseek(f_, 0, SEEK_END);
    long end = std::ftell(f_);
    std::fseek(f_, cur, SEEK_SET);
    return (size_t)end;
  }
  void close() {
    if (f_) { std::fclose(f_); f_ = nullptr; }
  }

private:
  std::FILE* f_ = nullptr;
};

struct HostFs {
  File open(const String& path, const char* mode) {
    std::FILE* f = std::fopen(hostFsResolve(path).c_str(), mode);
    return File(f);
  }
  bool exists(const String& path) {
    std::FILE* f = std::fopen(hostFsResolve(path).c_str(), "rb");
    if (!f) return false;
    std::fclose(f);
    return true;
  }
  bool remove(const String& path) {
    return std::remove(hostFsResolve(path).c_str()) == 0;
  }
  bool rename(const String& from, const String& to) {
    return std::rename(hostFsResolve(from).c_str(), hostFsResolve(to).c_str()) == 0;
  }
};

extern HostFs hostFs;
#define FS hostFs

#endif  // PALA_HOSTFS_STATE_H
