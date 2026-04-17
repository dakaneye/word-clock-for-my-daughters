// firmware/lib/wifi_provision/src/web_server.cpp
// Guarded with #ifdef ARDUINO so PlatformIO LDF does not try to compile
// this TU in the native test build. Same pattern as nvs_store.cpp and
// dns_wrapper.cpp.
#ifdef ARDUINO

#include <WebServer.h>
#include <WiFi.h>
#include <esp_system.h>
#include <functional>
#include <string>
#include "wifi_provision/form_parser.h"
#include "wifi_provision/credential_validator.h"
#include "form_html.h"

namespace wc::wifi_provision::web {

using SubmitHandler = std::function<void(const FormBody&)>;
using ConfirmationStatus = std::function<std::string()>;

static WebServer& server() {
    static WebServer s(80);
    return s;
}

static std::string current_csrf;
static std::string last_error;
static int submit_count = 0;
static constexpr int MAX_SUBMITS = 5;           // spec: per-AP-session rate limit
static uint32_t last_scan_started_at = 0;
static constexpr uint32_t SCAN_COOLDOWN_MS = 10000;
static SubmitHandler on_submit;
static ConfirmationStatus get_status;

static std::string rand_hex(size_t bytes) {
    std::string out;
    out.reserve(bytes * 2);
    for (size_t i = 0; i < bytes; ++i) {
        char buf[3];
        snprintf(buf, sizeof(buf), "%02x", (uint8_t)(esp_random() & 0xFF));
        out += buf;
    }
    return out;
}

static std::string replace_all(std::string s, const std::string& from, const std::string& to) {
    size_t pos = 0;
    while ((pos = s.find(from, pos)) != std::string::npos) {
        s.replace(pos, from.size(), to);
        pos += to.size();
    }
    return s;
}

// Minimal JSON string escape: quote, backslash, control characters.
// Defense-in-depth for /status and /scan responses — if a future change
// feeds user-controlled text through these endpoints (e.g. a "connected
// to <SSID>" message), this keeps the JSON well-formed.
static std::string json_escape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 2);
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out += c;
                }
        }
    }
    return out;
}

static std::string render_form() {
    current_csrf = rand_hex(16);
    std::string html(FORM_HTML);
    html = replace_all(html, "{{CSRF_TOKEN}}", current_csrf);
    html = replace_all(html, "{{ERROR_MESSAGE}}", last_error);
    last_error.clear();
    return html;
}

// Common helper for the two paths where a 302 redirect to "/" is the
// correct response (iOS captive probe + wildcard catch-all). For error
// responses on /submit, the spec says 400 with the form body — use
// render_form() + server().send(400, ...) instead.
static void redirect_to_root() {
    server().sendHeader("Location", "/", true);
    server().send(302, "text/plain", "");
}

static void handle_root() {
    server().send(200, "text/html", render_form().c_str());
}

static void handle_ios_probe() {
    // iOS probes captive.apple.com/hotspot-detect.html for a <TITLE>Success</TITLE>.
    // A redirect here triggers the captive-portal auto-popup.
    redirect_to_root();
}

static void handle_scan() {
    std::string json = "[";
    int n = WiFi.scanComplete();
    const uint32_t now = millis();
    const bool cooldown_elapsed =
        last_scan_started_at == 0 || (now - last_scan_started_at) > SCAN_COOLDOWN_MS;

    if (n < 0) {
        // No scan complete yet — kick one off if cooldown allows.
        if (cooldown_elapsed) {
            WiFi.scanNetworks(/* async = */ true);
            last_scan_started_at = now;
        }
        server().send(200, "application/json", "[]");
        return;
    }
    for (int i = 0; i < n; ++i) {
        if (i > 0) json += ",";
        std::string ssid = WiFi.SSID(i).c_str();
        int rssi = WiFi.RSSI(i);
        bool secured = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
        json += "{\"ssid\":\"";
        json += json_escape(ssid);
        json += "\",\"rssi\":";
        json += std::to_string(rssi);
        json += ",\"secured\":";
        json += secured ? "true" : "false";
        json += "}";
    }
    json += "]";
    // scanDelete() frees the result buffer from the just-completed scan
    // unconditionally — it's independent of whether we start a new scan.
    WiFi.scanDelete();
    // Only kick off a fresh async scan if the cooldown window has elapsed.
    // Protects the 2.4 GHz radio from /scan spam by a misbehaving client
    // (or an iOS captive-portal view that polls aggressively).
    if (cooldown_elapsed) {
        WiFi.scanNetworks(/* async = */ true);
        last_scan_started_at = now;
    }
    server().send(200, "application/json", json.c_str());
}

