#include <loabout/LoAbout.h>

#include <locommand/Engine.h>
#include <lostar/Router.h>

namespace loabout {

namespace {

BannerFn s_banner_fn = nullptr;
void*      s_banner_user = nullptr;

void h_about(locommand::Context& ctx) {
  if (s_banner_fn) {
    s_banner_fn(ctx.out, s_banner_user);
    return;
  }
  ctx.out.append("Mesh CLI node.\n");
}

}  // namespace

void set_banner_fn(BannerFn fn, void* user) {
  s_banner_fn  = fn;
  s_banner_user = user;
}

void init() {
  static bool              done = false;
  static locommand::Engine eng{"about"};
  if (done) return;
  done = true;

  eng.setBareHandler(&h_about);
  eng.setRootBrief("device info");
  lostar::router().add(&eng);
}

}  // namespace loabout
