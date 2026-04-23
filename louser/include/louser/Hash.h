#pragma once

#include <cstddef>
#include <cstdint>

namespace louser {

/** Iteration count used by `hash_password`. Exposed so tests can lower it. */
#ifndef LOUSER_HASH_ITERS
#define LOUSER_HASH_ITERS 1000
#endif

/**
 * Iterated SHA-256 over `salt || password`. Output is always 32 bytes.
 *
 * Salted + iterated hashing only; not PBKDF2 (stored-blob format is plain `hash32`, so we can
 * move to PBKDF2 later without migrating rows by bumping an opaque version prefix when/if the
 * need arises).
 */
void hash_password(const uint8_t* salt, size_t salt_len,
                   const char* password,
                   uint8_t out[32]);

/** Fill @p out with @p n bytes of random material suitable for a per-user salt. */
void random_bytes(uint8_t* out, size_t n);

}  // namespace louser
