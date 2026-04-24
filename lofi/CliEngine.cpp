#include <lofi/Lofi.h>

#ifdef ESP32

#include <Arduino.h>
#include <WiFi.h>
#include <cstdio>
#include <cstring>

#if defined(ARDUINO_ARCH_ESP32)
#include <esp_task_wdt.h>
#endif

#include <locommand/Engine.h>
#include <lolog/LoLog.h>
#include <lostar/Busy.h>
#include <lostar/Deferred.h>
#include <lostar/Host.h>
#include <lostar/Router.h>

// App-provided hook (weakly defined by lotato) that re-persists lofi credentials + resets
// reconnect timers when CLI-driven settings changes happen.
extern "C" void lofi_on_lo_settings_changed(void);

namespace lofi {

namespace {

/** Min interval between Lofi::serviceWifiScan() calls from the tick hook. */
constexpr uint32_t kWifiScanServiceMs = 100;

/**
 * Lofi exposes exactly one async scan and one async connect at a time, so one deferred-reply
 * slot per operation is sufficient. If the caller has no deferrer attached (e.g. the fork
 * adapter skipped it), the handler falls back to returning the snapshot synchronously.
 */
lostar_deferred_reply g_pending_scan    = {};
lostar_deferred_reply g_pending_connect = {};

void clear_deferred(lostar_deferred_reply& d) {
  d.fire      = nullptr;
  d.route_ctx = nullptr;
}

void on_scan_done(void* /*ctx*/, const char* text) {
  if (!g_pending_scan.fire) return;
  lostar_deferred_reply d = g_pending_scan;
  clear_deferred(g_pending_scan);
  const char* msg = text ? text : "";
  lostar_fire_deferred_reply(&d, msg, (uint32_t)strlen(msg));
}

void on_connect_done(void* /*ctx*/, bool ok, const char* detail) {
  char msg[96];
  if (ok) snprintf(msg, sizeof(msg), "WiFi connected: %s", detail ? detail : "");
  else    snprintf(msg, sizeof(msg), "WiFi connect failed: %s", detail ? detail : "?");
  if (!g_pending_connect.fire) return;
  lostar_deferred_reply d = g_pending_connect;
  clear_deferred(g_pending_connect);
  lostar_fire_deferred_reply(&d, msg, (uint32_t)strlen(msg));
}

/* ── wifi root handlers ───────────────────────────────────────────── */

void h_wifi_status(locommand::Context& ctx) {
  auto&       lf = Lofi::instance();
  wl_status_t wl = WiFi.status();
  char        saved_ssid[33] = {};
  char        saved_psk[65]  = {};
  lf.getActiveCredentials(saved_ssid, sizeof(saved_ssid), saved_psk, sizeof(saved_psk));
  if (wl == WL_CONNECTED) {
    ctx.out.appendf("WiFi: connected\nSSID: %s\nIP: %s", WiFi.SSID().c_str(),
                    WiFi.localIP().toString().c_str());
  } else {
    ctx.out.appendf("WiFi: not connected\nSaved: %s\nUse: wifi scan",
                    saved_ssid[0] ? saved_ssid : "(none)");
  }
}

void h_wifi_scan(locommand::Context& ctx) {
  auto& lf = Lofi::instance();
  lf.serviceWifiScan();

  lostar_deferred_reply dr = lostar_capture_deferred_reply();
  if (!dr.fire) {
    if (lf.scanSnapshotCount() > 0) {
      lf.formatScanBody(ctx.out);
    } else {
      lf.requestWifiScan();
      ctx.out.append("Scanning for WiFi devices...");
    }
    return;
  }

  if (lf.scanSnapshotCount() > 0) {
    lf.formatScanBody(ctx.out);
    return;
  }

  g_pending_scan = dr;
  lf.requestWifiScan();
  ctx.out.append("Scanning for WiFi devices...");
}

void h_wifi_connect(locommand::Context& ctx) {
  if (ctx.argc < 1) { ctx.printHelp(); return; }
  auto&       lf   = Lofi::instance();
  const char* tok1 = ctx.argv[0];
  const char* tok2 = (ctx.argc >= 2) ? ctx.argv[1] : "";

  char ssid_to_use[33] = {};
  bool is_index        = true;
  for (const char* q = tok1; *q; q++) {
    if (*q < '0' || *q > '9') { is_index = false; break; }
  }
  if (is_index && tok1[0] != '\0') {
    int     idx = atoi(tok1) - 1;
    int32_t rssi;
    if (idx < 0 || !lf.scanSnapshotEntry(idx, ssid_to_use, &rssi)) {
      ctx.out.appendf("Err - index out of range (1..%d)\nRun: wifi scan first", lf.scanSnapshotCount());
      return;
    }
  } else {
    strncpy(ssid_to_use, tok1, sizeof(ssid_to_use) - 1);
  }

  char pwd_to_use[65] = {};
  if (tok2[0] != '\0') {
    strncpy(pwd_to_use, tok2, sizeof(pwd_to_use) - 1);
  } else {
    lf.getKnownWifiPassword(ssid_to_use, pwd_to_use, sizeof(pwd_to_use));
  }

  lf.saveWifiConnect(ssid_to_use, pwd_to_use);
  lofi_on_lo_settings_changed();

  lostar_deferred_reply dr = lostar_capture_deferred_reply();
  if (dr.fire) g_pending_connect = dr;
  lf.beginConnect(ssid_to_use, pwd_to_use);
  ::lolog::LoLog::debug("lofi", "lofi cli: wifi connecting ssid=%s", ssid_to_use);
  ctx.out.appendf("Connecting to %s...", ssid_to_use);
}

void h_wifi_forget(locommand::Context& ctx) {
  if (ctx.argc < 1) { ctx.printHelp(); return; }
  if (!Lofi::instance().forgetKnownWifi(ctx.argv[0])) {
    ctx.out.append("Err - SSID not in known list\n");
    return;
  }
  lofi_on_lo_settings_changed();
  ctx.out.append("OK\n");
}

const locommand::ArgSpec k_wifi_connect_args[] = {
    {"n_or_ssid", "string", nullptr, true, "Scan index (1-based) or SSID"},
    {"password",  "secret", nullptr, false, "PSK if not already saved"},
};

const locommand::ArgSpec k_wifi_forget_args[] = {
    {"ssid", "string", nullptr, true, "Network SSID to remove from known list"},
};

void wifi_tick(void* /*ctx*/) {
  static uint32_t s_last_ms = 0;
  const uint32_t  now       = millis();
  if ((uint32_t)(now - s_last_ms) < kWifiScanServiceMs) return;
  s_last_ms = now;
#if defined(ARDUINO_ARCH_ESP32)
  esp_task_wdt_reset();
#endif
  Lofi::instance().serviceWifiScan();
#if defined(ARDUINO_ARCH_ESP32)
  esp_task_wdt_reset();
#endif
}

/** Busy hint: an async scan or connect is currently in flight. */
bool wifi_busy(void* /*ctx*/) {
  return g_pending_scan.fire != nullptr || g_pending_connect.fire != nullptr;
}

}  // namespace

void init() {
  static bool              s_inited = false;
  static locommand::Engine s_eng{"wifi"};
  if (s_inited) return;
  s_inited = true;

  auto& lf = Lofi::instance();
  lf.begin();
  lf.setScanCompleteCallback(&on_scan_done, nullptr);
  lf.setConnectCompleteCallback(&on_connect_done, nullptr);

  s_eng.add("status", &h_wifi_status, nullptr, nullptr, "STA / saved SSID snapshot");
  s_eng.add("scan",   &h_wifi_scan,   nullptr, nullptr, "scan for APs (async reply)");
  s_eng.addWithArgs("connect", &h_wifi_connect, k_wifi_connect_args, 2, nullptr,
                    "connect by index or SSID");
  s_eng.addWithArgs("forget",  &h_wifi_forget,  k_wifi_forget_args,  1, nullptr,
                    "remove SSID from known list");
  s_eng.setRootBrief("WiFi STA scan/connect");

  lostar::router().add(&s_eng);

  lostar_register_tick_hook(&wifi_tick, nullptr);
  lostar_register_busy_hint(&wifi_busy, nullptr);
}

}  // namespace lofi

#endif  // ESP32
