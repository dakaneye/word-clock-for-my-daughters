// firmware/lib/wifi_provision/src/wifi_provision.cpp
// Guarded with #ifdef ARDUINO — same pattern as nvs_store.cpp / dns_wrapper.cpp /
// web_server.cpp. PlatformIO LDF would otherwise compile this TU in the
// native test build where <WiFi.h> etc. don't exist.
#ifdef ARDUINO

#include <WiFi.h>
#include <IPAddress.h>
#include <time.h>
#include "wifi_provision.h"
#include "wifi_provision/state_machine.h"
#include "wifi_provision/form_parser.h"

// Forward decls to the adapter namespaces (defined in sibling .cpp files).
namespace wc::wifi_provision::nvs_store {
    bool has_credentials();
    struct StoredCredentials { String ssid; String pw; String tz; };
    StoredCredentials read();
    void write(const FormBody& body);
    void touch_last_sync(uint64_t unix_seconds);
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

// Timers (all in millis). 0 means "not armed".
static uint32_t ap_started_at = 0;
static uint32_t awaiting_started_at = 0;
static uint32_t validating_started_at = 0;
static uint32_t last_sta_attempt_at = 0;
static uint32_t sta_backoff_ms = 2000;

// In-flight credentials held while awaiting Audio-button confirmation or
// doing a trial connection. Never written to NVS until Validating succeeds.
static FormBody pending;

static constexpr uint32_t AP_TIMEOUT_MS          = 10UL * 60UL * 1000UL;
static constexpr uint32_t CONFIRMATION_TIMEOUT_MS = 60UL * 1000UL;
static constexpr uint32_t VALIDATING_TIMEOUT_MS   = 30UL * 1000UL;

static std::string confirmation_message() {
    switch (sm.state()) {
        case State::AwaitingConfirmation: return "Waiting for Audio button…";
        case State::Validating:           return "Connecting…";
        case State::Online:               return "Connected!";
        default:                          return "Ready.";
    }
}

static void start_ap() {
    WiFi.mode(WIFI_AP);
    uint64_t mac = ESP.getEfuseMac();
    char ssid[32];
    snprintf(ssid, sizeof(ssid), "WordClock-Setup-%04X",
             (uint16_t)((mac >> 32) & 0xFFFF));
    WiFi.softAP(ssid, /* password = */ nullptr, /* channel = */ 1,
                /* ssid_hidden = */ 0, /* max_connection = */ 1);
    IPAddress ip = WiFi.softAPIP();
    dns_hijack::begin(ip);
    web::begin(
        [](const FormBody& body) {
            pending = body;
            sm.handle(Event::FormSubmitted);
            awaiting_started_at = millis();
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
}

static void start_validating() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(pending.ssid.c_str(), pending.pw.c_str());
    validating_started_at = millis();
}

void begin() {
    if (nvs_store::has_credentials()) {
        sm.handle(Event::BootWithCredentials);
        start_sta();
    } else {
        sm.handle(Event::BootWithNoCredentials);
        start_ap();
    }
}

void loop() {
    const uint32_t now = millis();

    switch (sm.state()) {
        case State::StaConnecting: {
            if (WiFi.status() == WL_CONNECTED) {
                sm.handle(Event::StaConnected);
                // NTP sync happens in main.cpp's ntp module; touch_last_sync on success.
            } else if (now - last_sta_attempt_at > sta_backoff_ms) {
                sta_backoff_ms = std::min<uint32_t>(sta_backoff_ms * 2, 5 * 60 * 1000);
                start_sta();
            }
            break;
        }
        case State::ApActive: {
            dns_hijack::loop();
            web::loop();
            if (now - ap_started_at > AP_TIMEOUT_MS) {
                sm.handle(Event::ApTimeout);
                stop_ap();
            }
            break;
        }
        case State::AwaitingConfirmation: {
            dns_hijack::loop();
            web::loop();
            if (now - awaiting_started_at > CONFIRMATION_TIMEOUT_MS) {
                sm.handle(Event::ConfirmationTimeout);
                pending = {};
            }
            break;
        }
        case State::Validating: {
            if (WiFi.status() == WL_CONNECTED) {
                nvs_store::write(pending);
                pending = {};
                stop_ap();
                sm.handle(Event::ValidationSucceeded);
            } else if (now - validating_started_at > VALIDATING_TIMEOUT_MS) {
                pending = {};
                WiFi.disconnect(true);
                // Re-enter AP to show the error; web_server's last_error is set
                // by the validator before we get here only on syntactic errors,
                // not on connection failure — set a user-visible message now.
                sm.handle(Event::ValidationFailed);
                start_ap();
            }
            break;
        }
        case State::Online: {
            if (WiFi.status() != WL_CONNECTED) {
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
    nvs_store::clear();
    sm.handle(Event::ResetCombo);
    start_ap();
}

void confirm_audio() {
    sm.handle(Event::AudioButtonConfirmed);
}

} // namespace wc::wifi_provision

#endif // ARDUINO
