#include <lostar/Router.h>

namespace lostar {

locommand::Router& router() {
  static locommand::Router s;
  return s;
}

}  // namespace lostar
