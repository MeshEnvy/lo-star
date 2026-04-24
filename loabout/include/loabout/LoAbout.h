#pragma once

#include <lomessage/Buffer.h>

namespace loabout {

/** Appends the `about` reply. Called with @p user from @ref set_banner_fn. */
using BannerFn = void (*)(lomessage::Buffer& out, void* user);

/**
 * Set the banner writer for the `about` CLI root. Call from app boot before or after @ref init
 * (last non-null call wins). If @p fn is nullptr, `about` prints a generic one-liner.
 */
void set_banner_fn(BannerFn fn, void* user);

/** Register the `about` engine on `lostar::router()`. Idempotent. */
void init();

}  // namespace loabout
