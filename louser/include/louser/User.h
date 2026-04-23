#pragma once

#include <cstddef>
#include <cstdint>

#include <lodb/LoDB.h>

/**
 * louser — portable user / session module for lo-star consumers.
 *
 * Users live in an LoDB table keyed by an internal `uint64_t id` (assigned on insert). Each
 * user row carries its own 32B salt plus an iterated-SHA-256 password hash. The first
 * successful `create()` flips its row's `admin` flag to true; subsequent creates are regular
 * users. Changing a user's password rotates the salt at the same time.
 */

namespace louser {

/** In-memory mirror of `LoUserUser` (pb) with fixed-size fields for ease of use. */
struct User {
  uint64_t id = 0;
  char     username[33] = {};
  uint8_t  salt[32] = {};
  uint16_t salt_len = 0;
  uint8_t  password_hash[32] = {};
  uint16_t password_hash_len = 0;
  bool     admin = false;
  uint32_t created_ms = 0;
};

/** Users store: one row per registered user. All methods are synchronous / blocking. */
class Users {
public:
  /** @p mount is the LoFS mount prefix (`/__ext__` default); `db_name` is our namespace. */
  explicit Users(const char* mount = "/__ext__");

  /** Lookup by case-insensitive `username`; returns true on hit and fills @p out. */
  bool findByUsername(const char* username, User& out);
  bool findById(uint64_t id, User& out);
  int  count();

  /**
   * Create a new user with password. Rejects duplicate usernames. If this is the first user
   * in the table, stamps `admin=true` on the new row automatically.
   *
   * @return 0=ok, 1=duplicate, 2=invalid args, 3=storage error.
   */
  int create(const char* username, const char* password, User* created_out = nullptr);

  /** Delete by username. @return true on success. */
  bool deleteUser(const char* username);

  /** Rename by username. @return 0=ok, 1=source-missing, 2=dest-exists, 3=invalid, 4=storage. */
  int rename(const char* old_name, const char* new_name);

  /**
   * Rotate salt + rehash password for an existing user. @return true on success.
   */
  bool setPassword(const char* username, const char* new_password);

  /** True if @p password matches the stored hash for @p user. */
  static bool verifyPassword(const User& user, const char* password);

private:
  LoDb _db;
  bool _registered = false;
  void ensure_registered();
};

}  // namespace louser
