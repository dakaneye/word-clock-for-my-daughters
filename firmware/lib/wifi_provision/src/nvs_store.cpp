// firmware/lib/wifi_provision/src/nvs_store.cpp
//
// ESP32-only adapter. The native unit-test environment does not link against
// the Arduino framework, so this translation unit compiles to nothing there.
// The platformio.ini `build_src_filter` only controls `src_dir` — library
// sources like this one are picked up by LDF, which is why the guard is here.
//
// Schema notes (see NVS schema section of the captive-portal spec):
//   namespace "wifi"
//     "ssid"         String
//     "pw"           String
//     "tz"           String  (POSIX TZ, e.g. "PST8PDT,M3.2.0,M11.1.0")
//     "last_sync"    ULong64 (Unix seconds)
//     "schema_ver"   UChar   (current: 1)
//     "valid"        UChar   (1 iff the credential triple landed atomically;
//                             written LAST during write(), cleared FIRST
//                             during clear(). has_credentials() checks this.)
#ifdef ARDUINO

#include <Preferences.h>
#include <cstdint>
#include <string>
#include "wifi_provision/form_parser.h"

namespace wc::wifi_provision::nvs_store {

static constexpr const char* NAMESPACE   = "wifi";
static constexpr uint8_t     SCHEMA_VER  = 1;
static constexpr uint8_t     VALID_TRUE  = 1;

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

// True only if the most recent write() completed atomically (valid sentinel
// set last) AND the schema version matches. Prevents stuck-in-StaConnecting
// after a partial NVS write (spec: validation before commit).
bool has_credentials() {
    if (!open_readable()) return false;
    uint8_t schema = prefs().getUChar("schema_ver", 0);
    uint8_t valid  = prefs().getUChar("valid",      0);
    close();
    return (schema == SCHEMA_VER) && (valid == VALID_TRUE);
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
    out.pw   = prefs().getString("pw",   "");
    out.tz   = prefs().getString("tz",   "PST8PDT,M3.2.0,M11.1.0");
    close();
    return out;
}

// Atomic write: clear the valid sentinel first (so a mid-write crash leaves
// has_credentials()==false), then write the triple + schema version, then
// write the sentinel LAST. Returns true iff the sentinel write succeeded.
//
// Per-key return values from Preferences::putString are not checked because
// an empty password (open network) legitimately returns length 0, which is
// indistinguishable from failure. The sentinel write at the end is the
// single commit point: if we got here without a crash, the triple is
// already in flash; the sentinel either lands or NVS is flat-out broken.
bool write(const FormBody& body) {
    if (!open_writable()) return false;
    prefs().putUChar("valid", 0);                           // invalidate first
    prefs().putString("ssid",      body.ssid.c_str());
    prefs().putString("pw",        body.pw.c_str());
    prefs().putString("tz",        body.tz.c_str());
    prefs().putUChar ("schema_ver", SCHEMA_VER);
    size_t sentinel = prefs().putUChar("valid", VALID_TRUE); // commit last
    close();
    return sentinel > 0;
}

// Internal — external callers go through wc::wifi_provision::touch_last_sync.
bool touch_last_sync(uint64_t unix_seconds) {
    if (!open_writable()) return false;
    bool ok = prefs().putULong64("last_sync", unix_seconds) > 0;
    close();
    return ok;
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
