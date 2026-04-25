#pragma once

#include <lofs/FsBackend.h>
#include <memory>

namespace lofs {

struct RamFsInner;

/**
 * In-RAM filesystem backend for LoFS.
 *
 * Stores file data in heap-backed structures keyed by normalized absolute path.
 * Reboots wipe everything. Enforces a total-bytes cap (`LOFS_RAM_CAP_BYTES`,
 * default 65536) so stuck callers cannot exhaust heap.
 */
class RamBackend : public FsBackend {
public:
  static RamBackend& instance();
  ~RamBackend() override;

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
  RamBackend();
  std::shared_ptr<RamFsInner> impl_;
};

}  // namespace lofs
