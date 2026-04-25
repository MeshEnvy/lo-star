#pragma once

#include <lofs/FsVolume.h>

#if defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)

#include <Adafruit_LittleFS.h>

namespace lofs {

/** Wraps Adafruit `Adafruit_LittleFS` (nRF/STM32 internal flash, …). */
class AdafruitFsVolume final : public FsVolume {
public:
  explicit AdafruitFsVolume(Adafruit_LittleFS* fs = nullptr) : fs_(fs) {}

  void bindFs(Adafruit_LittleFS* fs) { fs_ = fs; }

  IoFile open(const char* path, uint8_t mode) override;
  IoFile open(const char* path, const char* mode) override;
  bool exists(const char* path) override;
  bool mkdir(const char* path) override;
  bool remove(const char* path) override;
  bool rename(const char* from, const char* to) override;
  bool rmdir(const char* path) override;

private:
  Adafruit_LittleFS* fs_;
};

}  // namespace lofs

#endif
