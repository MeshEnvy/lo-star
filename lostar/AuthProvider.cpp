#include <lostar/AuthProvider.h>

namespace lostar {

static AuthProvider* g_auth_provider = nullptr;

void setAuthProvider(AuthProvider* provider) {
  g_auth_provider = provider;
}

AuthProvider* getAuthProvider() {
  return g_auth_provider;
}

}  // namespace lostar
