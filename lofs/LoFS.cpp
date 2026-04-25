#include <lofs/LoFS.h>

#include "backends/ExternalFlashBackend.h"
#include "backends/InternalFlashBackend.h"
#include "backends/RamBackend.h"
#include "backends/SdBackend.h"

#include <cstdio>
#include <cstring>

#if __has_include(<lolog/LoLog.h>)
#include <lolog/LoLog.h>
#define LOFS_LOG_DEBUG(...) ::lolog::LoLog::debug("lofs", __VA_ARGS__)
#else
#define LOFS_LOG_DEBUG(...) ((void)0)
#endif

#if defined(ARCH_ESP32)
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
static SemaphoreHandle_t s_lofs_mtx;
static void lofs_lock() {
  if (!s_lofs_mtx) s_lofs_mtx = xSemaphoreCreateRecursiveMutex();
  if (s_lofs_mtx) xSemaphoreTakeRecursive(s_lofs_mtx, portMAX_DELAY);
}
static void lofs_unlock() {
  if (s_lofs_mtx) xSemaphoreGiveRecursive(s_lofs_mtx);
}
#else
static void lofs_lock() {}
static void lofs_unlock() {}
#endif

namespace {

struct MountEntry {
  char prefix[24]{};
  lofs::FsBackend* backend = nullptr;
};

MountEntry s_mounts[8];
uint8_t s_nmounts = 0;

/** Static buffers used ONLY while lofs_lock is held. Saves ~1.5KB of stack per call chain. */
static char s_buf_v1[256];
static char s_buf_v2[256];
static char s_buf_s1[256];
static char s_buf_s2[256];

bool is_mount_prefix(const char* path, const char* pref, size_t plen) {
  if (strncmp(path, pref, plen) != 0) return false;
  return path[plen] == '/' || path[plen] == '\0';
}

/** Normalize user path to a virtual absolute path starting with a mount prefix. */
bool normalize_virtual(const char* in, char* out, size_t cap) {
  if (!in || !out || cap < 8) return false;
  while (*in == ' ') in++;
  if (*in == '\0') return false;

  if (in[0] != '/') {
    int n = snprintf(out, cap, "/__int__/%s", in);
    return n > 0 && (size_t)n < cap;
  }
  if (is_mount_prefix(in, "/__int__", 8) || is_mount_prefix(in, "/__ext__", 8) ||
      is_mount_prefix(in, "/__sd__", 7) || is_mount_prefix(in, "/__ram__", 8)) {
    strncpy(out, in, cap - 1);
    out[cap - 1] = '\0';
    return true;
  }
  int n = snprintf(out, cap, "/__int__%s", in);
  return n > 0 && (size_t)n < cap;
}

bool resolve_locked(const char* virtual_path, lofs::FsBackend** backend, char* stripped, size_t stripped_cap) {
  int best_len = -1;
  uint8_t best_i = 255;
  for (uint8_t i = 0; i < s_nmounts; i++) {
    size_t pl = strlen(s_mounts[i].prefix);
    if (!is_mount_prefix(virtual_path, s_mounts[i].prefix, pl)) continue;
    if ((int)pl > best_len) {
      best_len = (int)pl;
      best_i = i;
    }
  }
  if (best_i == 255 || !backend) return false;
  const char* p = virtual_path + best_len;
  if (*p == '/') p++;
  if (*p == '\0') {
    if (stripped_cap < 2) return false;
    stripped[0] = '/';
    stripped[1] = '\0';
  } else {
    if (*p != '/') {
      if (stripped_cap < 2) return false;
      stripped[0] = '/';
      strncpy(stripped + 1, p, stripped_cap - 2);
      stripped[stripped_cap - 1] = '\0';
    } else {
      strncpy(stripped, p, stripped_cap - 1);
      stripped[stripped_cap - 1] = '\0';
    }
  }
  *backend = s_mounts[best_i].backend;
  return *backend != nullptr;
}

constexpr size_t kCopyBuf = 512;

bool copy_stream(lofs::FsBackend* from_b, const char* from_p, lofs::FsBackend* to_b, const char* to_p) {
  if (!from_b || !to_b || !from_p || !to_p) return false;
  lofs::IoFile in = from_b->open(from_p, FILE_O_READ);
  if (!in) return false;
  lofs::IoFile out = to_b->open(to_p, FILE_O_WRITE);
  if (!out) {
    in.close();
    return false;
  }
  uint8_t buf[kCopyBuf];
  for (;;) {
    size_t n = in.read(buf, sizeof(buf));
    if (n == 0) break;
    size_t w = out.write(buf, n);
    if (w != n) {
      in.close();
      out.close();
      to_b->remove(to_p);
      return false;
    }
  }
  in.close();
  out.close();
  return true;
}

bool rename_via_tmp(const char* src_v, const char* dst_v) {
  lofs::FsBackend* sb = nullptr;
  lofs::FsBackend* db = nullptr;

  if (!resolve_locked(src_v, &sb, s_buf_s1, sizeof(s_buf_s1))) return false;
  char src_stripped[256];
  strncpy(src_stripped, s_buf_s1, sizeof(src_stripped) - 1);
  src_stripped[sizeof(src_stripped) - 1] = '\0';

  if (!resolve_locked(dst_v, &db, s_buf_s2, sizeof(s_buf_s2))) return false;

  if (sb == db) {
    if (db->exists(s_buf_s2)) {
      if (!db->remove(s_buf_s2)) return false;
    }
    return db->rename(src_stripped, s_buf_s2);
  }

  lofs::FsBackend* tb = nullptr;
  int n = snprintf(s_buf_v2, sizeof(s_buf_v2), "%s.t", dst_v);
  if (n <= 0 || (size_t)n >= sizeof(s_buf_v2)) return false;
  if (!resolve_locked(s_buf_v2, &tb, s_buf_s1, sizeof(s_buf_s1))) return false;
  if (tb != db) return false;

  char tmp_stripped[256];
  strncpy(tmp_stripped, s_buf_s1, sizeof(tmp_stripped) - 1);
  tmp_stripped[sizeof(tmp_stripped) - 1] = '\0';

  if (!copy_stream(sb, src_stripped, tb, tmp_stripped)) {
    if (tb->exists(tmp_stripped)) tb->remove(tmp_stripped);
    return false;
  }

  if (!resolve_locked(dst_v, &db, s_buf_s2, sizeof(s_buf_s2))) return false;

  if (db->exists(s_buf_s2)) {
    if (!db->remove(s_buf_s2)) {
      tb->remove(tmp_stripped);
      return false;
    }
  }
  if (!db->rename(tmp_stripped, s_buf_s2)) {
    tb->remove(tmp_stripped);
    return false;
  }
  (void)sb->remove(src_stripped);
  return true;
}

}  // namespace

