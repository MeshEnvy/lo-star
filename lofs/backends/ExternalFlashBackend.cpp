#include "ExternalFlashBackend.h"
#include "InternalFlashBackend.h"

#include <cstdio>

extern "C" __attribute__((weak)) lofs::FsVolume* lofs_variant_external_volume(void) { return nullptr; }

namespace lofs {

ExternalFlashBackend::ExternalFlashBackend() { bindPlatformVolume(); }

void ExternalFlashBackend::bindPlatformVolume() {
#if defined(ARCH_ESP32) || defined(RP2040_PLATFORM)
  delegate_internal_ = true;
  vol_ = nullptr;
#else
  delegate_internal_ = false;
  vol_ = lofs_variant_external_volume();
#endif
}

ExternalFlashBackend& ExternalFlashBackend::instance() {
  static ExternalFlashBackend inst;
  return inst;
}

bool ExternalFlashBackend::available() const {
  if (delegate_internal_) return InternalFlashBackend::instance().available();
  return vol_ != nullptr;
}

IoFile ExternalFlashBackend::open(const char* path, uint8_t mode) {
  if (delegate_internal_) return InternalFlashBackend::instance().open(path, mode);
  if (!vol_) return {};
#if defined(ESP32) || defined(ARCH_ESP32) || defined(RP2040_PLATFORM)
  return vol_->open(path, mode);
#else
  return vol_->open(path, mode);
#endif
}

IoFile ExternalFlashBackend::open(const char* path, const char* mode) {
  if (delegate_internal_) return InternalFlashBackend::instance().open(path, mode);
  if (!vol_) return {};
  return vol_->open(path, mode);
}

bool ExternalFlashBackend::exists(const char* path) {
  if (delegate_internal_) return InternalFlashBackend::instance().exists(path);
  return vol_ && path && vol_->exists(path);
}

bool ExternalFlashBackend::mkdir(const char* path) {
  if (delegate_internal_) return InternalFlashBackend::instance().mkdir(path);
  return vol_ && path && vol_->mkdir(path);
}

bool ExternalFlashBackend::remove(const char* path) {
  if (delegate_internal_) return InternalFlashBackend::instance().remove(path);
  return vol_ && path && vol_->remove(path);
}

bool ExternalFlashBackend::rename(const char* from, const char* to) {
  if (delegate_internal_) return InternalFlashBackend::instance().rename(from, to);
  return vol_ && from && to && vol_->rename(from, to);
}

static bool rmdir_ext(FsVolume* vol, ExternalFlashBackend& self, const char* path, bool recursive) {
  if (!vol || !path) return false;
  if (!recursive) return vol->rmdir(path);
  IoFile dir = self.open(path, FILE_O_READ);
  if (!dir || !dir.isDirectory()) {
    if (dir) dir.close();
    return false;
  }
  for (;;) {
    IoFile f = dir.openNextFile();
    if (!f) break;
    char child[256];
    snprintf(child, sizeof(child), "%s/%s", path, f.name());
    if (f.isDirectory()) {
      f.close();
      rmdir_ext(vol, self, child, true);
    } else {
      f.close();
      vol->remove(child);
    }
  }
  dir.close();
  return vol->rmdir(path);
}

bool ExternalFlashBackend::rmdir(const char* path, bool recursive) {
  if (delegate_internal_) return InternalFlashBackend::instance().rmdir(path, recursive);
  if (!vol_) return false;
  return rmdir_ext(vol_, *this, path, recursive);
}

uint64_t ExternalFlashBackend::totalBytes() {
  if (delegate_internal_) return InternalFlashBackend::instance().totalBytes();
  return 0;
}

uint64_t ExternalFlashBackend::usedBytes() {
  if (delegate_internal_) return InternalFlashBackend::instance().usedBytes();
  return 0;
}

}  // namespace lofs
