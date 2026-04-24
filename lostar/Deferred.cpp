#include <lostar/Deferred.h>

#include <string.h>

namespace {

lostar_deferred_reply g_current = {nullptr, nullptr};
bool                  g_has_current = false;
const void           *g_host_token = nullptr;

}  // namespace

extern "C" void lostar_ingress_attach_deferrer(const lostar_deferred_reply *d) {
  if (!d) {
    g_current.fire      = nullptr;
    g_current.route_ctx = nullptr;
    g_has_current       = false;
    return;
  }
  g_current      = *d;
  g_has_current  = (d->fire != nullptr);
}

extern "C" lostar_deferred_reply lostar_capture_deferred_reply(void) {
  if (g_has_current) return g_current;
  lostar_deferred_reply stub = {nullptr, nullptr};
  return stub;
}

extern "C" void lostar_fire_deferred_reply(const lostar_deferred_reply *d, const char *text, uint32_t len) {
  if (!d || !d->fire || !text) return;
  d->fire(d->route_ctx, text, len);
}

extern "C" void lostar_ingress_set_host_token(const void *tok) { g_host_token = tok; }

extern "C" const void *lostar_ingress_host_token(void) { return g_host_token; }