bool LoFS::mount(const char* prefix, lofs::FsBackend* backend) {
  if (!prefix || !prefix[0] || !backend) return false;
  lofs_lock();
  for (uint8_t i = 0; i < s_nmounts; i++) {
    if (strcmp(s_mounts[i].prefix, prefix) == 0) {
      s_mounts[i].backend = backend;
      lofs_unlock();
      return true;
    }
  }
  if (s_nmounts >= 8) {
    lofs_unlock();
    return false;
  }
  strncpy(s_mounts[s_nmounts].prefix, prefix, sizeof(s_mounts[0].prefix) - 1);
  s_mounts[s_nmounts].prefix[sizeof(s_mounts[0].prefix) - 1] = '\0';
  s_mounts[s_nmounts].backend = backend;
  s_nmounts++;
  lofs_unlock();
  return true;
}

bool LoFS::unmount(const char* prefix) {
  if (!prefix || !prefix[0]) return false;
  lofs_lock();
  for (uint8_t i = 0; i < s_nmounts; i++) {
    if (strcmp(s_mounts[i].prefix, prefix) != 0) continue;
    for (uint8_t j = i + 1; j < s_nmounts; j++) s_mounts[j - 1] = s_mounts[j];
    s_nmounts--;
    lofs_unlock();
    return true;
  }
  lofs_unlock();
  return false;
}

bool LoFS::mount(const char* prefix, lofs::FsVolume* vol) {
  if (!prefix || !prefix[0] || !vol) return false;
  if (strcmp(prefix, "/__int__") == 0) {
    auto& inb = lofs::InternalFlashBackend::instance();
    inb.bindVolume(vol);
    return mount("/__int__", &inb);
  }
  LOFS_LOG_DEBUG("mount(FsVolume): unsupported mount prefix: %s", prefix);
  return false;
}

