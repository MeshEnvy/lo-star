#pragma once

#include "Command.h"

namespace locommand {

/** Nested CLI: strict group-or-leaf tree, flat help, root name match. */
class Engine {
public:
  /** @param root_name literal for the root token (e.g. "lotato"); not copied, must outlive Engine. */
  explicit Engine(const char* root_name);
  ~Engine();

  Engine(const Engine&) = delete;
  Engine& operator=(const Engine&) = delete;

  /** One-line summary for Router global help (optional). */
  void setRootBrief(const char* brief) { _root_brief = brief; }
  const char* rootBrief() const { return _root_brief; }

  /**
   * When true, this root appears on signed-out global `help` (compact list, no `CLI roots:` header).
   * Default false; enable for public entry roots (e.g. `about`, `hi`).
   */
  void setGuestTopHelp(bool on) { _guest_top_help = on; }
  bool guestTopHelp() const { return _guest_top_help; }

  /** Register a dotted path under the root (e.g. "wifi.scan"). Creates group segments as needed.
   *  Enforces group-or-leaf invariant. Returns the new leaf or nullptr on conflict / OOM. */
  Command* add(const char* path, Handler handler, const char* usage_suffix = nullptr,
               const char* hint = nullptr, const char* brief = nullptr);

  /** Same as add but builds usage from @p specs and enables structured argument help. */
  Command* addWithArgs(const char* path, Handler handler, const ArgSpec* specs, int n_specs,
                       const char* hint = nullptr, const char* brief = nullptr, const char* details = nullptr);

  /**
   * If set, the whole engine (root) is hidden from global help and dispatch/help when the guard
   * fails for @p app_ctx (e.g. `require_admin` for lotato/config/wifi).
   */
  void setRootGuard(Guard g) { _root_guard = g; }

  /** If set, a bare root (`about` with no subcommand) runs this instead of listing engine help. */
  void setBareHandler(Handler h) { _bare_handler = h; }

  /**
   * If set, any non-empty input after the root (that is not `help` / `?` / `help …`) is tokenized
   * as argv and passed to @p h. Use for roots like `hi <user> <pw>` with no subcommand name.
   * @p arg_usage_suffix usage after the root name, e.g. `"<username> <password>"` (shown in help).
   */
  void setRestHandler(Handler h, const char* arg_usage_suffix) {
    _remainder_handler = h;
    _rest_arg_usage    = arg_usage_suffix;
  }

  /**
   * Attach a `Guard` to the command at @p dotted_path (e.g. "wifi.scan"). When the guard returns
   * false for the caller, dispatch does not run the handler; the command is omitted from help.
   */
  bool setGuardFor(const char* dotted_path, Guard guard);

  /** Same as setGuardFor but accepts a pre-resolved Command pointer (from add/addWithArgs). */
  static void setGuard(Command* cmd, Guard guard);

  /** True if @p cmd starts with @ref rootName followed by '\0', whitespace, or '?'. */
  bool matchesRoot(const char* cmd) const;

  /** Dispatch text after the root token (e.g. " wifi status"). Bare / help / ? => full help. */
  void dispatch(const char* input_after_root, lomessage::Buffer& out, void* app_ctx);

  /** One help line per leaf: "<root> <path> [usage]  (hint) - <brief>". @p sub_path is
   *  space-separated segments under root or nullptr for all commands. */
  void formatHelp(lomessage::Buffer& out, const char* sub_path = nullptr, void* app_ctx = nullptr) const;

  /** True if any descendant is visible to @p app_ctx. Guard-less engines are always visible. */
  bool anyVisible(void* app_ctx) const;

  const char* rootName() const { return _root.name; }

private:
  Command _root;
  const char* _root_brief = nullptr;
  bool _guest_top_help = false;
  Guard _root_guard = nullptr;
  Handler _bare_handler = nullptr;
  Handler _remainder_handler = nullptr;
  const char* _rest_arg_usage = nullptr;
};

}  // namespace locommand
