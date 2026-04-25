#pragma once

#include <lofs/FsBackend.h>

namespace lofs {

class ExternalFlashBackend : public FsBackend {
public:
  static ExternalFlashBackend& instance();

  /** True when `/__ext__` is backed by a volume other than the internal-flash delegate (nRF QSPI, …). */
  bool hasDedicatedVolume() const { return vol_ != nullptr && !delegate_internal_; }

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
  ExternalFlashBackend();
  void bindPlatformVolume();
  bool delegate_internal_ = false;
  FsVolume* vol_ = nullptr;
};

}  // namespace lofs

extern "C" lofs::FsVolume* lofs_variant_external_volume(void) __attribute__((weak));