bool LoFS::mount(std::initializer_list<FsVolumeBinding> bindings) {
  bool ok = true;
  for (const auto& b : bindings) {
    ok = mount(b.prefix, b.vol) && ok;
  }
  return ok;
}

lofs::FsBackend* LoFS::resolveBackend(const char* virtual_path, char* stripped_out, size_t stripped_cap) {
  lofs::FsBackend* b = nullptr;
  lofs_lock();
  bool ok = resolve_locked(virtual_path, &b, stripped_out, stripped_cap);
  lofs_unlock();
  return ok ? b : nullptr;
}

void LoFS::bindInternalFs(lofs::FsVolume* vol) {
  if (!vol) {
    lofs::InternalFlashBackend::instance().bindVolume(nullptr);
    return;
  }
  (void)mount("/__int__", vol);
}

static lofs::IoFile open_norm(const char* filepath, uint8_t mode) {
  lofs_lock();
  if (!normalize_virtual(filepath, s_buf_v1, sizeof(s_buf_v1))) {
    lofs_unlock();
    return {};
  }
  lofs::FsBackend* b = nullptr;
  if (!resolve_locked(s_buf_v1, &b, s_buf_s1, sizeof(s_buf_s1))) {
    lofs_unlock();
    return {};
  }
  lofs::IoFile f = b->open(s_buf_s1, mode);
  lofs_unlock();
  return f;
}

static lofs::IoFile open_norm_str(const char* filepath, const char* mode) {
  lofs_lock();
  if (!normalize_virtual(filepath, s_buf_v1, sizeof(s_buf_v1))) {
    lofs_unlock();
    return {};
  }
  lofs::FsBackend* b = nullptr;
  if (!resolve_locked(s_buf_v1, &b, s_buf_s1, sizeof(s_buf_s1))) {
    lofs_unlock();
    return {};
  }
  lofs::IoFile f = b->open(s_buf_s1, mode);
  lofs_unlock();
  return f;
}

lofs::IoFile LoFS::open(const char* filepath, uint8_t mode) { return open_norm(filepath, mode); }

lofs::IoFile LoFS::open(const char* filepath, const char* mode) { return open_norm_str(filepath, mode); }

bool LoFS::exists(const char* filepath) {
  lofs_lock();
  if (!normalize_virtual(filepath, s_buf_v1, sizeof(s_buf_v1))) {
    lofs_unlock();
    return false;
  }
  lofs::FsBackend* b = nullptr;
  if (!resolve_locked(s_buf_v1, &b, s_buf_s1, sizeof(s_buf_s1))) {
    lofs_unlock();
    return false;
  }
  bool e = b->exists(s_buf_s1);
  lofs_unlock();
  return e;
}

bool LoFS::mkdir(const char* filepath) {
  lofs_lock();
  if (!normalize_virtual(filepath, s_buf_v1, sizeof(s_buf_v1))) {
    lofs_unlock();
    return false;
  }
  lofs::FsBackend* b = nullptr;
  if (!resolve_locked(s_buf_v1, &b, s_buf_s1, sizeof(s_buf_s1))) {
    lofs_unlock();
    return false;
  }
  bool ok = b->mkdir(s_buf_s1);
  lofs_unlock();
  return ok;
}

bool LoFS::remove(const char* filepath) {
  lofs_lock();
  if (!normalize_virtual(filepath, s_buf_v1, sizeof(s_buf_v1))) {
    lofs_unlock();
    return false;
  }
  lofs::FsBackend* b = nullptr;
  if (!resolve_locked(s_buf_v1, &b, s_buf_s1, sizeof(s_buf_s1))) {
    lofs_unlock();
    return false;
  }
  bool ok = b->remove(s_buf_s1);
  lofs_unlock();
  return ok;
}

