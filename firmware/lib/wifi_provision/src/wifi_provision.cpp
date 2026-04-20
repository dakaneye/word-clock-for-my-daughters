// firmware/lib/wifi_provision/src/wifi_provision.cpp
// Guarded with #ifdef ARDUINO — same pattern as nvs_store.cpp / dns_wrapper.cpp /
// web_server.cpp. PlatformIO LDF would otherwise compile this TU in the
// native test build where <WiFi.h> etc. don't exist.
#ifdef ARDUINO

// Compile-time guard: this module targets the Arduino-ESP32 framework
// specifically (it uses <WiFi.h>, <Preferences.h> via nvs_store, <DNSServer.h>
// via dns_wrapper, and <WebServer.h> via web_server — all Arduino-ESP32 core
// headers). If a future toolchain change defines ARDUINO but on a different
// arch, fail loud instead of silently compiling to nothing useful.
#if !defined(ARDUINO_ARCH_ESP32)
  #error "wifi_provision requires the Arduino-ESP32 framework (ARDUINO_ARCH_ESP32 not defined)"
#endif

#include <WiFi.h>
#include <IPAddress.h>
#include <time.h>
#include "wifi_provision.h"
#include "wifi_provision/state_machine.h"
#include "wifi_provision/form_parser.h"

// Forward decls to the adapter namespaces (defined in sibling .cpp files).
// Only the symbols actually called from this TU are declared here.
namespace wc::wifi_provision::nvs_store {
    bool has_credentials();
    struct StoredCredentials { String ssid; String pw; String tz; };
    StoredCredentials read();
    bool write(const FormBody& body);
    bool touch_last_sync(uint64_t unix_seconds);
    uint64_t last_sync();
    void clear();
}

namespace wc::wifi_provision::dns_hijack {
    void begin(const IPAddress& ap_ip);
    void loop();
    void stop();
}

namespace wc::wifi_provision::web {
    void begin(std::function<void(const FormBody&)> on_submit,
               std::function<std::string()> status);
    void loop();
    void stop();
}

