#include "RamBackend.h"

#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#ifndef LOFS_RAM_CAP_BYTES
#define LOFS_RAM_CAP_BYTES (64u * 1024u)
#endif

namespace lofs {

struct RamNode {
  bool is_dir = false;
  std::shared_ptr<std::vector<uint8_t>> data;
};

static std::string normalize_path(const char* p) {
  if (!p || !*p) return "/";
  std::string s;
  if (p[0] != '/') {
    s.reserve(strlen(p) + 1);
    s.push_back('/');
    s.append(p);
  } else {
    s = p;
  }
  std::string out;
  out.reserve(s.size());
  bool prev_slash = false;
  for (char c : s) {
    if (c == '/') {
      if (prev_slash) continue;
      prev_slash = true;
    } else {
      prev_slash = false;
    }
    out.push_back(c);
  }
  if (out.size() > 1 && out.back() == '/') out.pop_back();
  return out;
}

static bool is_direct_child(const std::string& dir_path, const std::string& candidate) {
  std::string prefix = (dir_path == "/") ? "/" : (dir_path + "/");
  if (candidate.size() <= prefix.size()) return false;
  if (candidate.compare(0, prefix.size(), prefix) != 0) return false;
  return candidate.find('/', prefix.size()) == std::string::npos;
}

struct RamFsInner;

namespace ram_internal {

class RamFileImpl final : public detail::IoFileImpl {
public:
  RamFileImpl(std::shared_ptr<RamFsInner> fs, std::string abs_path, std::shared_ptr<std::vector<uint8_t>> data,
              bool write_mode);

  ~RamFileImpl() override;

  size_t read(uint8_t* buf, size_t size) override;
  size_t write(const uint8_t* buf, size_t size) override;
  void flush() override;
  void close() override;
  bool isDirectory() const override;
  std::unique_ptr<detail::IoFileImpl> openNextFileImpl() override;
  const char* name() const override;
  bool valid() const override;
  size_t size() const override;
  size_t position() const override;
  bool seek(size_t pos) override;

private:
  std::shared_ptr<RamFsInner> fs_;
  std::string path_;
  std::shared_ptr<std::vector<uint8_t>> data_;
  bool write_;
  bool valid_;
  size_t pos_ = 0;
  mutable std::string name_cache_;
};

class RamDirImpl final : public detail::IoFileImpl {
public:
  RamDirImpl(std::shared_ptr<RamFsInner> fs, std::string abs_path, std::vector<std::string> children);

  ~RamDirImpl() override;

  size_t read(uint8_t*, size_t) override;
  size_t write(const uint8_t*, size_t) override;
  void flush() override;
  void close() override;
  bool isDirectory() const override;
  std::unique_ptr<detail::IoFileImpl> openNextFileImpl() override;
  const char* name() const override;
  bool valid() const override;
  size_t size() const override;
  size_t position() const override;
  bool seek(size_t pos) override;

private:
  std::shared_ptr<RamFsInner> fs_;
  std::string path_;
  std::vector<std::string> children_;
  size_t idx_ = 0;
  bool valid_ = true;
  mutable std::string name_cache_;
};

}  // namespace ram_internal

struct RamFsInner : std::enable_shared_from_this<RamFsInner> {
  RamFsInner() {
    RamNode root;
    root.is_dir = true;
    nodes_["/"] = root;
  }

  void lock() { mtx_.lock(); }

  void unlock() { mtx_.unlock(); }

  bool reserveBytes(size_t delta) {
    if (used_ + delta > cap_) return false;
    used_ += delta;
    return true;
  }

  void releaseBytes(size_t delta) {
    if (delta > used_) used_ = 0;
    else used_ -= delta;
  }

  IoFile open_path(const char* path, const char* mode, bool create);

  bool path_exists(const char* path);
  bool path_mkdir(const char* path);
  bool path_remove(const char* path);
  bool path_rename(const char* pathFrom, const char* pathTo);
  bool path_rmdir_one(const char* path);

