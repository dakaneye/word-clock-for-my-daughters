// firmware/tools/sd_loader/src/main.cpp
//
// SD LOADER — temporary firmware for cable-only SD content updates on a
// sealed clock. Flashed by tools/sd_load.py, which restores the original
// flash image afterwards. Serves line-based serial commands at 115200:
//
//   W <ssid>\t<pass>                join WiFi with explicit credentials
//   G <url>\t<name>\t<size>\t<crc>  download url -> /<name> (atomic)
//   L                               list SD root (name, size, crc32)
//   D <name>                        delete /<name>
//
// Protocol spec: docs/superpowers/specs/2026-07-13-sd-loader-design.md.
// Reads the clock's stored WiFi credentials from NVS (namespace "wifi",
// same schema as lib/wifi_provision/src/nvs_store.cpp). NEVER writes NVS.
#include <Arduino.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <SD.h>
#include <WiFi.h>
#include "pinmap.h"

static bool sd_ok = false;

// ---- CRC32 (zlib polynomial, init 0) --------------------------------
static uint32_t crc_table[256];

static void crc_init() {
    for (uint32_t i = 0; i < 256; ++i) {
        uint32_t c = i;
        for (int k = 0; k < 8; ++k)
            c = (c & 1) ? (0xEDB88320UL ^ (c >> 1)) : (c >> 1);
        crc_table[i] = c;
    }
}

// zlib-style: pass the previous return value back in (start with 0).
static uint32_t crc_update(uint32_t crc, const uint8_t* buf, size_t len) {
    crc ^= 0xFFFFFFFFUL;
    for (size_t i = 0; i < len; ++i)
        crc = crc_table[(crc ^ buf[i]) & 0xFF] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFUL;
}

static uint32_t crc_of_file(const char* path) {
    File f = SD.open(path, FILE_READ);
    if (!f) return 0;
    static uint8_t buf[4096];
    uint32_t crc = 0;
    while (true) {
        int n = f.read(buf, sizeof(buf));
        if (n <= 0) break;
        crc = crc_update(crc, buf, (size_t)n);
    }
    f.close();
    return crc;
}

// ---- WiFi ------------------------------------------------------------
static bool wifi_join(const String& ssid, const String& pw,
                      uint32_t timeout_ms = 20000) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), pw.c_str());
    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - start > timeout_ms) return false;
        delay(100);
    }
    return true;
}

// Distinguishes "no usable credentials" from "credentials present but the
// join failed" so the READY line can report wifi=none vs wifi=failed —
// the operator's next step differs (supply --wifi-ssid vs check the router).
static const char* wifi_fail_state = "none";

static bool wifi_from_nvs() {
    Preferences p;
    if (!p.begin("wifi", /*readOnly=*/true)) return false;
    // Same gate as nvs_store.cpp has_credentials(): the valid sentinel AND
    // the schema version. A future NVS schema bump must not let this
    // loader trust a differently-shaped credential record.
    bool valid = p.getUChar("valid", 0) == 1 &&
                 p.getUChar("schema_ver", 0) == 1;
    String ssid = p.getString("ssid", "");
    String pw = p.getString("pw", "");
    p.end();
    if (!valid || ssid.isEmpty()) return false;
    wifi_fail_state = "failed";       // creds exist; a miss is a join failure
    return wifi_join(ssid, pw);
}

// ---- Commands --------------------------------------------------------
// Split a tab-separated tail into at most n parts (last part keeps tabs).
static int split_tabs(String tail, String* out, int n) {
    int count = 0;
    while (count < n - 1) {
        int t = tail.indexOf('\t');
        if (t < 0) break;
        out[count++] = tail.substring(0, t);
        tail = tail.substring(t + 1);
    }
    out[count++] = tail;
    return count;
}

