#pragma once

#include <lofs/FsBackend.h>

namespace lofs {

class InternalFlashBackend : public FsBackend {
public:
  static InternalFlashBackend& instance();

  /** Runtime override (see `LoFS::bindInternalFs` / `bindVolume`). Passing `nullptr` re-binds the platform default. */
  void bindVolume(FsVolume* vol);

  bool available() const override;
  IoFile open(const char* path, uint8_t mode) override;
  IoFile open(const char* path, const char* mode) override;
  bool exists(const char* path) override;
  bool mkdir(const char* path) override;
  bool remove(const char* path) override;
  bool rename(const char* from, const char* to) override;
  bool rmdir(const char* path, bool recursive) override;
  uint64_t totalBytes() override;
  uint64_t usedBytes() override;

private:
  InternalFlashBackend();
  void bindPlatformVolume();
  FsVolume* vol_ = nullptr;
};

}  // namespace lofs