  std::map<std::string, RamNode> nodes_;
  uint64_t used_ = 0;
  uint64_t cap_ = LOFS_RAM_CAP_BYTES;
  std::mutex mtx_;
};

namespace ram_internal {

RamFileImpl::RamFileImpl(std::shared_ptr<RamFsInner> fs, std::string abs_path, std::shared_ptr<std::vector<uint8_t>> data,
                         bool write_mode)
    : fs_(std::move(fs)), path_(std::move(abs_path)), data_(std::move(data)), write_(write_mode), valid_(true) {}

RamFileImpl::~RamFileImpl() { close(); }

size_t RamFileImpl::read(uint8_t* buf, size_t size) {
  if (!valid_ || !data_ || !buf || size == 0) return 0;
  fs_->lock();
  size_t avail = data_->size() > pos_ ? data_->size() - pos_ : 0;
  size_t n = size < avail ? size : avail;
  if (n > 0) memcpy(buf, data_->data() + pos_, n);
  pos_ += n;
  fs_->unlock();
  return n;
}

size_t RamFileImpl::write(const uint8_t* buf, size_t size) {
  if (!valid_ || !write_ || !data_ || !buf || size == 0) return 0;
  fs_->lock();
  if (!fs_->reserveBytes(size)) {
    fs_->unlock();
    return 0;
  }
  size_t required = pos_ + size;
  if (required > data_->size()) data_->resize(required);
  memcpy(data_->data() + pos_, buf, size);
  pos_ += size;
  fs_->unlock();
  return size;
}

void RamFileImpl::flush() {}

void RamFileImpl::close() { valid_ = false; }

bool RamFileImpl::isDirectory() const { return false; }

std::unique_ptr<detail::IoFileImpl> RamFileImpl::openNextFileImpl() { return std::unique_ptr<detail::IoFileImpl>(); }

const char* RamFileImpl::name() const {
  size_t s = path_.rfind('/');
  name_cache_ = (s == std::string::npos) ? path_ : path_.substr(s + 1);
  return name_cache_.c_str();
}

bool RamFileImpl::valid() const { return valid_; }

size_t RamFileImpl::size() const { return (valid_ && data_) ? data_->size() : 0; }

size_t RamFileImpl::position() const { return valid_ ? pos_ : 0; }

bool RamFileImpl::seek(size_t pos) {
  if (!valid_ || !data_) return false;
  pos_ = (pos > data_->size()) ? data_->size() : pos;
  return true;
}

RamDirImpl::RamDirImpl(std::shared_ptr<RamFsInner> fs, std::string abs_path, std::vector<std::string> children)
    : fs_(std::move(fs)), path_(std::move(abs_path)), children_(std::move(children)) {}

RamDirImpl::~RamDirImpl() { close(); }

size_t RamDirImpl::read(uint8_t*, size_t) { return 0; }

size_t RamDirImpl::write(const uint8_t*, size_t) { return 0; }

void RamDirImpl::flush() {}

void RamDirImpl::close() { valid_ = false; }

bool RamDirImpl::isDirectory() const { return true; }

std::unique_ptr<detail::IoFileImpl> RamDirImpl::openNextFileImpl() {
  if (!valid_) return std::unique_ptr<detail::IoFileImpl>();
  while (idx_ < children_.size()) {
    std::string child = children_[idx_++];
    std::string full = (path_ == "/") ? ("/" + child) : (path_ + "/" + child);
    IoFile f = fs_->open_path(full.c_str(), "r", false);
    if (f) return f.releaseImpl();
  }
  return std::unique_ptr<detail::IoFileImpl>();
}

const char* RamDirImpl::name() const {
  size_t s = path_.rfind('/');
  name_cache_ = (s == std::string::npos) ? path_ : path_.substr(s + 1);
  return name_cache_.c_str();
}

bool RamDirImpl::valid() const { return valid_; }

size_t RamDirImpl::size() const { return 0; }

size_t RamDirImpl::position() const { return 0; }

bool RamDirImpl::seek(size_t pos) { return false; }

}  // namespace ram_internal

IoFile RamFsInner::open_path(const char* path, const char* mode, bool create) {
  std::string p = normalize_path(path);
  const bool want_write = mode && (mode[0] == 'w' || mode[0] == 'a');
  const bool truncate = mode && mode[0] == 'w';

  lock();
  auto it = nodes_.find(p);
  if (it == nodes_.end()) {
    if (!want_write || !create) {
      unlock();
      return IoFile();
    }
    RamNode n;
    n.is_dir = false;
    n.data = std::make_shared<std::vector<uint8_t>>();
    nodes_[p] = n;
    it = nodes_.find(p);
  }

  if (it->second.is_dir) {
    if (want_write) {
      unlock();
      return IoFile();
    }
    std::vector<std::string> children;
    for (auto& kv : nodes_) {
      if (kv.first == p) continue;
      if (is_direct_child(p, kv.first)) children.push_back(kv.first.substr(p == "/" ? 1 : p.size() + 1));
    }
    unlock();
    return IoFile(std::unique_ptr<detail::IoFileImpl>(
        new ram_internal::RamDirImpl(shared_from_this(), p, std::move(children))));
  }

  if (want_write && truncate) {
    releaseBytes(it->second.data ? it->second.data->size() : 0);
    if (it->second.data)
      it->second.data->clear();
    else
      it->second.data = std::make_shared<std::vector<uint8_t>>();
  }
  auto data = it->second.data;
  unlock();
  return IoFile(std::unique_ptr<detail::IoFileImpl>(
      new ram_internal::RamFileImpl(shared_from_this(), p, data, want_write)));
}

bool RamFsInner::path_exists(const char* path) {
  std::string p = normalize_path(path);
  lock();
  bool e = nodes_.find(p) != nodes_.end();
  unlock();
  return e;
}

bool RamFsInner::path_mkdir(const char* path) {
  std::string p = normalize_path(path);
  lock();
  if (nodes_.find(p) != nodes_.end()) {
    unlock();
    return false;
  }
  RamNode n;
  n.is_dir = true;
  nodes_[p] = n;
  unlock();
  return true;
}

bool RamFsInner::path_remove(const char* path) {
  std::string p = normalize_path(path);
  lock();
  auto it = nodes_.find(p);
  if (it == nodes_.end() || it->second.is_dir) {
    unlock();
    return false;
  }
  releaseBytes(it->second.data ? it->second.data->size() : 0);
  nodes_.erase(it);
  unlock();
  return true;
}

bool RamFsInner::path_rename(const char* pathFrom, const char* pathTo) {
  std::string from = normalize_path(pathFrom);
  std::string to = normalize_path(pathTo);
  if (from == to) return true;
  lock();
  auto it = nodes_.find(from);
  if (it == nodes_.end()) {
    unlock();
    return false;
  }
  auto dst = nodes_.find(to);
  if (dst != nodes_.end()) {
    if (dst->second.is_dir) {
      unlock();
      return false;
    }
    releaseBytes(dst->second.data ? dst->second.data->size() : 0);
    nodes_.erase(dst);
  }
  nodes_[to] = it->second;
  nodes_.erase(it);
  unlock();
  return true;
}

bool RamFsInner::path_rmdir_one(const char* path) {
  std::string p = normalize_path(path);
  if (p == "/") return false;
  lock();
  auto it = nodes_.find(p);
  if (it == nodes_.end() || !it->second.is_dir) {
    unlock();
    return false;
  }
  for (auto& kv : nodes_) {
    if (kv.first == p) continue;
    if (is_direct_child(p, kv.first)) {
      unlock();
      return false;
    }
  }
  nodes_.erase(it);
  unlock();
  return true;
}

RamBackend::RamBackend() : impl_(std::make_shared<RamFsInner>()) {}

RamBackend::~RamBackend() = default;

RamBackend& RamBackend::instance() {
  static RamBackend inst;
  return inst;
}

bool RamBackend::available() const { return impl_ != nullptr; }

IoFile RamBackend::open(const char* path, uint8_t mode) {
  if (!path || !impl_) return IoFile();
  return impl_->open_path(path, mode == FILE_O_READ ? "r" : "w", mode != FILE_O_READ);
}

IoFile RamBackend::open(const char* path, const char* mode) {
  if (!path || !mode || !impl_) return IoFile();
  const bool create = (mode[0] == 'w' || mode[0] == 'a');
  return impl_->open_path(path, mode, create);
}

bool RamBackend::exists(const char* path) { return path && impl_ && impl_->path_exists(path); }

bool RamBackend::mkdir(const char* path) { return path && impl_ && impl_->path_mkdir(path); }

bool RamBackend::remove(const char* path) { return path && impl_ && impl_->path_remove(path); }

bool RamBackend::rename(const char* from, const char* to) {
  return from && to && impl_ && impl_->path_rename(from, to);
}

bool RamBackend::rmdir(const char* path, bool recursive) {
  if (!path || !impl_) return false;
  if (!recursive) return impl_->path_rmdir_one(path);
  std::string p = normalize_path(path);
  if (p == "/") return false;
  impl_->lock();
  std::vector<std::string> victims;
  std::vector<std::string> subdirs;
  for (auto& kv : impl_->nodes_) {
    if (kv.first == p) continue;
    if (kv.first.size() > p.size() + 1 && kv.first.compare(0, p.size() + 1, p + "/") == 0) {
      if (kv.second.is_dir)
        subdirs.push_back(kv.first);
      else
        victims.push_back(kv.first);
    }
  }
  impl_->unlock();
  for (auto& f : victims) impl_->path_remove(f.c_str());
  for (auto it = subdirs.rbegin(); it != subdirs.rend(); ++it) impl_->path_rmdir_one(it->c_str());
  return impl_->path_rmdir_one(path);
}

uint64_t RamBackend::totalBytes() { return impl_ ? impl_->cap_ : 0; }

uint64_t RamBackend::usedBytes() { return impl_ ? impl_->used_ : 0; }

}  // namespace lofs
