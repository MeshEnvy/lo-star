#pragma once

#include <stddef.h>

namespace louser {

/**
 * One-shot bringup: register `hi` (signed-out only), `bye` / `whoami` (signed-in only), and the
 * `user` engine (admin subcommands; sign-out via `bye`, sign-up/sign-in via `hi`) on `lostar::router()`. Fork adapters
 * call this after `lotato::init()` (which registers `about` via `loabout`). Idempotent.
 *
 * Fork-specific guard policy on other engines (e.g. `lotato.pause` requires admin) is applied
 * separately by the fork adapter after init.
 */
void init();

}  // namespace louser
