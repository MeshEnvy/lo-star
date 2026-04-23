#pragma once

#include <cstdint>

#include <lodb/LoDB.h>
#include <lostar/NodeId.h>

namespace louser {

/** Runtime view of a stored LoUserSession row. */
struct Session {
  lostar::NodeId node_id = 0;
  uint64_t       user_id = 0;
  uint8_t        bind_fp[LOSTAR_NODEREF_CTX_CAP] = {};
  uint16_t       bind_fp_len = 0;
  uint32_t       created_ms = 0;
};

/**
 * Sessions store: one row per logged-in node. Sessions are pure soft state — wiping LoDB on
 * a dev device is an acceptable outcome.
 */
class Sessions {
public:
  explicit Sessions(const char* mount = "/__ext__");

  /** Create or replace the session for @p caller → @p user_id. Captures caller's `ctx` as bind_fp. */
  bool login(const lostar::NodeRef& caller, uint64_t user_id);

  /** Drop the session for @p node_id. No-op if absent. */
  bool logout(lostar::NodeId node_id);

  /**
   * Return the logged-in user_id for @p caller, honoring `lotato::platform::bind_check`. Returns
   * 0 when no session, or when a bind check fails (caller appears signed-out).
   */
  uint64_t whoami(const lostar::NodeRef& caller);

  /** Drop every session. */
  bool clearAll();

  /** Drop every session belonging to @p user_id. */
  bool dropForUser(uint64_t user_id);

private:
  LoDb _db;
  bool _registered = false;
  void ensure_registered();
};

}  // namespace louser
