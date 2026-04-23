#include <louser/Session.h>

#include <lolog/LoLog.h>
#include <lostar/Platform.h>

#include "louser.pb.h"

#include <Arduino.h>
#include <cstring>

namespace louser {

namespace {

constexpr const char* kDbName = "louser";
constexpr const char* kTable  = "sessions";

char key_buf[16];
const char* node_id_key(lostar::NodeId id) {
  snprintf(key_buf, sizeof(key_buf), "%08x", (unsigned)id);
  return key_buf;
}

void copy_from_pb(Session& out, const LoUserSession& r) {
  out.node_id    = r.node_id;
  out.user_id    = r.user_id;
  out.bind_fp_len = r.bind_fp.size <= sizeof(out.bind_fp) ? (uint16_t)r.bind_fp.size
                                                          : (uint16_t)sizeof(out.bind_fp);
  memcpy(out.bind_fp, r.bind_fp.bytes, out.bind_fp_len);
  out.created_ms = r.created_ms;
}

}  // namespace

Sessions::Sessions(const char* mount) : _db(kDbName, mount) {}

void Sessions::ensure_registered() {
  if (_registered) return;
  _db.registerTable(kTable, &LoUserSession_msg, sizeof(LoUserSession));
  _registered = true;
}

bool Sessions::login(const lostar::NodeRef& caller, uint64_t user_id) {
  ensure_registered();
  LoUserSession rec = LoUserSession_init_zero;
  rec.node_id = caller.id;
  rec.user_id = user_id;
  uint16_t n = caller.ctx_len;
  if (n > sizeof(rec.bind_fp.bytes)) n = sizeof(rec.bind_fp.bytes);
  rec.bind_fp.size = n;
  memcpy(rec.bind_fp.bytes, caller.ctx, n);
  rec.created_ms = (uint32_t)millis();

  const char* key = node_id_key(caller.id);
  if (_db.update(kTable, key, &rec) == LODB_OK) return true;
  return _db.insert(kTable, key, &rec) == LODB_OK;
}

bool Sessions::logout(lostar::NodeId node_id) {
  ensure_registered();
  return _db.deleteRecord(kTable, node_id_key(node_id)) == LODB_OK;
}

uint64_t Sessions::whoami(const lostar::NodeRef& caller) {
  ensure_registered();
  LoUserSession rec = LoUserSession_init_zero;
  if (_db.get(kTable, node_id_key(caller.id), &rec) != LODB_OK) return 0;

  lostar::NodeRef stored{};
  stored.id = rec.node_id;
  stored.ctx_len = (uint16_t)rec.bind_fp.size;
  if (stored.ctx_len > sizeof(stored.ctx)) stored.ctx_len = (uint16_t)sizeof(stored.ctx);
  memcpy(stored.ctx, rec.bind_fp.bytes, stored.ctx_len);

  if (!lotato::platform::bind_check(stored, caller)) {
    ::lolog::LoLog::debug("louser", "bind_check mismatch id=%08x — treating as signed-out", (unsigned)caller.id);
    return 0;
  }
  return rec.user_id;
}

bool Sessions::clearAll() {
  ensure_registered();
  return _db.truncate(kTable) == LODB_OK;
}

bool Sessions::dropForUser(uint64_t user_id) {
  ensure_registered();
  auto rows = _db.select(kTable, [&](const void* r) {
    auto* s = (const LoUserSession*)r;
    return s->user_id == user_id;
  });
  bool any = false;
  for (void* r : rows) {
    auto* s = (LoUserSession*)r;
    char key[16];
    snprintf(key, sizeof(key), "%08x", (unsigned)s->node_id);
    if (_db.deleteRecord(kTable, key) == LODB_OK) any = true;
  }
  LoDb::freeRecords(rows);
  return any;
}

}  // namespace louser
