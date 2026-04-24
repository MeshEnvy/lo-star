#include <lostar/Busy.h>

namespace {

constexpr int kMaxHints = 8;

struct HintSlot {
  lostar_busy_fn fn;
  void          *ctx;
};

HintSlot g_hints[kMaxHints];
int      g_count = 0;

}  // namespace

extern "C" void lostar_register_busy_hint(lostar_busy_fn fn, void *ctx) {
  if (!fn) return;
  for (int i = 0; i < g_count; i++) {
    if (g_hints[i].fn == fn && g_hints[i].ctx == ctx) return;
  }
  if (g_count >= kMaxHints) return;
  g_hints[g_count].fn  = fn;
  g_hints[g_count].ctx = ctx;
  g_count++;
}

extern "C" void lostar_unregister_busy_hint(lostar_busy_fn fn, void *ctx) {
  for (int i = 0; i < g_count; i++) {
    if (g_hints[i].fn == fn && g_hints[i].ctx == ctx) {
      for (int j = i; j < g_count - 1; j++) g_hints[j] = g_hints[j + 1];
      g_count--;
      g_hints[g_count].fn  = nullptr;
      g_hints[g_count].ctx = nullptr;
      return;
    }
  }
}

extern "C" bool lostar_is_busy(void) {
  for (int i = 0; i < g_count; i++) {
    if (g_hints[i].fn && g_hints[i].fn(g_hints[i].ctx)) return true;
  }
  return false;
}
