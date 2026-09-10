#ifndef PALA_HOSTFS_CONFIG_H
#define PALA_HOSTFS_CONFIG_H
// Stub for the host build of storage/sync_map.cpp. Shadows the firmware's
// src/config.h, which pulls in display geometry and the language catalog that
// a filesystem test has no use for. Put test/hostfs on the include path
// BEFORE Pala_One_2_1 to select it.
#endif
