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
#include <string>
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
    bool clear();
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
    void reset_submit_state(const std::string& msg, bool reset_rate_limit);
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

static constexpr uint32_t VALIDATING_HEARTBEAT_MS = 2UL * 1000UL;

// In-flight credentials held while awaiting Audio-button confirmation or
// doing a trial connection. Never written to NVS until Validating succeeds.
static FormBody pending;

// Reason-specific message shown for the ApActive state via /status and the
// form banner. Distinguishes a fresh AP ("Ready."), a confirmation timeout,
// and a validation failure — the state alone can't tell them apart.
static std::string ap_status_msg = "Ready.";

static constexpr uint32_t AP_TIMEOUT_MS           = 10UL * 60UL * 1000UL;
static constexpr uint32_t CONFIRMATION_TIMEOUT_MS = 60UL * 1000UL;
// 60 s (was 30 s). Intermittent timeouts observed on bench 2026-04-20
// where a first-try STA handshake took >30 s on a mildly congested 2.4
// GHz band. Retry usually worked, but the user shouldn't need a retry
// at all for a normal home network.
static constexpr uint32_t VALIDATING_TIMEOUT_MS   = 60UL * 1000UL;
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
    // The captive page only polls /status after a form submit, so any state
    // we return to outside of the submit → confirm → validate flow implies
    // something went wrong. ApActive here means "came back from failed
    // validation" (or a timed-out AwaitingConfirmation), so tell the user.
    switch (sm.state()) {
        case State::AwaitingConfirmation: return "Waiting for Audio button...";
        case State::Validating:           return "Connecting...";
        case State::Online:               return "Connected!";
        case State::ApActive:             return ap_status_msg;
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
            // Only a submit while genuinely in ApActive starts the
            // confirmation flow. A duplicate POST arriving while already
            // AwaitingConfirmation (an iOS re-probe, a second browser tab)
            // must not silently swap the pending credentials or reset the
            // 60 s timer — FormSubmitted is a no-op transition there anyway.
            if (sm.state() != State::ApActive) {
                Serial.println("[wifi_provision] ignoring submit; not in ApActive");
                return;
            }
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

static String sta_ssid;
static String sta_pw;
static bool   sta_creds_loaded = false;

static void start_sta() {
    // Read credentials from NVS and apply the POSIX TZ exactly once per
    // process. Backoff retries and WiFi-drop reconnects reuse the cached
    // copy rather than re-reading NVS + calling tzset() on every attempt:
    // the stored creds never change while running (reset_to_captive()
    // restarts the chip), so re-reading them each retry is pure waste.
    if (!sta_creds_loaded) {
        auto creds = nvs_store::read();
        sta_ssid = creds.ssid;
        sta_pw   = creds.pw;
        setenv("TZ", creds.tz.c_str(), 1);
        tzset();
        sta_creds_loaded = true;
    }
    WiFi.mode(WIFI_STA);
    WiFi.begin(sta_ssid.c_str(), sta_pw.c_str());
    last_sta_attempt_at = millis();
    Serial.printf("[wifi_provision] STA attempt to %s (backoff=%ums)\n",
                  sta_ssid.c_str(), sta_backoff_ms);
}

static void start_validating() {
    // Tear down the AP before starting STA. Attempted a "keep AP up during
    // Validating" pattern (2026-04-20) so the captive page could show
    // "Connecting…" → "Connected!", but the ESP32's AP_STA mode forces both
    // sides onto the STA's channel once connecting. The channel hop drops
    // the iPhone's association to our AP anyway, so the user's browser
    // can't poll /status through the transition — AND the STA connection
    // became unreliable (timeouts on known-good credentials). Reverted.
    //
    // UX compensation: the captive-portal form renders with last_error set
    // on validation failure, so when the AP comes back after a fail, the
    // user sees "Didn't connect. Check your password and try again."
    stop_ap();
    WiFi.disconnect(/* wifioff = */ true);
    WiFi.mode(WIFI_STA);
    WiFi.begin(pending.ssid.c_str(), pending.pw.c_str());
    validating_started_at = millis();
    last_validating_heartbeat_at = 0;
    Serial.printf("[wifi_provision] validating credentials for SSID=%s\n",
                  pending.ssid.c_str());
}

// Shared exit path for both validation failures (NVS write failed, or the
// trial connection timed out): drop pending creds, tear WiFi down, return to
// the captive AP with an accurate inline error, and keep the wrong-password
// attempt counted against the per-AP rate limit.
static void fail_validation(const char* msg) {
    pending = {};
    WiFi.disconnect(/* wifioff = */ true);
    ap_status_msg = msg;
    sm.handle(Event::ValidationFailed);
    start_ap();
    web::reset_submit_state(msg, /*reset_rate_limit=*/false);
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
                // Return the captive UI to the form with an accurate message:
                // the user simply didn't press Audio — this is NOT a password
                // failure — and clear the stuck "Press Audio" waiting page.
                // Confirmation timeouts don't count against the submit rate
                // limit. ap_started_at is intentionally NOT reset (the
                // 10-minute AP lifetime runs from first AP start), and
                // stop_ap()/start_ap() aren't called because the AP hardware
                // stayed up throughout AwaitingConfirmation — DNS + web keep
                // serving as the state returns to ApActive.
                ap_status_msg = "Timed out waiting for the Audio button. "
                                "Submit the form again.";
                web::reset_submit_state(ap_status_msg, /*reset_rate_limit=*/true);
            }
            break;
        }
        case State::Validating: {
            // AP is torn down; just a serial heartbeat so operators see
            // progress during the 5-30 s WiFi.status() wait.
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
                    fail_validation("Couldn't save credentials. Please try again.");
                } else {
                    // Apply TZ to the live session. start_sta() sets TZ from
                    // NVS on every warm boot, but the first-provision path
                    // never calls start_sta() — it goes Validating -> Online
                    // directly. Without this, localtime_r returns UTC until
                    // the next reboot.
                    setenv("TZ", pending.tz.c_str(), 1);
                    tzset();
                    pending = {};
                    sta_backoff_ms = STA_BACKOFF_INITIAL_MS;
                    sm.handle(Event::ValidationSucceeded);
                }
            } else if (now - validating_started_at > VALIDATING_TIMEOUT_MS) {
                Serial.println("[wifi_provision] validation timeout; back to captive");
                fail_validation("Didn't connect. Check your password and try again.");
            }
            break;
        }
        case State::Online: {
            if (WiFi.status() != WL_CONNECTED) {
                Serial.println("[wifi_provision] WiFi dropped");
                sta_backoff_ms = STA_BACKOFF_INITIAL_MS;
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
    if (!nvs_store::clear()) {
        Serial.println("[wifi_provision] WARNING: NVS clear failed; "
                       "credentials may persist across the restart");
        Serial.flush();
    }
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
