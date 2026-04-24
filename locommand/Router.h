#pragma once

#include <lomessage/Buffer.h>

namespace locommand {

class Engine;

/** Dispatches among multiple CLI roots (e.g. lotato, wifi, config). Non-owning engines. */
class Router {
public:
  static constexpr int kMaxEngines = 8;

  Router();

  void add(Engine* e);
  void clear();

  /** Look up a registered engine by its root name (e.g. "wifi", "lotato"). Returns nullptr if
   *  no engine with that root is registered. Used by fork adapters to attach guard policy. */
  Engine* engineByName(const char* root_name);

  /** True if @p cmd matches any registered engine root (leading spaces skipped). */
  bool matchesAnyRoot(const char* cmd) const;

  /** True for bare `help` / `?` / `help …` (same cases `dispatch` handles before root match). */
  bool matchesGlobalHelp(const char* cmd) const;

  /**
   * Dispatch a full command line (including root token). Returns false if no engine matched
   * (caller may fall through to legacy CLI).
   */
  bool dispatch(const char* command, lomessage::Buffer& out, void* app_ctx);

  /** One line per root (brief) plus each engine's flat help. Engines with no visible commands
   *  under @p app_ctx are hidden entirely (root line + sub-help both suppressed). */
  void formatGlobalHelp(lomessage::Buffer& out, void* app_ctx = nullptr) const;

private:
  Engine* _engines[kMaxEngines];
  int _n;
};

}  // namespace locommand
