#include <losettings/LoSettings.h>

#include "losettings.pb.h"

#include <cstdlib>
#include <cctype>
#include <cstring>
#include <cstdio>

#if __has_include(<lolog/LoLog.h>)
#include <lolog/LoLog.h>
#define LOSETTINGS_LOG_DEBUG(...) ::lolog::LoLog::debug("losettings", __VA_ARGS__)
#define LOSETTINGS_LOG_INFO(...) ::lolog::LoLog::info("losettings", __VA_ARGS__)
#define LOSETTINGS_LOG_ERROR(...) ::lolog::LoLog::error("losettings", __VA_ARGS__)
#else
#define LOSETTINGS_LOG_DEBUG(...) ((void)0)
#define LOSETTINGS_LOG_INFO(...) ((void)0)
#define LOSETTINGS_LOG_ERROR(...) ((void)0)
#endif

namespace losettings {

namespace {

constexpr const char* kTable = "kv";

enum Kind : uint32_t {
  KIND_BOOL = 1,
  KIND_INT32 = 2,
  KIND_UINT32 = 3,
  KIND_FLOAT = 4,
  KIND_STRING = 5,
  KIND_BYTES = 6,
};

bool key_is_sensitive(const char* key) {
  if (!key) return false;
  char low[40];
  size_t n = strlen(key);
  if (n >= sizeof(low)) n = sizeof(low) - 1;
  for (size_t i = 0; i < n; i++) low[i] = (char)tolower((unsigned char)key[i]);
  low[n] = '\0';
  return strstr(low, "token") || strstr(low, "password") || strstr(low, "psk") || strstr(low, "secret") ||
         strstr(low, "auth");
}

bool should_redact_sensitive(const char* key) {
  if (!key_is_sensitive(key)) return false;
#if __has_include(<lolog/LoLog.h>)
  return !::lolog::LoLog::isVerbose();
#else
  return true;
#endif
}

void format_value_preview(const LoSettingsKv& rec, const char* key, char* out, size_t out_cap) {
  if (!out || out_cap < 2) return;
  out[0] = '\0';
  if (should_redact_sensitive(key)) {
    snprintf(out, out_cap, "(redacted)");
    return;
  }
  switch (rec.kind) {
    case KIND_BOOL:
      snprintf(out, out_cap, "%s", (rec.data.size >= 1 && rec.data.bytes[0]) ? "true" : "false");
      return;
    case KIND_INT32: {
      if (rec.data.size < 4) {
        snprintf(out, out_cap, "(bad-int32)");
        return;
      }
      int32_t v = 0;
      memcpy(&v, rec.data.bytes, 4);
      snprintf(out, out_cap, "%ld", (long)v);
      return;
    }
    case KIND_UINT32: {
      if (rec.data.size < 4) {
        snprintf(out, out_cap, "(bad-uint32)");
        return;
      }
      uint32_t v = 0;
      memcpy(&v, rec.data.bytes, 4);
      snprintf(out, out_cap, "%lu", (unsigned long)v);
      return;
    }
    case KIND_FLOAT: {
      if (rec.data.size < 4) {
        snprintf(out, out_cap, "(bad-float)");
        return;
      }
      float v = 0.0f;
      memcpy(&v, rec.data.bytes, 4);
      snprintf(out, out_cap, "%.3f", (double)v);
      return;
    }
    case KIND_STRING: {
      size_t n = rec.data.size;
      if (n > 96) n = 96;
      if (n > out_cap - 1) n = out_cap - 1;
      memcpy(out, rec.data.bytes, n);
      out[n] = '\0';
      return;
    }
    case KIND_BYTES:
      snprintf(out, out_cap, "(bytes:%u)", (unsigned)rec.data.size);
      return;
    default:
      snprintf(out, out_cap, "(kind:%lu size:%u)", (unsigned long)rec.kind, (unsigned)rec.data.size);
      return;
  }
}

bool load(LoDb& db, const char* key, LoSettingsKv& out) {
  if (!key) return false;
  bool ok = db.get(kTable, key, &out) == LODB_OK;
  if (ok) {
    char preview[128];
    format_value_preview(out, key, preview, sizeof(preview));
    LOSETTINGS_LOG_DEBUG("read table=%s key=%s value=%s", kTable, key, preview);
  }
  return ok;
}

bool save(LoDb& db, const char* key, const LoSettingsKv& rec) {
  bool ok = db.update(kTable, key, &rec) == LODB_OK;
  if (!ok) ok = db.insert(kTable, key, &rec) == LODB_OK;
  if (ok) {
    char preview[128];
    format_value_preview(rec, key, preview, sizeof(preview));
    LOSETTINGS_LOG_DEBUG("write table=%s key=%s value=%s", kTable, key, preview);
  }
  return ok;
}

void fill_base(LoSettingsKv& rec, const char* key, uint32_t kind) {
  memset(&rec, 0, sizeof(rec));
  strncpy(rec.key, key, sizeof(rec.key) - 1);
  rec.kind = kind;
}

}  // namespace

LoSettings::LoSettings(const char* ns, const char* mount) : _db(ns, mount) {}

void LoSettings::ensure_registered() {
  if (_registered) return;
  _db.registerTable(kTable, &LoSettingsKv_msg, sizeof(LoSettingsKv));
  _registered = true;
}

bool LoSettings::valid_key(const char* key) const {
  if (!key || !key[0]) return false;
  size_t n = strlen(key);
  if (n > 32) return false;
  for (size_t i = 0; i < n; i++) {
    char c = key[i];
    bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '.' ||
              c == '-';
    if (!ok) return false;
  }
  return true;
}

bool LoSettings::has(const char* key) {
  if (!valid_key(key)) return false;
  ensure_registered();
  LoSettingsKv rec = LoSettingsKv_init_zero;
  return load(_db, key, rec);
}

bool LoSettings::remove(const char* key) {
  if (!valid_key(key)) return false;
  ensure_registered();
  return _db.deleteRecord(kTable, key) == LODB_OK;
}

bool LoSettings::clear() {
  ensure_registered();
  return _db.truncate(kTable) == LODB_OK;
}

bool LoSettings::getBool(const char* key, bool def) {
  if (!valid_key(key)) return def;
  ensure_registered();
  LoSettingsKv rec = LoSettingsKv_init_zero;
  if (!load(_db, key, rec) || rec.kind != KIND_BOOL || rec.data.size < 1) return def;
  return rec.data.bytes[0] != 0;
}

int32_t LoSettings::getInt(const char* key, int32_t def) {
  if (!valid_key(key)) return def;
  ensure_registered();
  LoSettingsKv rec = LoSettingsKv_init_zero;
  if (!load(_db, key, rec) || rec.kind != KIND_INT32 || rec.data.size < 4) return def;
  int32_t v;
  memcpy(&v, rec.data.bytes, 4);
  return v;
}

uint32_t LoSettings::getUInt(const char* key, uint32_t def) {
  if (!valid_key(key)) return def;
  ensure_registered();
  LoSettingsKv rec = LoSettingsKv_init_zero;
  if (!load(_db, key, rec) || rec.kind != KIND_UINT32 || rec.data.size < 4) return def;
  uint32_t v;
  memcpy(&v, rec.data.bytes, 4);
  return v;
}

float LoSettings::getFloat(const char* key, float def) {
  if (!valid_key(key)) return def;
  ensure_registered();
  LoSettingsKv rec = LoSettingsKv_init_zero;
  if (!load(_db, key, rec) || rec.kind != KIND_FLOAT || rec.data.size < 4) return def;
  float v;
  memcpy(&v, rec.data.bytes, 4);
  return v;
}

size_t LoSettings::getString(const char* key, char* out, size_t cap, const char* def) {
  if (!out || cap < 1) return 0;
  out[0] = '\0';
  const char* src = def ? def : "";
  size_t src_len = strlen(src);
  size_t dn = (src_len < cap - 1) ? src_len : cap - 1;
  if (!valid_key(key)) {
    memcpy(out, src, dn);
    out[dn] = '\0';
    return dn;
  }
  ensure_registered();
  LoSettingsKv rec = LoSettingsKv_init_zero;
  if (!load(_db, key, rec) || rec.kind != KIND_STRING) {
    memcpy(out, src, dn);
    out[dn] = '\0';
    return dn;
  }
  size_t n = rec.data.size;
  if (n > cap - 1) n = cap - 1;
  memcpy(out, rec.data.bytes, n);
  out[n] = '\0';
  return n;
}

size_t LoSettings::getBytes(const char* key, uint8_t* out, size_t cap) {
  if (!valid_key(key) || !out || cap == 0) return 0;
  ensure_registered();
  LoSettingsKv rec = LoSettingsKv_init_zero;
  if (!load(_db, key, rec) || rec.kind != KIND_BYTES) return 0;
  size_t n = rec.data.size;
  if (n > cap) n = cap;
  memcpy(out, rec.data.bytes, n);
  return n;
}

bool LoSettings::setBool(const char* key, bool v) {
  if (!valid_key(key)) return false;
  ensure_registered();
  LoSettingsKv rec;
  if (load(_db, key, rec) && rec.kind == KIND_BOOL && rec.data.size == 1 && (rec.data.bytes[0] != 0) == v) {
    return true; // Unchanged
  }
  fill_base(rec, key, KIND_BOOL);
  rec.data.size = 1;
  rec.data.bytes[0] = v ? 1 : 0;
  return save(_db, key, rec);
}

bool LoSettings::setInt(const char* key, int32_t v) {
  if (!valid_key(key)) return false;
  ensure_registered();
  LoSettingsKv rec;
  if (load(_db, key, rec) && rec.kind == KIND_INT32 && rec.data.size == 4) {
    int32_t exist;
    memcpy(&exist, rec.data.bytes, 4);
    if (exist == v) return true;
  }
  fill_base(rec, key, KIND_INT32);
  rec.data.size = 4;
  memcpy(rec.data.bytes, &v, 4);
  return save(_db, key, rec);
}

bool LoSettings::setUInt(const char* key, uint32_t v) {
  if (!valid_key(key)) return false;
  ensure_registered();
  LoSettingsKv rec;
  if (load(_db, key, rec) && rec.kind == KIND_UINT32 && rec.data.size == 4) {
    uint32_t exist;
    memcpy(&exist, rec.data.bytes, 4);
    if (exist == v) return true;
  }
  fill_base(rec, key, KIND_UINT32);
  rec.data.size = 4;
  memcpy(rec.data.bytes, &v, 4);
  return save(_db, key, rec);
}

bool LoSettings::setFloat(const char* key, float v) {
  if (!valid_key(key)) return false;
  ensure_registered();
  LoSettingsKv rec;
  if (load(_db, key, rec) && rec.kind == KIND_FLOAT && rec.data.size == 4) {
    float exist;
    memcpy(&exist, rec.data.bytes, 4);
    if (exist == v) return true;
  }
  fill_base(rec, key, KIND_FLOAT);
  rec.data.size = 4;
  memcpy(rec.data.bytes, &v, 4);
  return save(_db, key, rec);
}

bool LoSettings::setString(const char* key, const char* v) {
  if (!valid_key(key)) return false;
  ensure_registered();
  const char* src = v ? v : "";
  size_t n = strlen(src);
  if (n > sizeof(LoSettingsKv::data.bytes)) n = sizeof(LoSettingsKv::data.bytes);
  
  LoSettingsKv rec;
  if (load(_db, key, rec) && rec.kind == KIND_STRING && rec.data.size == n) {
    if (n == 0 || memcmp(rec.data.bytes, src, n) == 0) return true;
  }
  
  fill_base(rec, key, KIND_STRING);
  rec.data.size = (pb_size_t)n;
  if (n) memcpy(rec.data.bytes, src, n);
  return save(_db, key, rec);
}

bool LoSettings::setBytes(const char* key, const uint8_t* v, size_t n) {
  if (!valid_key(key) || (!v && n)) return false;
  ensure_registered();
  if (n > sizeof(LoSettingsKv::data.bytes)) n = sizeof(LoSettingsKv::data.bytes);
  
  LoSettingsKv rec;
  if (load(_db, key, rec) && rec.kind == KIND_BYTES && rec.data.size == n) {
    if (n == 0 || memcmp(rec.data.bytes, v, n) == 0) return true;
  }
  
  fill_base(rec, key, KIND_BYTES);
  rec.data.size = (pb_size_t)n;
  if (n) memcpy(rec.data.bytes, v, n);
  return save(_db, key, rec);
}

int LoSettings::listKeys(char keys[][32], int max) {
  ensure_registered();
  if (!keys || max <= 0) return 0;
  auto rows = _db.select(kTable);
  int n = 0;
  for (void* row : rows) {
    if (n >= max) break;
    LoSettingsKv* kv = (LoSettingsKv*)row;
    strncpy(keys[n], kv->key, 31);
    keys[n][31] = '\0';
    n++;
  }
  LoDb::freeRecords(rows);
  return n;
}

#if defined(LOSETTINGS_TEST)
bool losettings_run_selftest(const char* mount) {
  static bool ran = false;
  if (ran) return true;
  ran = true;

  bool ok = true;
  auto expect = [&](bool cond, const char* msg) {
    if (!cond) {
      LOSETTINGS_LOG_ERROR("selftest FAIL: %s", msg);
      ok = false;
    }
  };

  LoSettings st("losettings_selftest", mount);
  expect(st.clear(), "clear");
  expect(st.setBool("b", true), "setBool");
  expect(st.setInt("i", -12345), "setInt");
  expect(st.setUInt("u", 12345u), "setUInt");
  expect(st.setFloat("f", 3.25f), "setFloat");
  expect(st.setString("s", "hello"), "setString");
  uint8_t bytes_in[8] = {0, 1, 2, 3, 0xaa, 0xbb, 0xcc, 0xdd};
  expect(st.setBytes("x", bytes_in, sizeof(bytes_in)), "setBytes");

  expect(st.getBool("b", false) == true, "getBool");
  expect(st.getInt("i", 0) == -12345, "getInt");
  expect(st.getUInt("u", 0) == 12345u, "getUInt");
  float fv = st.getFloat("f", 0.0f);
  expect(fv > 3.249f && fv < 3.251f, "getFloat");

  char sbuf[16];
  size_t sn = st.getString("s", sbuf, sizeof(sbuf), "");
  expect(sn == 5 && strcmp(sbuf, "hello") == 0, "getString");

  uint8_t bytes_out[8] = {};
  size_t xn = st.getBytes("x", bytes_out, sizeof(bytes_out));
  expect(xn == sizeof(bytes_in) && memcmp(bytes_in, bytes_out, sizeof(bytes_in)) == 0, "getBytes");

  char keys[16][32];
  int key_n = st.listKeys(keys, 16);
  expect(key_n >= 6, "listKeys");

  expect(st.remove("s"), "remove");
  expect(!st.has("s"), "has false after remove");
  expect(st.clear(), "clear end");
  LOSETTINGS_LOG_INFO("selftest %s", ok ? "PASS" : "FAIL");
  return ok;
}
#endif

}  // namespace losettings