namespace wc::wifi_provision {

static StateMachine sm;
static State last_logged_state = State::Boot;

// Timers (all in millis). 0 means "not armed".
static uint32_t ap_started_at = 0;
static uint32_t awaiting_started_at = 0;
static uint32_t validating_started_at = 0;
static uint32_t last_validating_heartbeat_at = 0;
static uint32_t last_sta_attempt_at = 0;
static uint32_t sta_backoff_ms = 2000;

// Grace period after successful provisioning during which we keep the AP up
// so the user's captive-portal browser sees "Connected!" before the AP
// vanishes. 0 means "not armed" (warm boot via StaConnecting doesn't use this).
static uint32_t online_grace_started_at = 0;
static constexpr uint32_t ONLINE_GRACE_MS = 5UL * 1000UL;
static constexpr uint32_t VALIDATING_HEARTBEAT_MS = 2UL * 1000UL;

// In-flight credentials held while awaiting Audio-button confirmation or
// doing a trial connection. Never written to NVS until Validating succeeds.
static FormBody pending;

static constexpr uint32_t AP_TIMEOUT_MS           = 10UL * 60UL * 1000UL;
static constexpr uint32_t CONFIRMATION_TIMEOUT_MS = 60UL * 1000UL;
static constexpr uint32_t VALIDATING_TIMEOUT_MS   = 30UL * 1000UL;
static constexpr uint32_t STA_BACKOFF_INITIAL_MS  = 2000;
static constexpr uint32_t STA_BACKOFF_MAX_MS      = 5UL * 60UL * 1000UL;

static const char* state_name(State s) {
    switch (s) {
        case State::Boot:                 return "Boot";
        case State::StaConnecting:        return "StaConnecting";
        case State::ApActive:             return "ApActive";
        case State::AwaitingConfirmation: return "AwaitingConfirmation";
        case State::Validating:           return "Validating";
        case State::Online:               return "Online";
        case State::IdleNoCredentials:    return "IdleNoCredentials";
    }
    // Unreachable: the switch above is exhaustive on a C++ enum class.
    // Kept to silence -Wreturn-type on compilers that don't see through
    // the exhaustive switch.
    return "?";
}

static void log_state_if_changed() {
    State now = sm.state();
    if (now != last_logged_state) {
        Serial.printf("[wifi_provision] %s -> %s\n",
                      state_name(last_logged_state), state_name(now));
        last_logged_state = now;
    }
}

static std::string confirmation_message() {
    switch (sm.state()) {
        case State::AwaitingConfirmation: return "Waiting for Audio button…";
        case State::Validating:           return "Connecting…";
        case State::Online:               return "Connected!";
        default:                          return "Ready.";
    }
}

static void start_ap() {
    WiFi.mode(WIFI_AP_STA);
    uint64_t mac = ESP.getEfuseMac();
    char ssid[32];
    snprintf(ssid, sizeof(ssid), "WordClock-Setup-%04X",
             (uint16_t)((mac >> 32) & 0xFFFF));
    WiFi.softAP(ssid, /* password = */ nullptr, /* channel = */ 1,
                /* ssid_hidden = */ 0, /* max_connection = */ 4);
    IPAddress ip = WiFi.softAPIP();
    Serial.printf("[wifi_provision] AP up: SSID=%s IP=%s\n",
                  ssid, ip.toString().c_str());
    dns_hijack::begin(ip);
    web::begin(
        [](const FormBody& body) {
            pending = body;
            sm.handle(Event::FormSubmitted);
            awaiting_started_at = millis();
            Serial.println("[wifi_provision] form accepted; awaiting audio confirm");
            log_state_if_changed();
        },
        confirmation_message
    );
    ap_started_at = millis();
}

static void stop_ap() {
    web::stop();
    dns_hijack::stop();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    ap_started_at = 0;
}

static void start_sta() {
    auto creds = nvs_store::read();
    setenv("TZ", creds.tz.c_str(), 1);
    tzset();
    WiFi.mode(WIFI_STA);
    WiFi.begin(creds.ssid.c_str(), creds.pw.c_str());
    last_sta_attempt_at = millis();
    Serial.printf("[wifi_provision] STA attempt to %s (backoff=%ums)\n",
                  creds.ssid.c_str(), sta_backoff_ms);
}

static void start_validating() {
    // KEEP the AP up during Validating so the captive-portal page can poll
    // /status and show the user "Connecting…" → "Connected!" before the AP
    // vanishes. Possible because we run in WIFI_AP_STA mode (set by
    // start_ap) — the radio handles both concurrently. AP teardown is
    // deferred to the Online state's 5-second grace timer below.
    //
    // Supersedes the original "stop_ap before WiFi.mode(WIFI_STA)" pattern.
    // The old flow left the user's browser stranded with no way to surface
    // success — handle_status' "Connected!" branch was unreachable because
    // the web server was dead by the time the state reached Online.
    WiFi.begin(pending.ssid.c_str(), pending.pw.c_str());
    validating_started_at = millis();
    last_validating_heartbeat_at = 0;
    Serial.printf("[wifi_provision] validating credentials for SSID=%s\n",
                  pending.ssid.c_str());
}

void begin() {
    Serial.println("[wifi_provision] begin");
    if (nvs_store::has_credentials()) {
        sm.handle(Event::BootWithCredentials);
        start_sta();
    } else {
        sm.handle(Event::BootWithNoCredentials);
        start_ap();
    }
    log_state_if_changed();
}

void loop() {
    const uint32_t now = millis();

    switch (sm.state()) {
        case State::StaConnecting: {
            if (WiFi.status() == WL_CONNECTED) {
                Serial.println("[wifi_provision] STA connected");
                sta_backoff_ms = STA_BACKOFF_INITIAL_MS;
                sm.handle(Event::StaConnected);
                // NTP sync happens in main.cpp's ntp module; touch_last_sync on success.
            } else if (now - last_sta_attempt_at > sta_backoff_ms) {
                sta_backoff_ms = std::min<uint32_t>(sta_backoff_ms * 2, STA_BACKOFF_MAX_MS);
                start_sta();
            }
            break;
        }
        case State::ApActive: {
            dns_hijack::loop();
            web::loop();
            if (now - ap_started_at > AP_TIMEOUT_MS) {
                Serial.println("[wifi_provision] AP timeout; entering idle");
                sm.handle(Event::ApTimeout);
                stop_ap();
            }
            break;
        }
        case State::AwaitingConfirmation: {
            dns_hijack::loop();
            web::loop();
            if (now - awaiting_started_at > CONFIRMATION_TIMEOUT_MS) {
                Serial.println("[wifi_provision] audio confirm timeout");
                sm.handle(Event::ConfirmationTimeout);
                pending = {};
                // Note: ap_started_at is intentionally NOT reset here. The
                // 10-minute AP lifetime runs from the first AP start; a
                // submit-then-timeout cycle doesn't extend the broadcast
                // window. stop_ap()/start_ap() aren't called because the AP
                // hardware stayed up throughout AwaitingConfirmation — the
                // state returns to ApActive with DNS + web already serving.
            }
            break;
        }
        case State::Validating: {
            // Keep serving the captive page so the user sees "Connecting…"
            // via /status polling. AP + STA run concurrently in WIFI_AP_STA.
            dns_hijack::loop();
            web::loop();
            // Heartbeat the serial log so operators can see progress during
            // the 5-30 s window WiFi.status() takes to resolve.
            if (now - last_validating_heartbeat_at > VALIDATING_HEARTBEAT_MS) {
                uint32_t elapsed_s = (now - validating_started_at) / 1000;
                Serial.printf("[wifi_provision] connecting to home WiFi... (%us elapsed)\n",
                              elapsed_s);
                last_validating_heartbeat_at = now;
            }
            if (WiFi.status() == WL_CONNECTED) {
                Serial.printf("[wifi_provision] validated; committing creds (SSID=%s)\n",
                              pending.ssid.c_str());
                if (!nvs_store::write(pending)) {
                    Serial.println("[wifi_provision] ERROR: NVS write failed; retrying captive");
                    pending = {};
                    // Disconnect STA only (wifioff=false) — the AP must stay
                    // up so the user can retry submitting. With wifioff=true
                    // the radio shuts off and the AP disappears.
                    WiFi.disconnect(/* wifioff = */ false);
                    sm.handle(Event::ValidationFailed);
                    // AP is already running; do not re-call start_ap().
                } else {
                    pending = {};
                    sta_backoff_ms = STA_BACKOFF_INITIAL_MS;
                    // Arm the grace timer so the user's /status poll catches
                    // "Connected!" before the AP goes down.
                    online_grace_started_at = millis();
                    sm.handle(Event::ValidationSucceeded);
                }
            } else if (now - validating_started_at > VALIDATING_TIMEOUT_MS) {
                Serial.println("[wifi_provision] validation timeout; back to captive");
                pending = {};
                WiFi.disconnect(/* wifioff = */ false);
                sm.handle(Event::ValidationFailed);
                // AP is already running from AwaitingConfirmation.
            }
            break;
        }
        case State::Online: {
            // Grace window: right after a fresh provisioning, keep the AP +
            // web server alive for a few seconds so the user's captive-portal
            // page can poll /status one more time and see "Connected!".
            if (online_grace_started_at != 0) {
                if (now - online_grace_started_at > ONLINE_GRACE_MS) {
                    Serial.println("[wifi_provision] online grace window ended; stopping AP");
                    stop_ap();
                    online_grace_started_at = 0;
                } else {
                    dns_hijack::loop();
                    web::loop();
                }
            }
            if (WiFi.status() != WL_CONNECTED) {
                Serial.println("[wifi_provision] WiFi dropped");
                sta_backoff_ms = STA_BACKOFF_INITIAL_MS;
                // If the drop happened during the grace window, abandon the
                // window — the AP is about to be torn down anyway via
                // start_sta's WiFi.mode(WIFI_STA) call.
                online_grace_started_at = 0;
                sm.handle(Event::WiFiDropped);
                start_sta();
            }
            break;
        }
        case State::IdleNoCredentials: {
            // Nothing to do; waiting for reset combo.
            break;
        }
        case State::Boot: {
            // Handled during begin().
            break;
        }
    }
    log_state_if_changed();
}

State state() { return sm.state(); }

uint32_t seconds_since_last_sync() {
    uint64_t last = nvs_store::last_sync();
    if (last == 0) return UINT32_MAX;
    time_t now = time(nullptr);
    if (now <= 0 || (uint64_t)now < last) return UINT32_MAX;
    return (uint32_t)(now - (time_t)last);
}

void reset_to_captive() {
    // Per spec §Reset flow (line 45): clear NVS then ESP.restart().
    // Clean WiFi stack state is load-bearing; we explicitly don't try to
    // transition in-place.
    Serial.println("[wifi_provision] reset_to_captive: clearing NVS + restarting");
    Serial.flush();
    nvs_store::clear();
    ESP.restart();
    // not reached
}

void confirm_audio() {
    // The state-machine transition alone is insufficient — WiFi.begin() and
    // the validation timer must fire on entry to Validating, which is what
    // start_validating() does. Without this call, the clock appears to "try"
    // to connect but never actually calls WiFi.begin with the submitted
    // credentials; the uninitialized validating_started_at forces an immediate
    // timeout into ValidationFailed.
    Serial.println("[wifi_provision] audio confirmed");
    sm.handle(Event::AudioButtonConfirmed);
    if (sm.state() == State::Validating) {
        start_validating();
    }
    log_state_if_changed();
}

void touch_last_sync(uint64_t unix_seconds) {
    nvs_store::touch_last_sync(unix_seconds);
}

} // namespace wc::wifi_provision

#endif // ARDUINO
