#include <lostar/NodeId.h>

#include <cctype>
#include <cstdio>
#include <cstring>

namespace lostar {

namespace {

bool all_digits(const char* p, size_t n) {
  for (size_t i = 0; i < n; i++) {
    if (p[i] < '0' || p[i] > '9') return false;
  }
  return n > 0;
}

bool all_hex(const char* p, size_t n) {
  for (size_t i = 0; i < n; i++) {
    char c = p[i];
    bool ok = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
    if (!ok) return false;
  }
  return n > 0;
}

bool parse_u32_hex(const char* p, size_t n, uint32_t* out) {
  if (n == 0 || n > 8) return false;
  uint32_t v = 0;
  for (size_t i = 0; i < n; i++) {
    char c = p[i];
    uint32_t d;
    if (c >= '0' && c <= '9') d = (uint32_t)(c - '0');
    else if (c >= 'a' && c <= 'f') d = (uint32_t)(c - 'a' + 10);
    else if (c >= 'A' && c <= 'F') d = (uint32_t)(c - 'A' + 10);
    else return false;
    v = (v << 4) | d;
  }
  *out = v;
  return true;
}

bool parse_u32_dec(const char* p, size_t n, uint32_t* out) {
  if (n == 0 || n > 10) return false;
  uint64_t v = 0;
  for (size_t i = 0; i < n; i++) {
    char c = p[i];
    if (c < '0' || c > '9') return false;
    v = v * 10 + (uint64_t)(c - '0');
    if (v > 0xFFFFFFFFu) return false;
  }
  *out = (uint32_t)v;
  return true;
}

}  // namespace

void format_canonical(NodeId id, char* out, size_t out_cap) {
  if (!out || out_cap < 10) {
    if (out && out_cap > 0) out[0] = '\0';
    return;
  }
  snprintf(out, out_cap, "!%08x", (unsigned)id);
}

bool parse_canonical(const char* in, NodeId* out) {
  if (!in || !out) return false;
  while (*in == ' ' || *in == '\t') in++;
  size_t n = strlen(in);
  while (n > 0 && (in[n - 1] == ' ' || in[n - 1] == '\t' || in[n - 1] == '\r' || in[n - 1] == '\n')) n--;
  if (n == 0) return false;

  if (in[0] == '!') {
    uint32_t v;
    if (!parse_u32_hex(in + 1, n - 1, &v)) return false;
    *out = v;
    return true;
  }
  if (n >= 2 && in[0] == '0' && (in[1] == 'x' || in[1] == 'X')) {
    uint32_t v;
    if (!parse_u32_hex(in + 2, n - 2, &v)) return false;
    *out = v;
    return true;
  }
  // Ambiguous bare: prefer decimal when all digits, else hex.
  if (all_digits(in, n)) {
    uint32_t v;
    if (!parse_u32_dec(in, n, &v)) return false;
    *out = v;
    return true;
  }
  if (all_hex(in, n)) {
    uint32_t v;
    if (!parse_u32_hex(in, n, &v)) return false;
    *out = v;
    return true;
  }
  return false;
}

}  // namespace lostar
