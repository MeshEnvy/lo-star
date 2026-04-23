#include <lostar/Platform.h>

#include <cstring>

namespace lotato {
namespace platform {

/**
 * Weak default. Platform delegates override with a strong definition; the fallback treats any
 * two NodeRefs with the same `id` as the same peer, which is correct for platforms whose native
 * identity already equals NodeId (e.g. meshtastic) and safe for host tests.
 */
__attribute__((weak)) bool bind_check(const lostar::NodeRef& stored, const lostar::NodeRef& incoming) {
  if (stored.ctx_len == 0 || incoming.ctx_len == 0) return stored.id == incoming.id;
  if (stored.ctx_len != incoming.ctx_len) return false;
  return memcmp(stored.ctx, incoming.ctx, stored.ctx_len) == 0;
}

}  // namespace platform
}  // namespace lotato
