#include <louser/Hash.h>

#include <Arduino.h>
#include <SHA256.h>
#include <cstring>

#ifdef ESP32
extern "C" {
#include <esp_system.h>
}
#endif

namespace louser {

void hash_password(const uint8_t* salt, size_t salt_len, const char* password, uint8_t out[32]) {
  if (!out) return;
  SHA256 h;
  h.reset();
  if (salt && salt_len) h.update(salt, salt_len);
  if (password) h.update(password, strlen(password));
  h.finalize(out, 32);
  for (uint32_t i = 1; i < LOUSER_HASH_ITERS; i++) {
    h.reset();
    if (salt && salt_len) h.update(salt, salt_len);
    h.update(out, 32);
    h.finalize(out, 32);
  }
}

void random_bytes(uint8_t* out, size_t n) {
  if (!out || !n) return;
#ifdef ESP32
  esp_fill_random(out, n);
#else
  for (size_t i = 0; i < n; i++) {
    out[i] = (uint8_t)random(0, 256);
  }
#endif
}

}  // namespace louser
