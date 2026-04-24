#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Deferred reply — the platform-independent "reply goes back to whoever issued this command"
 * abstraction. The fork adapter attaches a deferrer before calling `lostar_ingress_text_dm`;
 * lo-star / apps capture or fire it without ever looking inside `route_ctx`.
 *
 * - Sync reply: CLI handler calls `lostar_fire_deferred_reply(...)` directly, or lostar fires
 *   the attached deferrer with whatever the dispatch emitted. The fork may free `route_ctx`
 *   after the ingress call if nothing captured it.
 * - Async reply: CLI handler calls `lostar_capture_deferred_reply()` to take a copy, parks the
 *   copy in an app-owned pending slot, and fires it later when the async operation completes.
 *   In that case the fork's pool-allocated `route_ctx` must stay valid until fire is called.
 */
typedef struct {
  void (*fire)(void *route_ctx, const char *text, uint32_t len);
  void *route_ctx;
} lostar_deferred_reply;

/** Fork-adapter facing: attach the deferrer for the current ingress. Single-threaded; pass NULL
 *  to clear. lostar copies the pointer target by value when a consumer calls capture. */
void lostar_ingress_attach_deferrer(const lostar_deferred_reply *d);

/** Engine-facing: snapshot the current deferrer by value. Returns a null-stub (fire == NULL)
 *  when none is attached. Callers use the snapshot for both immediate and delayed replies. */
lostar_deferred_reply lostar_capture_deferred_reply(void);

/** Engine-facing: send text via a deferred handle. No-op if @p d or @p d->fire is NULL. */
void lostar_fire_deferred_reply(const lostar_deferred_reply *d, const char *text, uint32_t len);

/**
 * Per-ingress opaque token for fork-coupled apps (e.g. meshcore's chunked-reply route cache).
 * lostar never dereferences this; it just stores and returns the pointer. Cleared automatically
 * when the ingress call completes.
 */
void        lostar_ingress_set_host_token(const void *tok);
const void *lostar_ingress_host_token(void);

#ifdef __cplusplus
}  // extern "C"
#endif
