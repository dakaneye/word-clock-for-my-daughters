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
static constexpr int MAX_SUBMITS = 5;
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

static std::string render_form() {
    current_csrf = rand_hex(16);
    std::string html(FORM_HTML);
    html = replace_all(html, "{{CSRF_TOKEN}}", current_csrf);
    html = replace_all(html, "{{ERROR_MESSAGE}}", last_error);
    last_error.clear();
    return html;
}

static void handle_root() {
    server().send(200, "text/html", render_form().c_str());
}

static void handle_ios_probe() {
    // iOS probes captive.apple.com/hotspot-detect.html for a <TITLE>Success</TITLE>.
    // Returning a redirect instead triggers the captive-portal popup.
    server().sendHeader("Location", "/", true);
    server().send(302, "text/plain", "");
}

static void handle_scan() {
    std::string json = "[";
    int n = WiFi.scanComplete();
    if (n < 0) {
        WiFi.scanNetworks(/* async = */ true);
        server().send(200, "application/json", "[]");
        return;
    }
    for (int i = 0; i < n; ++i) {
        if (i > 0) json += ",";
        String ssid = WiFi.SSID(i);
        ssid.replace("\\", "\\\\");
        ssid.replace("\"", "\\\"");
        int rssi = WiFi.RSSI(i);
        bool secured = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
        json += "{\"ssid\":\"";
        json += ssid.c_str();
        json += "\",\"rssi\":";
        json += std::to_string(rssi);
        json += ",\"secured\":";
        json += secured ? "true" : "false";
        json += "}";
    }
    json += "]";
    WiFi.scanDelete();
    WiFi.scanNetworks(true);
    server().send(200, "application/json", json.c_str());
}

static void handle_submit() {
    if (submit_count >= MAX_SUBMITS) {
        server().send(429, "text/plain",
                      "Too many attempts. Reset the clock and try again.");
        return;
    }
    std::string body = server().arg("plain").c_str();
    if (body.empty()) {
        // arg("plain") is empty when the body is form-encoded; reconstruct it.
        body.clear();
        for (int i = 0; i < server().args(); ++i) {
            if (!body.empty()) body += "&";
            body += server().argName(i).c_str();
            body += "=";
            body += server().arg(i).c_str();
        }
    }
    FormBody parsed = parse_form_body(body);
    if (parsed.csrf != current_csrf) {
        last_error = "Session expired — please resubmit.";
        server().sendHeader("Location", "/", true);
        server().send(302, "text/plain", "");
        return;
    }
    ValidationResult v = validate(parsed);
    if (!v.ok) {
        last_error = v.message;
        server().sendHeader("Location", "/", true);
        server().send(302, "text/plain", "");
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
    std::string json = "{\"message\":\"" + status + "\"}";
    server().send(200, "application/json", json.c_str());
}

static void handle_wildcard() {
    server().sendHeader("Location", "/", true);
    server().send(302, "text/plain", "");
}

void begin(SubmitHandler submit_cb, ConfirmationStatus status_cb) {
    on_submit = std::move(submit_cb);
    get_status = std::move(status_cb);
    submit_count = 0;
    last_error.clear();

    server().on("/", HTTP_GET, handle_root);
    server().on("/submit", HTTP_POST, handle_submit);
    server().on("/scan", HTTP_GET, handle_scan);
    server().on("/status", HTTP_GET, handle_status);
    server().on("/hotspot-detect.html", HTTP_GET, handle_ios_probe);
    server().onNotFound(handle_wildcard);
    server().begin();

    // Pre-seed the scan so the dropdown populates immediately.
    WiFi.scanNetworks(/* async = */ true);
}

void loop() {
    server().handleClient();
}

void stop() {
    server().stop();
}

} // namespace wc::wifi_provision::web

#endif // ARDUINO
