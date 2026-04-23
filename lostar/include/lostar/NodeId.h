#pragma once

#include <cstddef>
#include <cstdint>

/**
 * lostar::NodeId / NodeRef
 *
 * A single 32-bit canonical node identity shared by every LoStar consumer (lotato, louser,
 * platform delegates). The canonical string form is `!xxxxxxxx` (8 lowercase hex chars),
 * matching the Potato Mesh web contract (see `potato-mesh/data/mesh_ingestor/node_identity.py`
 * and `CONTRACTS.md`). Meshtastic's `node_num` maps 1:1; meshcore distils the first four
 * bytes of its 32-byte pub_key.
 *
 * NodeRef carries the id plus an opaque, platform-defined `ctx` buffer. LoStar itself never
 * looks inside `ctx`; interpretation belongs to the delegate via `lotato::platform::*` hooks
 * (bind_check, send_chunk). This lets meshcore carry pub_key+secret+out_path for crypto /
 * routing while meshtastic carries node_num+channel, without either leaking into shared code.
 */

#ifndef LOSTAR_NODEREF_CTX_CAP
#define LOSTAR_NODEREF_CTX_CAP 96
#endif

namespace lostar {

/** 32-bit canonical node identity. Matches `!xxxxxxxx` web form exactly (no translation). */
using NodeId = uint32_t;

/** Opaque platform-defined identity/routing context companion to NodeId. */
struct NodeRef {
  NodeId   id = 0;
  uint16_t ctx_len = 0;
  uint8_t  ctx[LOSTAR_NODEREF_CTX_CAP] = {};
};

/** Canonical hex form: writes `"!%08x"` + NUL into @p out (requires cap >= 10). */
void format_canonical(NodeId id, char* out, size_t out_cap);

/**
 * Parse a node identity in any of the forms the web contract accepts: `!xxxxxxxx`, `0xHEX`,
 * bare decimal, or bare hex. On success writes to @p out and returns true; otherwise false.
 */
bool parse_canonical(const char* in, NodeId* out);

}  // namespace lostar
