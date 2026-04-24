#pragma once

namespace louser {

/**
 * One-shot bringup: allocate the singleton `user` engine, register its handlers, attach the
 * default guards (`require_user` on logout/whoami/bye, `require_admin` on admin subcommands),
 * and register the engine with `lostar::router()`. Fork adapters call this after
 * `lotato::init()`. Idempotent.
 *
 * Fork-specific guard policy on other engines (e.g. `lotato.pause` requires admin) is applied
 * separately by the fork adapter after init.
 */
void init();

}  // namespace louser
