#pragma once

#include <lostar/NodeId.h>

namespace lostar {

/**
 * Pluggable authentication provider. Evaluates whether a given caller is a known user
 * or an admin, decoupling CLI guard policies from specific database/session implementations.
 */
class AuthProvider {
public:
  virtual ~AuthProvider() = default;

  /** Return true if the caller has a valid, signed-in session. */
  virtual bool isUser(const NodeRef& caller) = 0;

  /** Return true if the caller has a valid session AND is an administrator. */
  virtual bool isAdmin(const NodeRef& caller) = 0;

  /** If signed in, copies the display name into name_out and returns true. */
  virtual bool getCurrentUser(const NodeRef& caller, char* name_out, size_t name_cap) {
    if (name_out && name_cap > 0) name_out[0] = '\0';
    return isUser(caller); 
  }
};

/** Set the global AuthProvider used by CLI guards. */
void setAuthProvider(AuthProvider* provider);

/** Get the currently registered AuthProvider. */
AuthProvider* getAuthProvider();

}  // namespace lostar
