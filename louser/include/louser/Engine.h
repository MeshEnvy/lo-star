#pragma once

#include <cstddef>

#include <locommand/Engine.h>

#include <lostar/NodeId.h>

namespace louser {

/**
 * Build the `user` CLI engine: admin subcommands (sign-out is root `bye`; sign-up/sign-in is root `hi`).
 *
 * The caller's `NodeRef` must be stashed into `locommand::Context::app_ctx` by the dispatcher
 * before each call; `caller_node_ref(ctx)` unpacks it.
 */
void register_engine(locommand::Engine& user_eng);

/** Extract the caller `NodeRef` from a locommand Context (nullptr if not set). */
const lostar::NodeRef* caller_node_ref(const locommand::Context& ctx);

}  // namespace louser
