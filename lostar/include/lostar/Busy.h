#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Busy-hinter registry. Apps / modules that do async work register a predicate that returns
 * true while they want the host to stay awake. `lostar_is_busy()` ORs every registered hinter.
 *
 * Fork adapters poll `lostar_is_busy()` to decide sleep / wake policy — this lets lo-star stay
 * out of host-specific power management while still keeping the host awake during a pending
 * wifi scan, a draining ingest queue, etc.
 *
 * Fixed-capacity registry; duplicate registrations of the same (fn, ctx) pair are deduplicated.
 */

typedef bool (*lostar_busy_fn)(void *ctx);

void lostar_register_busy_hint(lostar_busy_fn fn, void *ctx);
void lostar_unregister_busy_hint(lostar_busy_fn fn, void *ctx);
bool lostar_is_busy(void);

#ifdef __cplusplus
}  // extern "C"
#endif
