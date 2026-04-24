#include <lostar/Deferred.h>
#include <lostar/Host.h>
#include <lostar/Router.h>

#include <locommand/Router.h>
#include <lolog/LoLog.h>
#include <lomessage/Buffer.h>
#include <lostar/NodeId.h>
#include <louser/Engine.h>

#include <cstring>

namespace {

constexpr size_t kDispatchBufferBytes = 1280;
constexpr size_t kCommandScratchBytes = 256;

/** Strip leading whitespace + UTF-8 BOM so `/?` still matches after phone/app framing. */
void skip_leading_framing(const uint8_t *&data, size_t &len) {
  while (len > 0 && (data[0] == ' ' || data[0] == '\t' || data[0] == '\r' || data[0] == '\n')) {
    data++;
    len--;
  }
  if (len >= 3 && data[0] == 0xEF && data[1] == 0xBB && data[2] == 0xBF) {
    data += 3;
    len -= 3;
  }
  while (len > 0 && (data[0] == ' ' || data[0] == '\t' || data[0] == '\r' || data[0] == '\n')) {
    data++;
    len--;
  }
}

void fill_caller_ref(lostar::NodeRef &out, const lostar_TextDm &dm) {
  out = {};
  out.id = dm.from;
  if (dm.from_pubkey_len > 0) {
    uint16_t n = dm.from_pubkey_len;
    if (n > sizeof(out.ctx)) n = sizeof(out.ctx);
    out.ctx_len = n;
    std::memcpy(out.ctx, dm.from_pubkey, n);
  }
}

}  // namespace

extern "C" bool lostar_ingress_text_dm(const lostar_TextDm *dm) {
  if (!dm || !dm->payload || dm->payload_len == 0) return false;

  const uint8_t *data = dm->payload;
  size_t         len  = dm->payload_len;
  skip_leading_framing(data, len);
  if (len == 0) return false;

  char command[kCommandScratchBytes];
  auto &rt = lostar::router();

  if (data[0] == '/') {
    if (len < 2) return false;
    size_t copy = len - 1;
    if (copy > sizeof(command) - 1) copy = sizeof(command) - 1;
    std::memcpy(command, data + 1, copy);
    command[copy] = '\0';
  } else {
    size_t copy = len;
    if (copy > sizeof(command) - 1) copy = sizeof(command) - 1;
    std::memcpy(command, data, copy);
    command[copy] = '\0';
    if (!rt.matchesGlobalHelp(command)) return false;
  }

  char        rewritten[kCommandScratchBytes];
  const char *dispatch_line = command;
  if (louser::rewrite_alias(command, rewritten, sizeof(rewritten))) dispatch_line = rewritten;

  const bool is_root = rt.matchesAnyRoot(dispatch_line);
  const bool is_help = rt.matchesGlobalHelp(dispatch_line);
  if (!is_root && !is_help) return false;

  lostar::NodeRef caller;
  fill_caller_ref(caller, *dm);

  lomessage::Buffer out(kDispatchBufferBytes);
  void             *app_ctx = static_cast<void *>(&caller);
  if (!rt.dispatch(dispatch_line, out, app_ctx)) return false;

  if (out.length() > 0) {
    lostar_deferred_reply dr = lostar_capture_deferred_reply();
    if (dr.fire) {
      lostar_fire_deferred_reply(&dr, out.c_str(), (uint32_t)out.length());
    } else {
      const lostar_host_ops *ops = lostar_host();
      if (ops && ops->send_text_dm) {
        ops->send_text_dm(ops->ctx, dm->from, out.c_str(), (uint32_t)out.length());
      } else {
        ::lolog::LoLog::debug("lostar", "dispatched but no reply path (no deferrer, no host_ops)");
      }
    }
  }
  return true;
}
