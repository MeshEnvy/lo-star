#pragma once

#include <initializer_list>
#include <lofs/LoFS.h>

namespace LoStar {

/**
 * Bring up lo-star infrastructure:
 * - apply requested filesystem mounts, then mount LoFS defaults
 * - register/load LoLog config
 * - optional self-tests gated by compile flags
 */
void boot(std::initializer_list<LoFS::FSysBinding> mounts = {});

}  // namespace LoStar
