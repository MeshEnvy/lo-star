#pragma once

#include <lofs/FsVolume.h>
#include <lofs/IoFile.h>
#include <cstddef>
#include <cstdint>

namespace lofs {

/** Per-mount filesystem adapter (internal flash, external flash, SD, …). */
class FsBackend {
public:
  virtual ~FsBackend() = default;
  virtual bool available() const = 0;
  virtual IoFile open(const char* path, uint8_t mode) = 0;
  virtual IoFile open(const char* path, const char* mode) = 0;
  virtual bool exists(const char* path) = 0;
  virtual bool mkdir(const char* path) = 0;
  virtual bool remove(const char* path) = 0;
  /** Native rename on this backend only (same volume). */
  virtual bool rename(const char* from, const char* to) = 0;
  virtual bool rmdir(const char* path, bool recursive) = 0;
  virtual uint64_t totalBytes() = 0;
  virtual uint64_t usedBytes() = 0;
};

}  // namespace lofs

#ifndef FILE_O_READ
#define FILE_O_READ ((uint8_t)0)
#endif
#ifndef FILE_O_WRITE
#define FILE_O_WRITE ((uint8_t)1)
#endif
