#include <louser/Engine.h>
#include <louser/Auth.h>
#include <louser/Guard.h>
#include <louser/User.h>

#include <lolog/LoLog.h>
#include <locommand/Router.h>
#include <lostar/Router.h>

#include <cctype>
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
  user_eng.add("logout", &h_logout, nullptr, nullptr, "clear this node's session");
  user_eng.add("sessions.clear", &h_sessions_clear, nullptr, nullptr, "[admin] drop all sessions");
  user_eng.addWithArgs("delete", &h_delete, k_user_args, 1, nullptr, "[admin] delete account");
  user_eng.addWithArgs("rename", &h_rename, k_rename_args, 2, nullptr, "[admin] rename account");
  user_eng.addWithArgs("passwd", &h_passwd, k_passwd_args, 2, nullptr, "[admin] reset password");
  user_eng.setRootBrief("user accounts & sessions");
}

namespace {

bool guest_session(void* app_ctx) { return !require_user(app_ctx); }

void guest_help_banner(lomessage::Buffer& out) {
  out.append("You're not signed in.\n");
}

}  // namespace

void init() {
  static bool              s_inited = false;
  static locommand::Engine s_hi_eng{"hi"};
  static locommand::Engine s_bye_eng{"bye"};
  static locommand::Engine s_whoami_eng{"whoami"};
  static locommand::Engine s_user_eng{"user"};
  if (s_inited) return;
  s_inited = true;

  s_hi_eng.setRootGuard(&require_logged_out);
  s_hi_eng.setRestHandler(&h_hi, "<username> <password>");
  s_hi_eng.setRootBrief("register if new, else log in");

  s_bye_eng.setRootGuard(&require_user);
  s_bye_eng.setBareHandler(&h_logout);
  s_bye_eng.setRootBrief("log out");

  s_whoami_eng.setRootGuard(&require_user);
  s_whoami_eng.setBareHandler(&h_whoami);
  s_whoami_eng.setRootBrief("current user");

  register_engine(s_user_eng);

  s_user_eng.setGuardFor("logout", &require_user);
  s_user_eng.setGuardFor("sessions.clear", &require_admin);
  s_user_eng.setGuardFor("delete", &require_admin);
  s_user_eng.setGuardFor("rename", &require_admin);
  s_user_eng.setGuardFor("passwd", &require_admin);

  auto& rt = lostar::router();
  rt.add(&s_hi_eng);
  rt.add(&s_bye_eng);
  rt.add(&s_whoami_eng);
  rt.add(&s_user_eng);

  locommand::Router::setGuestHelp(&guest_session, &guest_help_banner);
}

}  // namespace louser
