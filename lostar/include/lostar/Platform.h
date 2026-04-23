#pragma once

#include <cstddef>
#include <cstdint>

#include <lostar/NodeId.h>

/**
 * Platform hooks shared across LoStar consumers.
 *
 * `lotato::platform::bind_check` lets louser verify that the peer sending a CLI request is the
 * same cryptographic identity that established the session. Each platform interprets the
 * opaque `NodeRef.ctx` buffer however it likes; meshcore stashes the sender pub_key and
 * compares it, meshtastic just returns true because `NodeId` already equals `node_num`.
 *
 * The declaration lives in lo-star so `louser` can call it without depending on lotato `src/`.
 * Each platform provides the definition in its own `IngestPlatform.h` / `MeshtasticPlatform.h`
 * (tagged `inline` so consumers pull in exactly one copy per translation unit).
 *
 * `send_chunk` and `fill_node_ref` are declared next to their native callers (lotato `src/`)
 * rather than here, because they accept platform-native identity arguments.
 */

namespace lotato {
namespace platform {

/**
 * True if @p incoming represents the same peer as the session previously bound to @p stored.
 * Default implementation (no platform header pulled in) returns true, which keeps `louser` usable
 * on host test builds that don't have a mesh transport.
 */
bool bind_check(const lostar::NodeRef& stored, const lostar::NodeRef& incoming);

}  // namespace platform
}  // namespace lotato
