#if defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)

#include <lofs/adapters/AdafruitFsVolume.h>

#include <lofs/FsBackend.h>
#include <cstring>
#include <memory>
#include <string>

namespace lofs {
namespace {

using AdafruitFile = Adafruit_LittleFS_Namespace::File;

class AdafruitFileImpl final : public detail::IoFileImpl {
public:
  explicit AdafruitFileImpl(AdafruitFile f) : f_(std::move(f)), open_(true) {}

  ~AdafruitFileImpl() override { close(); }

  size_t read(uint8_t* buf, size_t size) override {
    if (!open_ || !buf || size == 0) return 0;
    uint16_t n = size > 0xffff ? 0xffff : static_cast<uint16_t>(size);
    int r = f_.read(buf, n);
    return r > 0 ? static_cast<size_t>(r) : 0;
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
    AdafruitFile n = f_.openNextFile(Adafruit_LittleFS_Namespace::FILE_O_READ);
    if (!n) return nullptr;
    return std::unique_ptr<detail::IoFileImpl>(new AdafruitFileImpl(std::move(n)));
  }

  const char* name() const override {
    if (!open_) return "";
    return f_.name() ? f_.name() : "";
  }

  bool valid() const override { return open_ && static_cast<bool>(f_); }

  size_t size() const override { return open_ ? f_.size() : 0; }
  size_t position() const override { return open_ ? f_.position() : 0; }
  bool seek(size_t pos) override { return open_ && f_.seek(pos); }

private:
  AdafruitFile f_;
  bool open_;
};

}  // namespace

IoFile AdafruitFsVolume::open(const char* path, uint8_t mode) {
  if (!fs_ || !path) return {};
  uint8_t m = (mode == FILE_O_READ) ? Adafruit_LittleFS_Namespace::FILE_O_READ : Adafruit_LittleFS_Namespace::FILE_O_WRITE;
  AdafruitFile f = fs_->open(path, m);
  if (!f) return {};
  return IoFile(std::unique_ptr<detail::IoFileImpl>(new AdafruitFileImpl(std::move(f))));
}

IoFile AdafruitFsVolume::open(const char* path, const char* mode) {
  if (!fs_ || !path || !mode) return {};
  uint8_t m = (mode[0] == 'r' && mode[1] == '\0') ? Adafruit_LittleFS_Namespace::FILE_O_READ
                                                 : Adafruit_LittleFS_Namespace::FILE_O_WRITE;
  AdafruitFile f = fs_->open(path, m);
  if (!f) return {};
  return IoFile(std::unique_ptr<detail::IoFileImpl>(new AdafruitFileImpl(std::move(f))));
}

bool AdafruitFsVolume::exists(const char* path) { return fs_ && path && fs_->exists(path); }

bool AdafruitFsVolume::mkdir(const char* path) { return fs_ && path && fs_->mkdir(path); }

bool AdafruitFsVolume::remove(const char* path) { return fs_ && path && fs_->remove(path); }

bool AdafruitFsVolume::rename(const char* from, const char* to) {
  return fs_ && from && to && fs_->rename(from, to);
}

bool AdafruitFsVolume::rmdir(const char* path) { return fs_ && path && fs_->rmdir(path); }

}  // namespace lofs

#endif
