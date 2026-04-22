#include "LoSerial.h"

#include <cstdlib>
#include <cstdio>
#include <cstring>

namespace loserial {

namespace {

Stream* g_sink = nullptr;

Stream& sink() {
  if (g_sink) return *g_sink;
  return Serial;
}

constexpr size_t kChunk = 48;

}  // namespace

void LoSerial::begin(Stream& s) { g_sink = &s; }

Stream& LoSerial::stream() { return sink(); }

void LoSerial::writeChunked(const char* data, size_t len) {
  if (!data || len == 0) return;
  Stream& s = sink();
  size_t written = 0;
  while (written < len) {
    size_t take = len - written;
    if (take > kChunk) take = kChunk;
    s.write(reinterpret_cast<const uint8_t*>(data + written), take);
    written += take;
    yield();
  }
}

void LoSerial::printLine(const char* s) {
  if (s) writeChunked(s, strlen(s));
  sink().write(reinterpret_cast<const uint8_t*>("\r\n"), 2);
}

void LoSerial::printMeshCliReply(const char* reply) {
  if (!reply) return;
  Stream& s = sink();
  s.write(reinterpret_cast<const uint8_t*>("  -> "), 5);
  writeChunked(reply, strlen(reply));
  s.write(reinterpret_cast<const uint8_t*>("\r\n"), 2);
}

void LoSerial::printf(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vprintf(fmt, ap);
  va_end(ap);
}

void LoSerial::vprintf(const char* fmt, va_list ap) {
  if (!fmt) return;
  char stack_buf[256];
  va_list ap_probe;
  va_copy(ap_probe, ap);
  int n = vsnprintf(stack_buf, sizeof(stack_buf), fmt, ap_probe);
  va_end(ap_probe);
  if (n <= 0) return;

  if ((size_t)n < sizeof(stack_buf)) {
    writeChunked(stack_buf, (size_t)n);
    return;
  }

  size_t needed = (size_t)n + 1;
  char* heap_buf = (char*)malloc(needed);
  if (!heap_buf) {
    // OOM fallback: emit best-effort truncated line from stack buffer.
    writeChunked(stack_buf, strlen(stack_buf));
    return;
  }

  va_list ap_emit;
  va_copy(ap_emit, ap);
  int n2 = vsnprintf(heap_buf, needed, fmt, ap_emit);
  va_end(ap_emit);
  if (n2 > 0) {
    size_t out_len = (size_t)n2;
    if (out_len >= needed) out_len = needed - 1;
    writeChunked(heap_buf, out_len);
  }
  free(heap_buf);
}

}  // namespace loserial
