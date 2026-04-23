#include <louser/Auth.h>

namespace louser {

Auth::Auth() : _users("/__ext__"), _sessions("/__ext__") {}

Auth& Auth::instance() {
  static Auth s;
  return s;
}

bool Auth::currentUser(const lostar::NodeRef& caller, User& out) {
  uint64_t uid = _sessions.whoami(caller);
  if (!uid) return false;
  return _users.findById(uid, out);
}

bool Auth::isAdmin(const lostar::NodeRef& caller) {
  User u;
  if (!currentUser(caller, u)) return false;
  return u.admin;
}

}  // namespace louser
