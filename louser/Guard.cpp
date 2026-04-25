#include <louser/Guard.h>
#include <louser/Auth.h>

#include <lostar/NodeId.h>

namespace louser {

namespace {

const lostar::NodeRef* node_ref(void* app_ctx) {
  return static_cast<const lostar::NodeRef*>(app_ctx);
}

}  // namespace

bool require_user(void* app_ctx) {
  const lostar::NodeRef* nr = node_ref(app_ctx);
  if (!nr) return false;
  return Auth::instance().sessions().whoami(*nr) != 0;
}

bool require_logged_out(void* app_ctx) {
  const lostar::NodeRef* nr = node_ref(app_ctx);
  if (!nr) return false;
  return Auth::instance().sessions().whoami(*nr) == 0;
}

bool require_admin(void* app_ctx) {
  const lostar::NodeRef* nr = node_ref(app_ctx);
  if (!nr) return false;
  return Auth::instance().isAdmin(*nr);
}

}  // namespace louser
