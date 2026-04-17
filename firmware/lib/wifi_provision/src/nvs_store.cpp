// firmware/lib/wifi_provision/src/nvs_store.cpp
//
// ESP32-only adapter. The native unit-test environment does not link against
// the Arduino framework, so this translation unit compiles to nothing there.
// The platformio.ini `build_src_filter` only controls `src_dir` — library
// sources like this one are picked up by LDF, which is why the guard is here.
#ifdef ARDUINO

#include <Preferences.h>
#include <cstdint>
#include <string>
#include "wifi_provision/form_parser.h"

namespace wc::wifi_provision::nvs_store {

static constexpr const char* NAMESPACE = "wifi";

static Preferences& prefs() {
    static Preferences p;
    return p;
}

bool open_readable() {
    return prefs().begin(NAMESPACE, /* readOnly = */ true);
}

bool open_writable() {
    return prefs().begin(NAMESPACE, /* readOnly = */ false);
}

void close() {
    prefs().end();
}

bool has_credentials() {
    if (!open_readable()) return false;
    bool has = prefs().isKey("ssid");
    close();
    return has;
}

struct StoredCredentials {
    String ssid;
    String pw;
    String tz;
};

StoredCredentials read() {
    StoredCredentials out;
    if (!open_readable()) return out;
    out.ssid = prefs().getString("ssid", "");
    out.pw   = prefs().getString("pw", "");
    out.tz   = prefs().getString("tz", "PST8PDT,M3.2.0,M11.1.0");
    close();
    return out;
}

void write(const FormBody& body) {
    if (!open_writable()) return;
    prefs().putString("ssid", body.ssid.c_str());
    prefs().putString("pw",   body.pw.c_str());
    prefs().putString("tz",   body.tz.c_str());
    close();
}

void touch_last_sync(uint64_t unix_seconds) {
    if (!open_writable()) return;
    prefs().putULong64("last_sync", unix_seconds);
    close();
}

uint64_t last_sync() {
    if (!open_readable()) return 0;
    uint64_t v = prefs().getULong64("last_sync", 0);
    close();
    return v;
}

void clear() {
    if (!open_writable()) return;
    prefs().clear();
    close();
}

} // namespace wc::wifi_provision::nvs_store

#endif // ARDUINO
