#include "SdBackend.h"

#include <lofs/capabilities.h>

#include <cstdio>
#include <memory>
#include <string>

#if LOFS_CAP_SD
#include <SD.h>
#include <SPI.h>
#endif

namespace lofs {

#if LOFS_CAP_SD

namespace {

class SdFileImpl final : public detail::IoFileImpl {
public:
  explicit SdFileImpl(::File f) : f_(std::move(f)), open_(true) {}

  ~SdFileImpl() override { close(); }

  size_t read(uint8_t* buf, size_t size) override {
    if (!open_ || !buf || size == 0) return 0;
    return f_.read(buf, size);
  }

  size_t write(const uint8_t* buf, size_t size) override {
    if (!open_ || !buf || size == 0) return 0;
    return f_.write(buf, size);
  }

  void flush() override {
    if (open_) f_.flush();
  }

  void close() override {
    if (open_) {
      f_.close();
      open_ = false;
    }
  }

  bool isDirectory() const override { return open_ && f_.isDirectory(); }

  std::unique_ptr<detail::IoFileImpl> openNextFileImpl() override {
    if (!open_ || !f_.isDirectory()) return nullptr;
    ::File n = f_.openNextFile();
    if (!n) return nullptr;
    return std::unique_ptr<detail::IoFileImpl>(new SdFileImpl(std::move(n)));
  }

  const char* name() const override {
    if (!open_) return "";
    name_cache_ = f_.name();
    return name_cache_.c_str();
  }

  bool valid() const override { return open_ && static_cast<bool>(f_); }

  size_t size() const override { return open_ ? f_.size() : 0; }
  size_t position() const override { return open_ ? f_.position() : 0; }
  bool seek(size_t pos) override { return open_ && f_.seek(pos); }

private:
  mutable std::string name_cache_;
  /** ESP32 `fs::File::isDirectory()` is not `const`; `IoFileImpl::isDirectory()` is. */
  mutable ::File f_;
  bool open_;
};

bool ensure_sd() {
  static bool inited = false;
  if (inited) return SD.cardType() != CARD_NONE;
  inited = true;
  SD.begin();
  return SD.cardType() != CARD_NONE;
}

}  // namespace

SdBackend::SdBackend() = default;

SdBackend& SdBackend::instance() {
  static SdBackend inst;
  return inst;
}

bool SdBackend::available() const { return ensure_sd(); }

IoFile SdBackend::open(const char* path, uint8_t mode) {
  if (!ensure_sd() || !path) return {};
  ::File f = SD.open(path, mode == FILE_O_READ ? FILE_READ : FILE_WRITE);
  if (!f) return {};
  return IoFile(std::unique_ptr<detail::IoFileImpl>(new SdFileImpl(std::move(f))));
}

IoFile SdBackend::open(const char* path, const char* mode) {
  if (!ensure_sd() || !path || !mode) return {};
  ::File f = SD.open(path, mode);
  if (!f) return {};
  return IoFile(std::unique_ptr<detail::IoFileImpl>(new SdFileImpl(std::move(f))));
}

bool SdBackend::exists(const char* path) { return ensure_sd() && path && SD.exists(path); }

bool SdBackend::mkdir(const char* path) { return ensure_sd() && path && SD.mkdir(path); }

bool SdBackend::remove(const char* path) { return ensure_sd() && path && SD.remove(path); }

bool SdBackend::rename(const char* from, const char* to) {
  return ensure_sd() && from && to && SD.rename(from, to);
}

static bool rmdir_sd(const char* path, bool recursive) {
  if (!path) return false;
  if (!recursive) return SD.rmdir(path);
  ::File dir = SD.open(path);
  if (!dir || !dir.isDirectory()) {
    if (dir) dir.close();
    return false;
  }
  for (;;) {
    ::File f = dir.openNextFile();
    if (!f) break;
    char child[256];
    snprintf(child, sizeof(child), "%s/%s", path, f.name());
    if (f.isDirectory()) {
      f.close();
      rmdir_sd(child, true);
    } else {
      f.close();
      SD.remove(child);
    }
  }
  dir.close();
  return SD.rmdir(path);
}

bool SdBackend::rmdir(const char* path, bool recursive) {
  return ensure_sd() && path && rmdir_sd(path, recursive);
}

uint64_t SdBackend::totalBytes() {
  if (!ensure_sd()) return 0;
  return (uint64_t)SD.cardSize();
}

uint64_t SdBackend::usedBytes() { return 0; }

#else

SdBackend::SdBackend() = default;

SdBackend& SdBackend::instance() {
  static SdBackend inst;
  return inst;
}

bool SdBackend::available() const { return false; }
IoFile SdBackend::open(const char*, uint8_t) { return {}; }
IoFile SdBackend::open(const char*, const char*) { return {}; }
bool SdBackend::exists(const char*) { return false; }
bool SdBackend::mkdir(const char*) { return false; }
bool SdBackend::remove(const char*) { return false; }
bool SdBackend::rename(const char*, const char*) { return false; }
bool SdBackend::rmdir(const char*, bool) { return false; }
uint64_t SdBackend::totalBytes() { return 0; }
uint64_t SdBackend::usedBytes() { return 0; }

#endif

}  // namespace lofs
