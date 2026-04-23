#pragma once

#include <cstddef>

#include <locommand/Engine.h>

#include <lostar/NodeId.h>

namespace louser {

/**
 * Build the `user` CLI engine: register/login/whoami/logout + admin subcommands, plus the
 * aliases (`user hi`, `user bye`). Top-level aliases (`hi`, `bye`, `whoami`) are handled by
 * `rewrite_alias` — callers apply the rewrite before handing the line to `locommand::Router`.
 *
 * The caller's `NodeRef` must be stashed into `locommand::Context::app_ctx` by the dispatcher
 * before each call; `caller_node_ref(ctx)` unpacks it.
 */
void register_engine(locommand::Engine& user_eng);

/**
 * If @p in is one of `hi <user> <pw>`, `bye`, or `whoami`, write the equivalent `user ...`
 * form into @p out (NUL-terminated) and return true. Otherwise returns false and leaves
 * @p out untouched.
 */
bool rewrite_alias(const char* in, char* out, size_t out_cap);

/** Extract the caller `NodeRef` from a locommand Context (nullptr if not set). */
const lostar::NodeRef* caller_node_ref(const locommand::Context& ctx);

}  // namespace louser
