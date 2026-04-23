#pragma once

#include <cstdint>

#include <lostar/NodeId.h>

#include "User.h"
#include "Session.h"

namespace louser {

/** Thin coordinator over Users + Sessions. Instance-free facade; uses shared singletons. */
class Auth {
public:
  /** Singleton. First call binds to the default LoFS mount (`/__ext__`). */
  static Auth& instance();

  /** Resolve the caller's user. Returns true on hit and fills @p out. */
  bool currentUser(const lostar::NodeRef& caller, User& out);

  /** Convenience: true if caller has a valid session whose user is admin. */
  bool isAdmin(const lostar::NodeRef& caller);

  Users&    users()    { return _users; }
  Sessions& sessions() { return _sessions; }

private:
  Auth();
  Users    _users;
  Sessions _sessions;
};

}  // namespace louser