static void handle_submit() {
    if (submit_count >= MAX_SUBMITS) {
        // Spec: 429 on rate limit. Defense-in-depth per §Scope.
        server().send(429, "text/plain",
                      "Too many tries. Hold Hour + Audio on the clock for "
                      "10 seconds to start fresh.");
        return;
    }

    // Read fields directly from WebServer (which has already URL-decoded
    // them). Avoids the earlier round-trip-through-reconstructed-body bug
    // that corrupted SSIDs containing '&' / '=' / '+' / '%'.
    FormBody parsed;
    parsed.ssid = server().arg("ssid").c_str();
    parsed.pw   = server().arg("pw").c_str();
    parsed.tz   = server().arg("tz").c_str();
    parsed.csrf = server().arg("csrf").c_str();

    // CSRF: reject if the server-side token was never set (defense against
    // a POST before any GET /) OR the submitted token doesn't match.
    if (current_csrf.empty() || parsed.csrf != current_csrf) {
        last_error = "Let's try that again — the form timed out.";
        // Spec §HTTP endpoints: 400 on validation failure. Respond with
        // the form body so the user sees the error inline and gets a
        // fresh CSRF token in one round-trip.
        server().send(400, "text/html", render_form().c_str());
        return;
    }

    ValidationResult v = validate(parsed);
    if (!v.ok) {
        last_error = v.message;
        server().send(400, "text/html", render_form().c_str());
        return;
    }

    submit_count++;
    on_submit(parsed);
    std::string msg =
        "<!doctype html><html><body style='font-family:Georgia,serif;padding:2rem'>"
        "<h1>Press the Audio button on the clock</h1>"
        "<p>Press and release the Audio button within 60 seconds to confirm.</p>"
        "<p id='s'>Waiting…</p>"
        "<script>"
        "setInterval(async()=>{const r=await fetch('/status');"
        "const j=await r.json();document.getElementById('s').textContent=j.message;"
        "},2000);"
        "</script></body></html>";
    server().send(200, "text/html", msg.c_str());
}

static void handle_status() {
    std::string status = get_status();
    std::string json = "{\"message\":\"" + json_escape(status) + "\"}";
    server().send(200, "application/json", json.c_str());
}

static void handle_wildcard() {
    redirect_to_root();
}

void begin(SubmitHandler submit_cb, ConfirmationStatus status_cb) {
    on_submit = std::move(submit_cb);
    get_status = std::move(status_cb);
    submit_count = 0;
    last_error.clear();
    last_scan_started_at = 0;
    // Seed a CSRF token immediately so the first /submit can't slip past
    // the empty-string comparison.
    current_csrf = rand_hex(16);

    server().on("/", HTTP_GET, handle_root);
    server().on("/submit", HTTP_POST, handle_submit);
    server().on("/scan", HTTP_GET, handle_scan);
    server().on("/status", HTTP_GET, handle_status);
    server().on("/hotspot-detect.html", HTTP_GET, handle_ios_probe);
    server().onNotFound(handle_wildcard);
    server().begin();

    // Pre-seed the scan so the dropdown populates immediately.
    WiFi.scanNetworks(/* async = */ true);
    last_scan_started_at = millis();
}

void loop() {
    server().handleClient();
}

void stop() {
    server().stop();
}

} // namespace wc::wifi_provision::web

#endif // ARDUINO
