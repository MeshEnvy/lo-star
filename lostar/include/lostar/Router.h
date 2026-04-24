#pragma once

#include <locommand/Router.h>

namespace lostar {

/**
 * Shared CLI router owned by lostar. All lo-star apps/modules register their engines here so the
 * fork adapter can dispatch any text DM through a single entry point (`lostar_ingress_text_dm`).
 *
 * Ownership: engines are non-owning; keep them in static storage.
 */
locommand::Router& router();

}  // namespace lostar
