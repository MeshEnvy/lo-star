#pragma once

#include <lostar/Types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Advert observer registry. Apps that care about node adverts register a callback; lostar fans
 * incoming `lostar_NodeAdvert` PODs out to every registered observer in registration order.
 *
 * Callbacks run on the host's main loop context (inside `lostar_ingress_node_advert`).
 */

typedef void (*lostar_advert_observer_fn)(const lostar_NodeAdvert *adv, void *ctx);

void lostar_register_advert_observer(lostar_advert_observer_fn fn, void *ctx);
void lostar_unregister_advert_observer(lostar_advert_observer_fn fn, void *ctx);

#ifdef __cplusplus
}  // extern "C"
#endif
