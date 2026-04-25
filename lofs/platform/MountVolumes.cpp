#include <lofs/PlatformMount.h>

#include <lofs/LoFS.h>
#include <lofs/capabilities.h>

#include "../backends/ExternalFlashBackend.h"
#include "../backends/InternalFlashBackend.h"
#include "../backends/RamBackend.h"
#include "../backends/SdBackend.h"

#if __has_include(<lolog/LoLog.h>)
#include <lolog/LoLog.h>
#define LOFS_LOG_DEBUG(...) ::lolog::LoLog::debug("lofs", __VA_ARGS__)
#else
#define LOFS_LOG_DEBUG(...) ((void)0)
#endif

namespace lofs {

bool mount_platform_volumes() {
  auto& inb = InternalFlashBackend::instance();
  auto& exb = ExternalFlashBackend::instance();
  auto& sdb = SdBackend::instance();
  auto& ram = RamBackend::instance();

#if LOFS_CAP_SD
  if (!sdb.available()) {
    LOFS_LOG_DEBUG("mount_platform_volumes: LOFS_CAP_SD but no SD card");
    return false;
  }
#endif

  if (inb.available()) (void)LoFS::mount("/__int__", &inb);

  if (inb.available()) {
#if LOFS_CAP_EXT
    if (exb.hasDedicatedVolume()) {
      (void)LoFS::mount("/__ext__", &exb);
    } else {
      (void)LoFS::mount("/__ext__", &inb);
    }
#else
    (void)LoFS::mount("/__ext__", &inb);
#endif
  }

#if LOFS_CAP_SD
  (void)LoFS::mount("/__sd__", &sdb);
#endif

  if (ram.available()) (void)LoFS::mount("/__ram__", &ram);
  return true;
}

}  // namespace lofs
