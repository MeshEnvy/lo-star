#pragma once

namespace louser {

/**
 * `locommand::Guard`-compatible predicates over the caller's identity. `app_ctx` is the same
 * opaque pointer the engine dispatcher hands to handlers — `LotatoCli` sets it to a
 * `const lostar::NodeRef*`.
 */

/** True if the caller has any active session (= logged in). */
bool require_user(void* app_ctx);

/** True if the caller is a logged-in admin. */
bool require_admin(void* app_ctx);

}  // namespace louser
