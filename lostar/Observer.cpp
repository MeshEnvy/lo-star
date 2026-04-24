#include <lostar/AdvertObserver.h>
#include <lostar/Host.h>

namespace {

constexpr int kMaxObservers = 8;

struct ObsSlot {
  lostar_advert_observer_fn fn;
  void                     *ctx;
};

ObsSlot g_obs[kMaxObservers];
int     g_count = 0;

}  // namespace

extern "C" void lostar_register_advert_observer(lostar_advert_observer_fn fn, void *ctx) {
  if (!fn) return;
  for (int i = 0; i < g_count; i++) {
    if (g_obs[i].fn == fn && g_obs[i].ctx == ctx) return;
  }
  if (g_count >= kMaxObservers) return;
  g_obs[g_count].fn  = fn;
  g_obs[g_count].ctx = ctx;
  g_count++;
}

extern "C" void lostar_unregister_advert_observer(lostar_advert_observer_fn fn, void *ctx) {
  for (int i = 0; i < g_count; i++) {
    if (g_obs[i].fn == fn && g_obs[i].ctx == ctx) {
      for (int j = i; j < g_count - 1; j++) g_obs[j] = g_obs[j + 1];
      g_count--;
      g_obs[g_count].fn  = nullptr;
      g_obs[g_count].ctx = nullptr;
      return;
    }
  }
}

extern "C" void lostar_ingress_node_advert(const lostar_NodeAdvert *adv) {
  if (!adv) return;
  for (int i = 0; i < g_count; i++) {
    if (g_obs[i].fn) g_obs[i].fn(adv, g_obs[i].ctx);
  }
}
