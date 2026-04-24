#include <lostar/Host.h>

#include <lostar/Deferred.h>
#include <lostar/Types.h>

#include <string.h>

/* Layout sentinels mirrored in each fork adapter (see meshtastic/src/lostar_adapter.cpp and
 * meshcore/.../lostar_adapter.cpp). Values reflect the 32-bit-pointer ABI used by every
 * supported MCU target (ESP32, nRF52); both this TU and the adapter TU compile for the same
 * target, so any mismatch is a compile error on at least one side. Skipped on 64-bit hosts
 * (native tests) since those only compile lostar for symbol-existence checks. */
#if UINTPTR_MAX == 0xFFFFFFFFu
static_assert(sizeof(lostar_TextDm)         == 56,  "lostar_TextDm layout changed");
static_assert(sizeof(lostar_NodeAdvert)     == 104, "lostar_NodeAdvert layout changed");
static_assert(sizeof(lostar_host_ops)       == 20,  "lostar_host_ops layout changed");
static_assert(sizeof(lostar_deferred_reply) == 8,   "lostar_deferred_reply layout changed");
#endif

namespace {

constexpr int kMaxTickHooks = 8;

lostar_host_ops g_ops;
bool            g_ops_installed = false;

struct TickSlot {
  lostar_tick_fn fn;
  void          *ctx;
};

TickSlot g_ticks[kMaxTickHooks];
int      g_tick_count = 0;

}  // namespace

extern "C" void lostar_install_host(const lostar_host_ops *ops) {
  if (!ops) {
    memset(&g_ops, 0, sizeof(g_ops));
    g_ops_installed = false;
    return;
  }
  memset(&g_ops, 0, sizeof(g_ops));
  const uint32_t copy_bytes = ops->size < sizeof(g_ops) ? ops->size : (uint32_t)sizeof(g_ops);
  memcpy(&g_ops, ops, copy_bytes);
  g_ops.size       = copy_bytes;
  g_ops_installed  = true;
}

extern "C" const lostar_host_ops *lostar_host(void) { return g_ops_installed ? &g_ops : nullptr; }

extern "C" uint32_t lostar_self_nodenum(void) {
  if (!g_ops_installed || !g_ops.self_nodenum) return 0;
  return g_ops.self_nodenum(g_ops.ctx);
}

extern "C" int lostar_self_pubkey(uint8_t out[32]) {
  if (!g_ops_installed || !g_ops.self_pubkey) {
    memset(out, 0, 32);
    return 0;
  }
  return g_ops.self_pubkey(g_ops.ctx, out);
}

extern "C" void lostar_register_tick_hook(lostar_tick_fn fn, void *ctx) {
  if (!fn) return;
  for (int i = 0; i < g_tick_count; i++) {
    if (g_ticks[i].fn == fn && g_ticks[i].ctx == ctx) return;
  }
  if (g_tick_count >= kMaxTickHooks) return;
  g_ticks[g_tick_count].fn  = fn;
  g_ticks[g_tick_count].ctx = ctx;
  g_tick_count++;
}

extern "C" void lostar_tick(void) {
  for (int i = 0; i < g_tick_count; i++) {
    if (g_ticks[i].fn) g_ticks[i].fn(g_ticks[i].ctx);
  }
}
