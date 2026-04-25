#include <loabout/LoAbout.h>

#include <locommand/Engine.h>
#include <lostar/Router.h>

namespace loabout {

namespace {

BannerFn s_banner_fn = nullptr;
void*      s_banner_user = nullptr;

void emit_banner(lomessage::Buffer& out) {
  if (s_banner_fn) {
    s_banner_fn(out, s_banner_user);
    return;
  }
  out.append("Mesh CLI node.\n");
}

void h_about(locommand::Context& ctx) { emit_banner(ctx.out); }

}  // namespace

void set_banner_fn(BannerFn fn, void* user) {
  s_banner_fn  = fn;
  s_banner_user = user;
}

void append_banner(lomessage::Buffer& out) { emit_banner(out); }

void init() {
  static bool              done = false;
  static locommand::Engine eng{"about"};
  if (done) return;
  done = true;

  eng.setBareHandler(&h_about);
  eng.setRootBrief("about Lotato");
  eng.setGuestTopHelp(true);
  lostar::router().add(&eng);
}

}  // namespace loabout
