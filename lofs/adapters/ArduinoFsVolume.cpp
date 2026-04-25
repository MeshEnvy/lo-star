#if defined(ESP32) || defined(ARCH_ESP32) || defined(RP2040_PLATFORM)

#include <lofs/adapters/ArduinoFsVolume.h>

#include <lofs/FsBackend.h>
#include <LittleFS.h>
#include <cstring>
#include <memory>
#include <string>

namespace lofs {
namespace {

class ArduinoFileImpl final : public detail::IoFileImpl {
public:
  explicit ArduinoFileImpl(fs::File f) : f_(std::move(f)), open_(true), is_dir_(f_.isDirectory()) {}

  ~ArduinoFileImpl() override { close(); }

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

  bool isDirectory() const override { return open_ && is_dir_; }

  std::unique_ptr<detail::IoFileImpl> openNextFileImpl() override {
    if (!open_ || !f_.isDirectory()) return nullptr;
    fs::File n = f_.openNextFile();
    if (!n) return nullptr;
    return std::unique_ptr<detail::IoFileImpl>(new ArduinoFileImpl(std::move(n)));
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
  fs::File f_;
  bool open_;
  bool is_dir_;
};

}  // namespace

IoFile ArduinoFsVolume::open(const char* path, uint8_t mode) {
  if (!fs_ || !path) return {};
  if (mode == FILE_O_READ) {
    fs::File f = fs_->open(path, "r");
    if (!f) return {};
    return IoFile(std::unique_ptr<detail::IoFileImpl>(new ArduinoFileImpl(std::move(f))));
  }
  fs::File f = fs_->open(path, "w", true);
  if (!f) return {};
  return IoFile(std::unique_ptr<detail::IoFileImpl>(new ArduinoFileImpl(std::move(f))));
}

IoFile ArduinoFsVolume::open(const char* path, const char* mode) {
  if (!fs_ || !path || !mode) return {};
  const bool create = (mode[0] == 'w' || mode[0] == 'a');
  fs::File f = fs_->open(path, mode, create);
  if (!f) return {};
  return IoFile(std::unique_ptr<detail::IoFileImpl>(new ArduinoFileImpl(std::move(f))));
}

bool ArduinoFsVolume::exists(const char* path) { return fs_ && path && fs_->exists(path); }

bool ArduinoFsVolume::mkdir(const char* path) { return fs_ && path && fs_->mkdir(path); }

bool ArduinoFsVolume::remove(const char* path) { return fs_ && path && fs_->remove(path); }

bool ArduinoFsVolume::rename(const char* from, const char* to) {
  return fs_ && from && to && fs_->rename(from, to);
}

bool ArduinoFsVolume::rmdir(const char* path) { return fs_ && path && fs_->rmdir(path); }

uint64_t ArduinoFsVolume::totalBytes() {
  if (!fs_) return 0;
  if (fs_ == &LittleFS) return LittleFS.totalBytes();
  return 0;
}

uint64_t ArduinoFsVolume::usedBytes() {
  if (!fs_) return 0;
  if (fs_ == &LittleFS) return LittleFS.usedBytes();
  return 0;
}

}  // namespace lofs

#endif