bool LoFS::rename(const char* oldfilepath, const char* newfilepath) {
  if (!oldfilepath || !newfilepath) return false;
  lofs_lock();
  if (!normalize_virtual(oldfilepath, s_buf_v1, sizeof(s_buf_v1))) {
    lofs_unlock();
    return false;
  }
  // We must copy v1 because normalize_virtual for v2 will clobber it.
  char src_v[256];
  strncpy(src_v, s_buf_v1, sizeof(src_v) - 1);
  src_v[sizeof(src_v) - 1] = '\0';

  if (!normalize_virtual(newfilepath, s_buf_v1, sizeof(s_buf_v1))) {
    lofs_unlock();
    return false;
  }
  bool ok = rename_via_tmp(src_v, s_buf_v1);
  lofs_unlock();
  return ok;
}

bool LoFS::writeFileAtomic(const char* filepath, const uint8_t* data, size_t size) {
  if (!filepath || !data || size == 0) return false;
  char tmp_path[280];
  int n = snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", filepath);
  if (n <= 0 || (size_t)n >= sizeof(tmp_path)) return false;

  // Best-effort cleanup from any previous interrupted write.
  if (LoFS::exists(tmp_path)) (void)LoFS::remove(tmp_path);

  lofs::IoFile f = LoFS::open(tmp_path, FILE_O_WRITE);
  if (!f) return false;

  size_t written = f.write(data, size);
  if (written != size) {
    f.close();
    (void)LoFS::remove(tmp_path);
    return false;
  }

  f.flush();
  f.close();

  if (!LoFS::rename(tmp_path, filepath)) {
    (void)LoFS::remove(tmp_path);
    return false;
  }
  return true;
}

bool LoFS::readFileAtomic(const char* filepath, uint8_t* out, size_t cap, size_t* out_size) {
  if (!filepath || !out || cap == 0 || !out_size) return false;
  *out_size = 0;

  // Guard against FS backends that may return a truthy File for absent paths.
  if (!LoFS::exists(filepath)) return false;

  lofs::IoFile f = LoFS::open(filepath, FILE_O_READ);
  if (!f) {
    LOFS_LOG_DEBUG("readFileAtomic: exists but open failed: %s", filepath);
    return false;
  }

  // Some backends can return handles to directories via open("r").
  if (f.isDirectory()) {
    LOFS_LOG_DEBUG("readFileAtomic: path opened as directory: %s", filepath);
    f.close();
    return false;
  }

  size_t n = f.read(out, cap);
  f.close();

  // Treat an empty read with a now-absent path as not-found.
  if (n == 0 && !LoFS::exists(filepath)) return false;

  *out_size = n;
  return true;
}

bool LoFS::rmdir(const char* filepath, bool recursive) {
  lofs_lock();
  if (!normalize_virtual(filepath, s_buf_v1, sizeof(s_buf_v1))) {
    lofs_unlock();
    return false;
  }
  lofs::FsBackend* b = nullptr;
  if (!resolve_locked(s_buf_v1, &b, s_buf_s1, sizeof(s_buf_s1))) {
    lofs_unlock();
    return false;
  }
  bool ok = b->rmdir(s_buf_s1, recursive);
  lofs_unlock();
  return ok;
}

uint64_t LoFS::totalBytes(const char* filepath) {
  lofs_lock();
  if (!normalize_virtual(filepath, s_buf_v1, sizeof(s_buf_v1))) {
    lofs_unlock();
    return 0;
  }
  lofs::FsBackend* b = nullptr;
  if (!resolve_locked(s_buf_v1, &b, s_buf_s1, sizeof(s_buf_s1))) {
    lofs_unlock();
    return 0;
  }
  uint64_t t = b->totalBytes();
  lofs_unlock();
  return t;
}

uint64_t LoFS::usedBytes(const char* filepath) {
  lofs_lock();
  if (!normalize_virtual(filepath, s_buf_v1, sizeof(s_buf_v1))) {
    lofs_unlock();
    return 0;
  }
  lofs::FsBackend* b = nullptr;
  if (!resolve_locked(s_buf_v1, &b, s_buf_s1, sizeof(s_buf_s1))) {
    lofs_unlock();
    return 0;
  }
  uint64_t u = b->usedBytes();
  lofs_unlock();
  return u;
}

uint64_t LoFS::freeBytes(const char* filepath) {
  uint64_t t = totalBytes(filepath);
  uint64_t u = usedBytes(filepath);
  return t > u ? t - u : 0;
}

bool LoFS::isSDCardAvailable() { return lofs::SdBackend::instance().available(); }