static void cmd_get(const String& tail) {
    String part[4];
    if (split_tabs(tail, part, 4) != 4) {
        Serial.println("ERR get expected url\\tname\\tsize\\tcrc");
        return;
    }
    const String& url = part[0];
    const String name = "/" + part[1];
    const String temp = name + ".part";
    const uint32_t want_size = strtoul(part[2].c_str(), nullptr, 10);
    const uint32_t want_crc = strtoul(part[3].c_str(), nullptr, 16);

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("ERR get wifi not connected");
        return;
    }
    HTTPClient http;
    http.setTimeout(15000);
    if (!http.begin(url)) {
        Serial.println("ERR get bad url");
        return;
    }
    int code = http.GET();
    if (code != 200) {
        Serial.printf("ERR get http %d\n", code);
        http.end();
        return;
    }
    SD.remove(temp);
    File out = SD.open(temp, FILE_WRITE);
    if (!out) {
        Serial.println("ERR get sd open failed");
        http.end();
        return;
    }
    WiFiClient* stream = http.getStreamPtr();
    static uint8_t buf[4096];
    uint32_t got = 0, crc = 0, last_pct = 0;
    bool io_fail = false;
    while (got < want_size) {
        size_t want = min((uint32_t)sizeof(buf), want_size - got);
        int n = stream->readBytes(buf, want);   // blocks up to timeout
        if (n <= 0) break;                       // stalled / closed early
        if (out.write(buf, (size_t)n) != (size_t)n) {
            io_fail = true;
            break;
        }
        crc = crc_update(crc, buf, (size_t)n);
        got += (uint32_t)n;
        uint32_t pct = (uint32_t)((uint64_t)got * 100 / want_size);
        if (pct >= last_pct + 5) {
            Serial.printf("PROG %s %u\n", part[1].c_str(), (unsigned)pct);
            last_pct = pct;
        }
    }
    out.close();
    http.end();
    if (io_fail || got != want_size) {
        SD.remove(temp);
        Serial.printf("ERR get short transfer %u/%u%s\n", (unsigned)got,
                      (unsigned)want_size, io_fail ? " (sd write)" : "");
        return;
    }
    if (crc != want_crc) {
        SD.remove(temp);
        Serial.println("ERR get stream crc mismatch");
        return;
    }
    // Read back from the card — proves the bytes that LANDED are right.
    uint32_t disk_crc = crc_of_file(temp.c_str());
    if (disk_crc != want_crc) {
        SD.remove(temp);
        Serial.println("ERR get readback crc mismatch");
        return;
    }
    // Swap the verified .part over the target. NOT atomic: this SD lib's
    // rename can't overwrite, so remove must come first, and a power cut
    // in that millisecond window leaves the slot empty (recover by
    // re-running the load). Everything before this point leaves the
    // original file untouched.
    SD.remove(name);
    if (!SD.rename(temp, name)) {
        Serial.println("ERR get rename failed");
        return;
    }
    Serial.printf("OK got %s %u %08x\n", part[1].c_str(),
                  (unsigned)want_size, (unsigned)want_crc);
}

static void cmd_list() {
    File root = SD.open("/");
    if (!root) {
        Serial.println("ERR list sd root");
        return;
    }
    for (File f = root.openNextFile(); f; f = root.openNextFile()) {
        if (!f.isDirectory()) {
            String path = String("/") + f.name();
            uint32_t size = f.size();
            f.close();
            Serial.printf("FILE %s %u %08x\n", path.c_str() + 1,
                          (unsigned)size,
                          (unsigned)crc_of_file(path.c_str()));
        } else {
            f.close();
        }
    }
    root.close();
    Serial.println("OK list");
}

static void handle(const String& line) {
    if (line.startsWith("W ")) {
        String part[2];
        if (split_tabs(line.substring(2), part, 2) != 2) {
            Serial.println("ERR wifi expected ssid\\tpass");
        } else if (wifi_join(part[0], part[1])) {
            Serial.printf("OK wifi %s\n", WiFi.localIP().toString().c_str());
        } else {
            Serial.println("ERR wifi join failed");
        }
    } else if (line.startsWith("G ")) {
        cmd_get(line.substring(2));
    } else if (line == "L") {
        cmd_list();
    } else if (line.startsWith("D ")) {
        String name = "/" + line.substring(2);
        if (SD.remove(name)) Serial.printf("OK del %s\n", name.c_str() + 1);
        else Serial.printf("ERR del %s\n", name.c_str() + 1);
    } else if (line.length() > 0) {
        Serial.println("ERR unknown command");
    }
}

void setup() {
    Serial.begin(115200);
    crc_init();
    sd_ok = SD.begin(PIN_SD_CS);
    bool wifi = wifi_from_nvs();
    Serial.printf("READY sd=%s wifi=%s\n", sd_ok ? "ok" : "fail",
                  wifi ? WiFi.localIP().toString().c_str()
                       : wifi_fail_state);
}

void loop() {
    static String line;
    while (Serial.available()) {
        char c = (char)Serial.read();
        if (c == '\n') {
            line.trim();
            handle(line);
            line = "";
        } else {
            line += c;
        }
    }
    delay(2);
}
