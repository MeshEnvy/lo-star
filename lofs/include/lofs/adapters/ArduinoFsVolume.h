#pragma once

#include <lofs/FsVolume.h>

#if defined(ESP32) || defined(ARCH_ESP32) || defined(RP2040_PLATFORM)

#include <FS.h>

namespace lofs {

/** Wraps Arduino `fs::FS` (LittleFS, SPIFFS, FAT partition, …). */
class ArduinoFsVolume final : public FsVolume {
public:
  explicit ArduinoFsVolume(fs::FS* fs = nullptr) : fs_(fs) {}

  void bindFs(fs::FS* fs) { fs_ = fs; }

  IoFile open(const char* path, uint8_t mode) override;
  IoFile open(const char* path, const char* mode) override;
  bool exists(const char* path) override;
  bool mkdir(const char* path) override;
  bool remove(const char* path) override;
  bool rename(const char* from, const char* to) override;
  bool rmdir(const char* path) override;
  uint64_t totalBytes() override;
  uint64_t usedBytes() override;

private:
  fs::FS* fs_;
};

}  // namespace lofs

#endif
