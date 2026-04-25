#include <louser/User.h>
#include <louser/Hash.h>

#include "louser.pb.h"

#include <lolog/LoLog.h>

#include <Arduino.h>
#include <cctype>
#include <cinttypes>
#include <cstring>

namespace louser {

namespace {

constexpr const char* kDbName = "louser";
constexpr const char* kTable  = "users";

bool valid_username(const char* u) {
  if (!u || !u[0]) return false;
  size_t n = strlen(u);
  if (n > 32) return false;
  for (size_t i = 0; i < n; i++) {
    char c = u[i];
    bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' ||
              c == '-' || c == '.';
    if (!ok) return false;
  }
  return true;
}

int ci_cmp(const char* a, const char* b) {
  while (*a && *b) {
    int ca = tolower((unsigned char)*a++);
    int cb = tolower((unsigned char)*b++);
    if (ca != cb) return ca - cb;
  }
  return (unsigned char)*a - (unsigned char)*b;
}

void copy_from_pb(User& out, const LoUserUser& r) {
  out.id = r.id;
  strncpy(out.username, r.username, sizeof(out.username) - 1);
  out.username[sizeof(out.username) - 1] = '\0';
  out.salt_len = r.salt.size <= sizeof(out.salt) ? (uint16_t)r.salt.size : (uint16_t)sizeof(out.salt);
  memcpy(out.salt, r.salt.bytes, out.salt_len);
  out.password_hash_len = r.password_hash.size <= sizeof(out.password_hash) ? (uint16_t)r.password_hash.size
                                                                             : (uint16_t)sizeof(out.password_hash);
  memcpy(out.password_hash, r.password_hash.bytes, out.password_hash_len);
  out.admin = r.admin;
  out.created_ms = r.created_ms;
}

void copy_to_pb(const User& u, LoUserUser& out) {
  memset(&out, 0, sizeof(out));
  out.id = u.id;
  strncpy(out.username, u.username, sizeof(out.username) - 1);
  out.salt.size = u.salt_len;
  memcpy(out.salt.bytes, u.salt, u.salt_len);
  out.password_hash.size = u.password_hash_len;
  memcpy(out.password_hash.bytes, u.password_hash, u.password_hash_len);
  out.admin = u.admin;
  out.created_ms = u.created_ms;
}

char id_key_buf[24];
const char* id_to_key(uint64_t id) {
  snprintf(id_key_buf, sizeof(id_key_buf), "%" PRIu64, id);
  return id_key_buf;
}

uint64_t next_id(std::vector<void*>& rows) {
  uint64_t max = 0;
  for (void* r : rows) {
    auto* u = (LoUserUser*)r;
    if (u->id > max) max = u->id;
  }
  return max + 1;
}

}  // namespace

Users::Users(const char* mount) : _db(kDbName, mount) {}

void Users::ensure_registered() {
  if (_registered) return;
  _db.registerTable(kTable, &LoUserUser_msg, sizeof(LoUserUser));
  _registered = true;
}

bool Users::findByUsername(const char* username, User& out) {
  if (!valid_username(username)) return false;
  ensure_registered();
  auto rows = _db.select(kTable, [&](const void* r) {
    auto* u = (const LoUserUser*)r;
    return ci_cmp(u->username, username) == 0;
  }, nullptr, 1);
  bool hit = !rows.empty();
  if (hit) copy_from_pb(out, *(LoUserUser*)rows[0]);
  LoDb::freeRecords(rows);
  return hit;
}

bool Users::findById(uint64_t id, User& out) {
  ensure_registered();
  LoUserUser rec = LoUserUser_init_zero;
  if (_db.get(kTable, id_to_key(id), &rec) != LODB_OK) return false;
  copy_from_pb(out, rec);
  return true;
}

int Users::count() {
  ensure_registered();
  return _db.count(kTable);
}

int Users::create(const char* username, const char* password, User* created_out) {
  if (!valid_username(username) || !password || !password[0]) return 2;
  ensure_registered();

  auto all_rows = _db.select(kTable);
  bool first_user = all_rows.empty();
  for (void* r : all_rows) {
    auto* u = (LoUserUser*)r;
    if (ci_cmp(u->username, username) == 0) {
      LoDb::freeRecords(all_rows);
      return 1;
    }
  }
  uint64_t new_id = next_id(all_rows);
  LoDb::freeRecords(all_rows);

  User u;
  u.id = new_id;
  strncpy(u.username, username, sizeof(u.username) - 1);
  u.salt_len = 32;
  random_bytes(u.salt, u.salt_len);
  hash_password(u.salt, u.salt_len, password, u.password_hash);
  u.password_hash_len = 32;
  u.admin = first_user;
  u.created_ms = (uint32_t)millis();

  LoUserUser pb;
  copy_to_pb(u, pb);
  if (_db.insert(kTable, id_to_key(u.id), &pb) != LODB_OK) return 3;

  if (created_out) *created_out = u;
  ::lolog::LoLog::info("louser", "user created id=%" PRIu64 " name=%s admin=%d", u.id, u.username,
                        (int)u.admin);
  return 0;
}

bool Users::deleteUser(const char* username) {
  User u;
  if (!findByUsername(username, u)) return false;
  return _db.deleteRecord(kTable, id_to_key(u.id)) == LODB_OK;
}

int Users::rename(const char* old_name, const char* new_name) {
  if (!valid_username(old_name) || !valid_username(new_name)) return 3;
  User existing;
  if (findByUsername(new_name, existing)) return 2;
  User u;
  if (!findByUsername(old_name, u)) return 1;
  strncpy(u.username, new_name, sizeof(u.username) - 1);
  u.username[sizeof(u.username) - 1] = '\0';
  LoUserUser pb;
  copy_to_pb(u, pb);
  return _db.update(kTable, id_to_key(u.id), &pb) == LODB_OK ? 0 : 4;
}

bool Users::setPassword(const char* username, const char* new_password) {
  if (!new_password || !new_password[0]) return false;
  User u;
  if (!findByUsername(username, u)) return false;
  u.salt_len = 32;
  random_bytes(u.salt, u.salt_len);
  hash_password(u.salt, u.salt_len, new_password, u.password_hash);
  u.password_hash_len = 32;
  LoUserUser pb;
  copy_to_pb(u, pb);
  return _db.update(kTable, id_to_key(u.id), &pb) == LODB_OK;
}

bool Users::verifyPassword(const User& user, const char* password) {
  if (!password) return false;
  uint8_t calc[32];
  hash_password(user.salt, user.salt_len, password, calc);
  if (user.password_hash_len != 32) return false;
  // Constant-time compare.
  uint8_t diff = 0;
  for (int i = 0; i < 32; i++) diff |= (uint8_t)(calc[i] ^ user.password_hash[i]);
  return diff == 0;
}

}  // namespace louser
