#pragma once

#include <lostar/Types.h>

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Host-ops vtable. Each fork fills one and passes it to `lostar_install_host`. `size` is the
 * byte size of the struct as seen by the installer; lostar uses it to gate access to new fields
 * so new releases can grow the vtable without breaking old adapters.
 *
 * Only `send_text_dm` is required. The rest are optional — NULL is fine.
 *   - `send_text_dm` — best-effort unsolicited DM. Used for heartbeats / pushed data, NOT for
 *      replies to ingress; replies go through `lostar_deferred_reply`.
 *   - `self_nodenum` — canonical NodeId of this device (`0` if unknown).
 *   - `self_pubkey`  — copy 32 public-key bytes into `out[32]`; returns 1 on success, 0 if unknown.
 */
typedef struct {
  uint32_t size;
  void     (*send_text_dm)(void *ctx, uint32_t to, const char *text, uint32_t len);
  uint32_t (*self_nodenum)(void *ctx);
  int      (*self_pubkey)(void *ctx, uint8_t out[32]);
  void    *ctx;
} lostar_host_ops;

/** Install the host_ops vtable. Pass NULL to uninstall (for tests). Copy is taken by value. */
void lostar_install_host(const lostar_host_ops *ops);

/** Read-only accessor; returns NULL if none installed. The pointer is stable after install. */
const lostar_host_ops *lostar_host(void);

/** Convenience accessors around the installed ops. Safe to call before install (return 0). */
uint32_t lostar_self_nodenum(void);
int      lostar_self_pubkey(uint8_t out[32]);

/**
 * Ingress: the fork adapter hands a decoded text DM to lostar after normalizing into a POD.
 * Returns true if lostar matched an engine root or global help and produced a reply.
 *
 * Fork adapters should call `lostar_ingress_attach_deferrer(...)` before this so the reply —
 * sync or async — can route back through the fork's native reply path.
 */
bool lostar_ingress_text_dm(const lostar_TextDm *dm);

/** Ingress: fork adapter hands a decoded advert to lostar; fanned out to subscribed observers. */
void lostar_ingress_node_advert(const lostar_NodeAdvert *adv);

/** Tick hook registry — apps register periodic work; fork adapter calls `lostar_tick()` once per
 *  host main-loop pass. Fixed-capacity registry; excess registrations are silently dropped. */
typedef void (*lostar_tick_fn)(void *ctx);
void lostar_register_tick_hook(lostar_tick_fn fn, void *ctx);
void lostar_tick(void);

#ifdef __cplusplus
}  // extern "C"
#endif
