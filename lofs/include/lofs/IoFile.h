#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>

namespace lofs {
namespace detail {

struct IoFileImpl {
  virtual ~IoFileImpl() = default;
  virtual size_t read(uint8_t* buf, size_t size) = 0;
  virtual size_t write(const uint8_t* buf, size_t size) = 0;
  virtual void flush() = 0;
  virtual void close() = 0;
  virtual bool isDirectory() const = 0;
  /** Next entry when this handle is a directory; nullptr when done. */
  virtual std::unique_ptr<IoFileImpl> openNextFileImpl() = 0;
  /** Stable until next call on this IoFileImpl or close. */
  virtual const char* name() const = 0;
  virtual bool valid() const = 0;
  virtual size_t size() const = 0;
  virtual size_t position() const = 0;
  virtual bool seek(size_t pos) = 0;
};

}  // namespace detail

/** Platform-neutral open file or directory handle (movable, RAII). */
class IoFile {
public:
  IoFile() = default;
  explicit IoFile(std::unique_ptr<detail::IoFileImpl> impl) : impl_(std::move(impl)) {}

  IoFile(const IoFile&) = delete;
  IoFile& operator=(const IoFile&) = delete;
  IoFile(IoFile&&) noexcept = default;
  IoFile& operator=(IoFile&&) noexcept = default;

  explicit operator bool() const { return impl_ && impl_->valid(); }

  size_t read(uint8_t* buf, size_t size) { return impl_ ? impl_->read(buf, size) : 0; }
  size_t write(const uint8_t* buf, size_t size) { return impl_ ? impl_->write(buf, size) : 0; }
  void flush() {
    if (impl_) impl_->flush();
  }
  void close() {
    if (impl_) {
      impl_->close();
      impl_.reset();
    }
  }
  bool isDirectory() const { return impl_ && impl_->isDirectory(); }

  IoFile openNextFile() {
    if (!impl_) return {};
    return IoFile(impl_->openNextFileImpl());
  }

  const char* name() const { return impl_ ? impl_->name() : ""; }

  size_t size() const { return impl_ ? impl_->size() : 0; }
  size_t position() const { return impl_ ? impl_->position() : 0; }
  bool seek(size_t pos) { return impl_ && impl_->seek(pos); }

  /** Used by RamBackend directory iteration to open child entries without extra hops. */
  std::unique_ptr<detail::IoFileImpl> releaseImpl() { return std::move(impl_); }

private:
  std::unique_ptr<detail::IoFileImpl> impl_;
};

inline IoFile invalid_iofile() { return {}; }

}  // namespace lofs
