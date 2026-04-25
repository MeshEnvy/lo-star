#pragma once

/**
 * LoFS mount policy — set **`LOFS_CAP_EXT`** / **`LOFS_CAP_SD`** explicitly per image (defaults `0`).
 *
 * - **`LOFS_CAP_EXT`:** When `1`, `/__ext__` uses dedicated external flash if
 *   `ExternalFlashBackend::hasDedicatedVolume()`; otherwise **internal** on `/__ext__`.
 *   When `0`, `/__ext__` is always **internal** (paths stay valid).
 * - **`LOFS_CAP_SD`:** When `1`, `/__sd__` is mounted and **`mount_platform_volumes()` fails** if
 *   no working card. When `0`, **`/__sd__` is not mounted**.
 */

#ifndef LOFS_CAP_EXT
#define LOFS_CAP_EXT 0
#endif

#ifndef LOFS_CAP_SD
#define LOFS_CAP_SD 0
#endif
