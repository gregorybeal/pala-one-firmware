#include "src/state.h"

HostFs hostFs;

static std::string g_root = ".";

void hostFsSetRoot(const std::string& root) { g_root = root; }

std::string hostFsResolve(const String& path) {
  std::string p(path.c_str());
  // Firmware paths are absolute LittleFS paths; re-root them under the test
  // directory rather than writing to the host's filesystem root.
  if (!p.empty() && p[0] == '/') return g_root + p;
  return g_root + "/" + p;
}
