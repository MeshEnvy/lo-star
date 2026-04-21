#include "InternalFlashBackend.h"

#if defined(ESP32_PLATFORM) || defined(RP2040_PLATFORM)
#include <LittleFS.h>
#elif defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)
#include <InternalFileSystem.h>
#endif

/** Weak hook mirroring `lofs_variant_external_fs`: apps can supply a specific internal-flash fs
 *  (SPIFFS, LittleFS, ...) at link time instead of the platform default. `nullptr` = fall back. */
extern "C" __attribute__((weak)) lofs::FSys* lofs_variant_internal_fs(void) { return nullptr; }

namespace lofs {

InternalFlashBackend::InternalFlashBackend() { bindPlatformFs(); }

void InternalFlashBackend::bindFs(FSys* fs) {
  if (fs) {
    _fs = fs;
  } else {
    bindPlatformFs();
  }
}

void InternalFlashBackend::bindPlatformFs() {
  if (FSys* v = lofs_variant_internal_fs()) {
    _fs = v;
    return;
  }
#if defined(ESP32_PLATFORM) || defined(RP2040_PLATFORM)
  _fs = &LittleFS;
#elif defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)
  _fs = &InternalFS;
#else
  _fs = nullptr;
#endif
}

InternalFlashBackend& InternalFlashBackend::instance() {
  static InternalFlashBackend inst;
  return inst;
}

bool InternalFlashBackend::available() const { return _fs != nullptr; }

File InternalFlashBackend::open(const char* path, uint8_t mode) {
  if (!_fs || !path) return lofs::invalid_file();
#if defined(ESP32_PLATFORM) || defined(RP2040_PLATFORM)
  if (mode == FILE_O_READ) return _fs->open(path, "r");
  return _fs->open(path, "w", true);
#else
  return _fs->open(path, mode == FILE_O_READ ? FILE_O_READ : FILE_O_WRITE);
#endif
}

File InternalFlashBackend::open(const char* path, const char* mode) {
  if (!_fs || !path || !mode) return lofs::invalid_file();
#if defined(ESP32_PLATFORM) || defined(RP2040_PLATFORM)
  return _fs->open(path, mode, mode[0] == 'w');
#else
  return _fs->open(path, (mode[0] == 'r' && mode[1] == '\0') ? FILE_O_READ : FILE_O_WRITE);
#endif
}

bool InternalFlashBackend::exists(const char* path) {
  if (!_fs || !path) return false;
  return _fs->exists(path);
}

bool InternalFlashBackend::mkdir(const char* path) {
  if (!_fs || !path) return false;
  return _fs->mkdir(path);
}

bool InternalFlashBackend::remove(const char* path) {
  if (!_fs || !path) return false;
  return _fs->remove(path);
}

bool InternalFlashBackend::rename(const char* from, const char* to) {
  if (!_fs || !from || !to) return false;
  return _fs->rename(from, to);
}

static bool rmdir_one(lofs::InternalFlashBackend& self, lofs::FSys* fs, const char* path, bool recursive) {
  if (!fs || !path) return false;
  if (!recursive) return fs->rmdir(path);
  File dir = self.open(path, (uint8_t)FILE_O_READ);
  if (!dir) return false;
  if (!dir.isDirectory()) {
    dir.close();
    return false;
  }
  for (;;) {
    File f = dir.openNextFile();
    if (!f) break;
    char child[256];
    snprintf(child, sizeof(child), "%s/%s", path, f.name());
    if (f.isDirectory()) {
      f.close();
      rmdir_one(self, fs, child, true);
    } else {
      f.close();
      fs->remove(child);
    }
  }
  dir.close();
  return fs->rmdir(path);
}

bool InternalFlashBackend::rmdir(const char* path, bool recursive) {
  if (!_fs || !path) return false;
  return rmdir_one(*this, _fs, path, recursive);
}

uint64_t InternalFlashBackend::totalBytes() {
  if (!_fs) return 0;
#if defined(ESP32_PLATFORM) || defined(RP2040_PLATFORM)
  // Best-effort: `fs::FS` base class doesn't expose totalBytes, so only report when the bound
  // driver is the platform default. Apps using SPIFFS/other drivers should query them directly.
  if (_fs == &LittleFS) return LittleFS.totalBytes();
  return 0;
#else
  return 0;
#endif
}

uint64_t InternalFlashBackend::usedBytes() {
  if (!_fs) return 0;
#if defined(ESP32_PLATFORM) || defined(RP2040_PLATFORM)
  if (_fs == &LittleFS) return LittleFS.usedBytes();
  return 0;
#else
  return 0;
#endif
}

}  // namespace lofs
