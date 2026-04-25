#pragma once

namespace lofs {

/**
 * Platform adapter entry: mount `/__int__`, `/__ext__`, `/__sd__`, `/__ram__` on the active target.
 * Uses **`LOFS_CAP_EXT`** / **`LOFS_CAP_SD`** from `capabilities.h` and
 * backend state (`InternalFlashBackend`, `ExternalFlashBackend`, `SdBackend`, …). Call after any
 * `LoFS::mount` of host `FsVolume*` bindings (e.g. `/__int__` override) and before `LoFS::open`.
 *
 * Lostar does not implement mount policy — this TU does.
 */
bool mount_platform_volumes();

}  // namespace lofs
