#include <louser/Engine.h>
#include <louser/Auth.h>
#include <louser/Guard.h>
#include <louser/LoUser.h>
#include <louser/User.h>

#include <lolog/LoLog.h>
#include <lostar/Router.h>

#include <cctype>
#include <cstdio>
#include <cstring>

namespace louser {

namespace {

const lostar::NodeRef* node_ref_from_ctx(const locommand::Context& ctx) {
  return static_cast<const lostar::NodeRef*>(ctx.app_ctx);
}

/* ── user register <name> <pw> ─────────────────────────────────────── */
void h_register(locommand::Context& ctx) {
  if (ctx.argc < 2) {
    ctx.printHelp();
    return;
  }
  User u;
  int rc = Auth::instance().users().create(ctx.argv[0], ctx.argv[1], &u);
  if (rc == 1) { ctx.out.append("Err - username already exists"); return; }
  if (rc == 2) { ctx.out.append("Err - invalid username or password"); return; }
  if (rc == 3) { ctx.out.append("Err - storage error"); return; }
  ctx.out.appendf("OK - user %s created%s", u.username, u.admin ? " (admin)" : "");
}

/* ── user login <name> <pw> ────────────────────────────────────────── */
void do_login(locommand::Context& ctx, const User& u) {
  const lostar::NodeRef* nr = node_ref_from_ctx(ctx);
  if (!nr) { ctx.out.append("Err - no caller identity"); return; }
  if (!Auth::instance().sessions().login(*nr, u.id)) {
    ctx.out.append("Err - session storage error");
    return;
  }
  ctx.out.appendf("OK - welcome, %s%s", u.username, u.admin ? " (admin)" : "");
}

void h_login(locommand::Context& ctx) {
  if (ctx.argc < 2) { ctx.printHelp(); return; }
  User u;
  if (!Auth::instance().users().findByUsername(ctx.argv[0], u) || !Users::verifyPassword(u, ctx.argv[1])) {
    ctx.out.append("Err - unknown user or wrong password");
    return;
  }
  do_login(ctx, u);
}

/* ── user whoami ───────────────────────────────────────────────────── */
void h_whoami(locommand::Context& ctx) {
  const lostar::NodeRef* nr = node_ref_from_ctx(ctx);
  User u;
  if (!nr || !Auth::instance().currentUser(*nr, u)) {
    ctx.out.append("(not signed in)");
    return;
  }
  ctx.out.appendf("%s%s", u.username, u.admin ? " (admin)" : "");
}

/* ── user logout ───────────────────────────────────────────────────── */
void h_logout(locommand::Context& ctx) {
  const lostar::NodeRef* nr = node_ref_from_ctx(ctx);
  if (!nr) { ctx.out.append("(not signed in)"); return; }
  Auth::instance().sessions().logout(nr->id);
  ctx.out.append("OK - logged out");
}

/* ── user hi <name> <pw> — register if missing, else login ─────────── */
void h_hi(locommand::Context& ctx) {
  if (ctx.argc < 2) { ctx.printHelp(); return; }
  User u;
  if (Auth::instance().users().findByUsername(ctx.argv[0], u)) {
    if (!Users::verifyPassword(u, ctx.argv[1])) {
      ctx.out.append("Err - wrong password");
      return;
    }
    do_login(ctx, u);
    return;
  }
  int rc = Auth::instance().users().create(ctx.argv[0], ctx.argv[1], &u);
  if (rc == 2) { ctx.out.append("Err - invalid username or password"); return; }
  if (rc == 3) { ctx.out.append("Err - storage error"); return; }
  if (rc != 0) { ctx.out.append("Err - register failed"); return; }
  do_login(ctx, u);
}

void h_bye(locommand::Context& ctx) { h_logout(ctx); }

/* ── admin commands ────────────────────────────────────────────────── */
void h_sessions_clear(locommand::Context& ctx) {
  Auth::instance().sessions().clearAll();
  ctx.out.append("OK - all sessions cleared");
}

void h_delete(locommand::Context& ctx) {
  if (ctx.argc < 1) { ctx.printHelp(); return; }
  User target;
  if (!Auth::instance().users().findByUsername(ctx.argv[0], target)) {
    ctx.out.append("Err - user not found");
    return;
  }
  Auth::instance().sessions().dropForUser(target.id);
  if (!Auth::instance().users().deleteUser(ctx.argv[0])) {
    ctx.out.append("Err - delete failed");
    return;
  }
  ctx.out.appendf("OK - user %s deleted", ctx.argv[0]);
}

void h_rename(locommand::Context& ctx) {
  if (ctx.argc < 2) { ctx.printHelp(); return; }
  int rc = Auth::instance().users().rename(ctx.argv[0], ctx.argv[1]);
  switch (rc) {
    case 0: ctx.out.appendf("OK - renamed %s -> %s", ctx.argv[0], ctx.argv[1]); return;
    case 1: ctx.out.append("Err - source user not found"); return;
    case 2: ctx.out.append("Err - destination username already exists"); return;
    case 3: ctx.out.append("Err - invalid username"); return;
    default: ctx.out.append("Err - storage error"); return;
  }
}

void h_passwd(locommand::Context& ctx) {
  if (ctx.argc < 2) { ctx.printHelp(); return; }
  User target;
  if (!Auth::instance().users().findByUsername(ctx.argv[0], target)) {
    ctx.out.append("Err - user not found");
    return;
  }
  if (!Auth::instance().users().setPassword(ctx.argv[0], ctx.argv[1])) {
    ctx.out.append("Err - password update failed");
    return;
  }
  Auth::instance().sessions().dropForUser(target.id);
  ctx.out.appendf("OK - password reset for %s (session invalidated)", ctx.argv[0]);
}

const locommand::ArgSpec k_userpw_args[] = {
    {"username", "string", nullptr, true, "Account username"},
    {"password", "secret", nullptr, true, "Account password"},
};
const locommand::ArgSpec k_user_args[] = {
    {"username", "string", nullptr, true, "Account username"},
};
const locommand::ArgSpec k_rename_args[] = {
    {"old_username", "string", nullptr, true, "Existing username"},
    {"new_username", "string", nullptr, true, "New username"},
};
const locommand::ArgSpec k_passwd_args[] = {
    {"username", "string", nullptr, true, "Target username"},
    {"new_password", "secret", nullptr, true, "New password"},
};

}  // namespace

const lostar::NodeRef* caller_node_ref(const locommand::Context& ctx) { return node_ref_from_ctx(ctx); }

void register_engine(locommand::Engine& user_eng) {
  user_eng.addWithArgs("register", &h_register, k_userpw_args, 2, nullptr, "create account (first is admin)");
  user_eng.addWithArgs("login", &h_login, k_userpw_args, 2, nullptr, "log in");
  user_eng.add("whoami", &h_whoami, nullptr, nullptr, "show current user");
  user_eng.add("logout", &h_logout, nullptr, nullptr, "clear this node's session");
  user_eng.addWithArgs("hi", &h_hi, k_userpw_args, 2, nullptr, "register if new, else login");
  user_eng.add("bye", &h_bye, nullptr, nullptr, "alias for logout");
  user_eng.add("sessions.clear", &h_sessions_clear, nullptr, nullptr, "[admin] drop all sessions");
  user_eng.addWithArgs("delete", &h_delete, k_user_args, 1, nullptr, "[admin] delete account");
  user_eng.addWithArgs("rename", &h_rename, k_rename_args, 2, nullptr, "[admin] rename account");
  user_eng.addWithArgs("passwd", &h_passwd, k_passwd_args, 2, nullptr, "[admin] reset password");
  user_eng.setRootBrief("user accounts & sessions");
}

namespace {

const char* skip_ws(const char* p) {
  while (*p == ' ' || *p == '\t') p++;
  return p;
}

bool match_token(const char* in, const char* tok, const char** tail_out) {
  size_t n = strlen(tok);
  if (strncmp(in, tok, n) != 0) return false;
  char after = in[n];
  if (after != '\0' && after != ' ' && after != '\t' && after != '\r' && after != '\n') return false;
  *tail_out = in + n;
  return true;
}

}  // namespace

bool rewrite_alias(const char* in, char* out, size_t out_cap) {
  if (!in || !out || out_cap < 8) return false;
  const char* p = skip_ws(in);
  const char* tail = nullptr;

  if (match_token(p, "hi", &tail)) {
    tail = skip_ws(tail);
    int n = snprintf(out, out_cap, "user hi%s%s", *tail ? " " : "", tail);
    return n > 0 && (size_t)n < out_cap;
  }
  if (match_token(p, "bye", &tail)) {
    tail = skip_ws(tail);
    int n = snprintf(out, out_cap, "user bye%s%s", *tail ? " " : "", tail);
    return n > 0 && (size_t)n < out_cap;
  }
  if (match_token(p, "whoami", &tail)) {
    tail = skip_ws(tail);
    int n = snprintf(out, out_cap, "user whoami%s%s", *tail ? " " : "", tail);
    return n > 0 && (size_t)n < out_cap;
  }
  return false;
}

void init() {
  static bool              s_inited = false;
  static locommand::Engine s_user_eng{"user"};
  if (s_inited) return;
  s_inited = true;

  register_engine(s_user_eng);

  s_user_eng.setGuardFor("logout",         &require_user);
  s_user_eng.setGuardFor("whoami",         &require_user);
  s_user_eng.setGuardFor("bye",            &require_user);
  s_user_eng.setGuardFor("sessions.clear", &require_admin);
  s_user_eng.setGuardFor("delete",         &require_admin);
  s_user_eng.setGuardFor("rename",         &require_admin);
  s_user_eng.setGuardFor("passwd",         &require_admin);

  lostar::router().add(&s_user_eng);
}

}  // namespace louser
