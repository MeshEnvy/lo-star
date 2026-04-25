#include "InternalFlashBackend.h"

#include <cstdio>
#include <cstring>

#if defined(ESP32) || defined(ARCH_ESP32) || defined(RP2040_PLATFORM)
#include <LittleFS.h>
#include <lofs/adapters/ArduinoFsVolume.h>
#elif defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)
#include <lofs/adapters/AdafruitFsVolume.h>
#include <InternalFileSystem.h>
#endif

extern "C" __attribute__((weak)) lofs::FsVolume* lofs_variant_internal_volume(void) { return nullptr; }

namespace lofs {

InternalFlashBackend::InternalFlashBackend() { bindPlatformVolume(); }

void InternalFlashBackend::bindVolume(FsVolume* vol) {
  if (vol) {
    vol_ = vol;
  } else {
    bindPlatformVolume();
  }
}

void InternalFlashBackend::bindPlatformVolume() {
  if (FsVolume* v = lofs_variant_internal_volume()) {
    vol_ = v;
    return;
  }
#if defined(ESP32) || defined(ARCH_ESP32) || defined(RP2040_PLATFORM)
  static ArduinoFsVolume s_platform(&LittleFS);
  vol_ = &s_platform;
#elif defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)
  static AdafruitFsVolume s_platform(&InternalFS);
  vol_ = &s_platform;
#else
  vol_ = nullptr;
#endif
}

InternalFlashBackend& InternalFlashBackend::instance() {
  static InternalFlashBackend inst;
  return inst;
}

bool InternalFlashBackend::available() const { return vol_ != nullptr; }

IoFile InternalFlashBackend::open(const char* path, uint8_t mode) {
  if (!vol_ || !path) return {};
  return vol_->open(path, mode);
}

IoFile InternalFlashBackend::open(const char* path, const char* mode) {
  if (!vol_ || !path || !mode) return {};
  return vol_->open(path, mode);
}

bool InternalFlashBackend::exists(const char* path) { return vol_ && path && vol_->exists(path); }

bool InternalFlashBackend::mkdir(const char* path) { return vol_ && path && vol_->mkdir(path); }

bool InternalFlashBackend::remove(const char* path) { return vol_ && path && vol_->remove(path); }

bool InternalFlashBackend::rename(const char* from, const char* to) {
  return vol_ && from && to && vol_->rename(from, to);
}

static bool rmdir_one(InternalFlashBackend& self, FsVolume* vol, const char* path, bool recursive) {
  if (!vol || !path) return false;
  if (!recursive) return vol->rmdir(path);
  IoFile dir = self.open(path, FILE_O_READ);
  if (!dir) return false;
  if (!dir.isDirectory()) {
    dir.close();
    return false;
  }
  for (;;) {
    IoFile f = dir.openNextFile();
    if (!f) break;
    char child[256];
    snprintf(child, sizeof(child), "%s/%s", path, f.name());
    if (f.isDirectory()) {
      f.close();
      rmdir_one(self, vol, child, true);
    } else {
      f.close();
      vol->remove(child);
    }
  }
  dir.close();
  return vol->rmdir(path);
}

bool InternalFlashBackend::rmdir(const char* path, bool recursive) {
  if (!vol_ || !path) return false;
  return rmdir_one(*this, vol_, path, recursive);
}

uint64_t InternalFlashBackend::totalBytes() { return vol_ ? vol_->totalBytes() : 0; }

uint64_t InternalFlashBackend::usedBytes() { return vol_ ? vol_->usedBytes() : 0; }

}  // namespace lofs
