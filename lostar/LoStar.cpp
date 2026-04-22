#include <lostar/LoStar.h>

#include <lodb/LoDB.h>
#include <lolog/LoLog.h>
#include <lofs/LoFS.h>
#include <losettings/LoSettings.h>

#ifndef LODB_TEST_MOUNT
#define LODB_TEST_MOUNT "/__ext__"
#endif

#ifndef LOSETTINGS_TEST_MOUNT
#define LOSETTINGS_TEST_MOUNT "/__ext__"
#endif

namespace LoStar {

void boot(std::initializer_list<LoFS::FSysBinding> mounts) {
  if (mounts.size() > 0) {
    (void)LoFS::mount(mounts);
  }
  LoFS::mountDefaults();
  ::lolog::LoLog::registerConfigSchema();
  ::lolog::LoLog::loadFromSettings();

#if defined(LODB_TEST)
  (void)lodb_run_selftest(LODB_TEST_MOUNT);
#endif
#if defined(LOSETTINGS_TEST)
  (void)losettings::losettings_run_selftest(LOSETTINGS_TEST_MOUNT);
#endif
}

}  // namespace LoStar
