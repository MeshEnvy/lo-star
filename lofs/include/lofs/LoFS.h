#pragma once

#include <lofs/FsBackend.h>
#include <cstddef>
#include <cstdint>
#include <initializer_list>

#define LOFS_VERSION "0.3.0-meshcore"

class LoFS {
public:
  struct FsVolumeBinding {
    const char* prefix;
    lofs::FsVolume* vol;
  };

  static bool mount(const char* prefix, lofs::FsBackend* backend);
  /** Mount host filesystem by virtual prefix (currently supports `/__int__`). */
  static bool mount(const char* prefix, lofs::FsVolume* vol);
  static bool mount(std::initializer_list<FsVolumeBinding> bindings);
  static bool unmount(const char* prefix);
  static lofs::FsBackend* resolveBackend(const char* virtual_path, char* stripped_out, size_t stripped_cap);

  /** Override the platform-default internal-flash volume (SPIFFS, LittleFS, …). Call before
   *  `lofs::mount_platform_volumes()` / any `LoFS::open`. `nullptr` restores the platform default. */
  static void bindInternalFs(lofs::FsVolume* vol);

  static lofs::IoFile open(const char* filepath, uint8_t mode);
  static lofs::IoFile open(const char* filepath, const char* mode);
  static bool exists(const char* filepath);
  static bool mkdir(const char* filepath);
  static bool remove(const char* filepath);
  static bool rename(const char* oldfilepath, const char* newfilepath);
  static bool writeFileAtomic(const char* filepath, const uint8_t* data, size_t size);
  static bool readFileAtomic(const char* filepath, uint8_t* out, size_t cap, size_t* out_size);
  static bool rmdir(const char* filepath, bool recursive = false);
  static uint64_t totalBytes(const char* filepath);
  static uint64_t usedBytes(const char* filepath);
  static uint64_t freeBytes(const char* filepath);

  static bool isSDCardAvailable();
};
