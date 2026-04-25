#pragma once

#include <lofs/IoFile.h>
#include <cstdint>

namespace lofs {

/**
 * Host-mounted volume (SPIFFS, LittleFS, SD/FAT, …) behind a single API.
 * Implementations live in adapter .cpp files; portable code only uses FsVolume&/pointer.
 */
class FsVolume {
public:
  virtual ~FsVolume() = default;

  virtual IoFile open(const char* path, uint8_t mode) = 0;
  virtual IoFile open(const char* path, const char* mode) = 0;
  virtual bool exists(const char* path) = 0;
  virtual bool mkdir(const char* path) = 0;
  virtual bool remove(const char* path) = 0;
  virtual bool rename(const char* from, const char* to) = 0;
  virtual bool rmdir(const char* path) = 0;
  virtual uint64_t totalBytes() { return 0; }
  virtual uint64_t usedBytes() { return 0; }
};

}  // namespace lofs
