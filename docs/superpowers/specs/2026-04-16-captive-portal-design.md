# Captive Portal — Design Spec

Date: 2026-04-16
Status: Design locked, ready for implementation planning

## Overview

The `wifi_provision` firmware module handles first-boot WiFi setup and subsequent re-provisioning on demand for the daughters' word clocks. It is a **custom** implementation — no WiFiManager or other third-party library — built on the Arduino-ESP32 core primitives (`WiFi`, `DNSServer`, `WebServer`, `Preferences`).

The design is tuned for a one-time-per-decade setup event (unboxing at ages 7 and 9, occasional re-provisioning after a move), with defense-in-depth security via a short AP lifetime, per-AP-session rate limits, physical-button confirmation before credentials commit, and connection validation before NVS write.

## Success criteria

- Dad unboxes clock → plugs in → connects iPhone to `WordClock-Setup-XXXX` → enters home WiFi SSID + password + timezone → presses Audio button on the clock within 60 seconds → clock associates with his WiFi, NTP-syncs, and begins normal operation. Under 2 minutes from power-on to lit face.
- When a daughter relocates in 2030+, Hour + Audio held for 10 seconds clears the stored credentials and re-enters the captive portal for a fresh setup with new SSID / password / timezone.
- Clock credentials persist across power cycles and firmware updates via `Preferences` NVS. No silent time-based expiry.
- If NTP sync has not succeeded in the last 24 hours (including before first setup), lit words display an amber tint instead of warm white — a visible signal that the RTC is running blind.
- HTML/CSS of the setup form is editable in a standalone browser for rapid iteration, with a build-time generator that embeds the finalized HTML as a C++ `const char[]`.

## User flow

### First boot (no stored credentials)

1. Clock powers on. Firmware reads NVS, finds no `ssid` key → enters captive portal mode.
2. ESP32 brings up `SoftAP` with SSID `WordClock-Setup-XXXX` (last 4 hex digits of the chip's base MAC), **open** (no password), 2.4 GHz channel 1, default IP `192.168.4.1`, max 1 client.
3. `DNSServer` on port 53 responds to every query with `192.168.4.1` (wildcard DNS hijack).
4. `WebServer` on port 80 serves the setup form at `/` and at the iOS captive-portal probe URL `/hotspot-detect.html`.
5. User connects iPhone to `WordClock-Setup-XXXX`. iOS hits `captive.apple.com/hotspot-detect.html`, the clock's DNS lie + HTTP redirect triggers the OS captive-portal popup. Safari opens with the form.
6. User picks their home SSID from the scanned dropdown, types the WiFi password, selects a timezone (Pacific pre-selected), submits.
7. Clock validates CSRF token, parses the form body, enters `AWAITING_CONFIRMATION` state. Response page renders: "Press the Audio button on the clock to confirm." The page polls `/status` every 2 seconds.
8. User walks to clock, presses the Audio tact switch within 60 seconds.
9. Clock enters `VALIDATING` state: attempts `WiFi.begin(ssid, pw)` in STA mode with the in-memory credentials. Has a 30-second timeout to tolerate Nest WiFi's first-associate latency.
10. On successful DHCP → write `ssid`, `pw`, `tz`, `last_sync=0` to NVS → shut down AP + DNS server → trigger NTP sync → `last_sync` updated → state `ONLINE`. Clock face lights up.
11. On failed association → discard in-memory creds → return to `AP_ACTIVE` with an error message on the form page: "Could not connect — check password and try again."

### Warm boot (credentials exist)

1. Clock powers on. Firmware reads NVS, finds `ssid` + `pw` + `tz`.
2. `WiFi.mode(WIFI_STA)`, `setenv("TZ", tz_string, 1)`, `tzset()`, `WiFi.begin(ssid, pw)`.
3. On connect → NTP sync → `ONLINE`. On fail → retry indefinitely with exponential backoff (2 s, 4 s, 8 s, …, capped at 5 min). DS3231 drives the clock face throughout; amber tint applied once 24 hours have elapsed since `last_sync`.

### Reset flow

1. User holds Hour + Audio tact switches simultaneously for 10 seconds during any normal-operation state (`ONLINE` or `STA_CONNECTING` or `IDLE_NO_CREDENTIALS`).
2. Firmware clears the `wifi` NVS namespace via `Preferences::clear()` → `ESP.restart()`.
3. Next boot finds no `ssid` → first-boot flow.

### AP timeout

If the clock has been in `AP_ACTIVE` state for 10 continuous minutes without a successful commit, the AP shuts down and the state transitions to `IDLE_NO_CREDENTIALS` (DS3231 drives the clock face with amber tint). User must hold the reset button combo to re-enter the captive portal. Prevents an indefinitely-broadcasting open AP if the clock is powered on without being set up.

## State machine

```
BOOT
  ├─ if NVS has wifi_ssid + wifi_pw → STATE = STA_CONNECTING
  └─ else                           → STATE = AP_ACTIVE

STA_CONNECTING
  │ (tries stored SSID with exponential backoff, DS3231 drives face throughout)
  ├─ on connect        → STATE = ONLINE, trigger NTP sync
  └─ on persistent fail → retry forever (no auto-fallback to AP)

AP_ACTIVE
  │ (SoftAP up, DNSServer + WebServer serving form)
  ├─ form submit (validated)  → STATE = AWAITING_CONFIRMATION
  ├─ 10 min elapsed no commit → STATE = IDLE_NO_CREDENTIALS

AWAITING_CONFIRMATION
  │ (held in-memory creds; "press Audio button" page being polled)
  ├─ Audio button pressed within 60 s → STATE = VALIDATING
  └─ 60 s timeout                     → discard creds, STATE = AP_ACTIVE

VALIDATING
  │ (WiFi.begin(in_memory_ssid, in_memory_pw) with 30 s timeout)
  ├─ WL_CONNECTED within 30 s → commit to NVS, shut down AP + DNS,
  │                              trigger NTP, STATE = ONLINE
  └─ timeout or error         → discard creds, STATE = AP_ACTIVE
                                 (form page shows error message)

ONLINE
  │ (normal clock operation; NTP re-sync every 6 h updates last_sync)
  ├─ WiFi drops                       → STATE = STA_CONNECTING
  └─ Hour + Audio held 10 s           → clear NVS, ESP.restart

IDLE_NO_CREDENTIALS
  │ (AP timed out; DS3231 drives face with amber tint)
  └─ Hour + Audio held 10 s           → clear NVS (already clear),
                                         ESP.restart, back to AP_ACTIVE
```

## Module layout

```
firmware/
  lib/wifi_provision/
    include/wifi_provision.h       # public API: begin(), loop(), state(), reset_to_captive()
    src/wifi_provision.cpp         # state machine, WiFi STA/AP management, NVS RW
    src/web_server.cpp             # HTTP routes, form parsing, CSRF token
    src/dns_server.cpp             # thin wrapper over DNSServer
    src/form_html.h                # .gitignore'd — generated at build time
  assets/captive-portal/
    form.html.template             # source HTML with {{KID_NAME}}, {{KID_ACCENT_COLOR}},
                                   #              {{KID_BG_COLOR}}, {{TZ_OPTIONS}} tokens
    form.css                       # shared styles (inlined at build time)
    gen_form_html.py               # build-time generator (pre: extra_script in PlatformIO)
  test/
    wifi_provision/
      form_parse_test.cpp
      validate_credentials_test.cpp
      tz_string_lookup_test.cpp
      state_machine_test.cpp
      html_gen_test.py             # pytest, runs the generator + asserts output
```

## AP configuration

| Parameter | Value |
|---|---|
| SSID | `WordClock-Setup-XXXX` (last 4 hex of base MAC) |
| Security | Open |
| Channel | 1 |
| IP | `192.168.4.1` |
| Subnet | `255.255.255.0`, DHCP `192.168.4.2–254` |
| Max clients | 1 |
| Band | 2.4 GHz (ESP32-WROOM-32 is 2.4 GHz only) |

Lifetime: AP is up only during `AP_ACTIVE` or `AWAITING_CONFIRMATION` states. Shut down immediately upon successful NVS commit or after the 10-minute timeout. Never broadcasts during normal operation.

## HTTP endpoints

| Method + Path | Purpose | Response |
|---|---|---|
| `GET /` | Serve setup form | 200 + embedded HTML with CSRF token |
| `GET /hotspot-detect.html` | iOS captive-portal probe | 302 → `/` (triggers OS popup) |
| `GET /scan` | Asynchronous SSID scan | 200 + JSON `[{ssid, rssi, secured}, ...]` |
| `POST /submit` | Form submission | 200 + "press Audio button" page, or 400 on validation failure, or 429 on rate limit |
| `GET /status` | Status poll (used by client JS) | 200 + JSON `{state, message}` |
| `GET /*` (wildcard) | Anything else | 302 → `/` |

**CSRF:** every `GET /` embeds a random 128-bit token generated from `esp_random()`. `POST /submit` must present the matching token as a hidden form field or the request is rejected with 400.

**Rate limit:** max 5 successful `POST /submit` requests per AP session. After 5, further submits return 429 with a message instructing the user to trigger a reset.

## NVS schema

Namespace: `"wifi"`.

| Key | Type | Max | Meaning |
|---|---|---|---|
| `ssid` | String | 32 chars | Stored home WiFi SSID |
| `pw` | String | 64 chars | Stored home WiFi password |
| `tz` | String | 64 chars | POSIX TZ string |
| `last_sync` | uint64 | — | Unix timestamp of last successful NTP sync |

Writes are atomic at the end of `VALIDATING` (on successful WL_CONNECTED) — never on form submit alone. Reset clears the entire `wifi` namespace via `Preferences::clear()`.

Flash wear: at most 1 credential write per setup event + 1 `last_sync` write per successful NTP sync (every 6 h). Well below the ESP32 NVS wear-leveled write budget over a 40-year lifespan.

## Timezone options

Initial list exposed in the form dropdown. Extensible by editing the table in `web_server.cpp`.

| Display label | POSIX TZ value |
|---|---|
| Pacific (Los Angeles, Seattle) — default | `PST8PDT,M3.2.0,M11.1.0` |
| Mountain (Denver, Salt Lake City) | `MST7MDT,M3.2.0,M11.1.0` |
| Arizona (Phoenix, no DST) | `MST7` |
| Central (Chicago, Dallas) | `CST6CDT,M3.2.0,M11.1.0` |
| Eastern (New York, Miami) | `EST5EDT,M3.2.0,M11.1.0` |
| Alaska (Anchorage) | `AKST9AKDT,M3.2.0,M11.1.0` |
| Hawaii (Honolulu) | `HST10` |
| UTC | `UTC0` |

## HTML + build integration

The setup form HTML is authored as a standalone file that can be opened directly in a browser for visual iteration (with mocked data), and embedded as a C++ constant for the actual firmware.

### Template tokens

Substituted at build time by `gen_form_html.py` based on `CLOCK_TARGET` (`emory` or `nora`):

| Token | Emory value | Nora value |
|---|---|---|
| `{{KID_NAME}}` | `Emory` | `Nora` |
| `{{KID_ACCENT_COLOR}}` | `#b88b4a` (maple honey) | `#3d2817` (walnut) |
| `{{KID_BG_COLOR}}` | `#dcc09a` (maple cream) | `#f0e0c0` (walnut cream) |
| `{{TZ_OPTIONS}}` | 8 `<option>` tags with Pacific pre-selected | identical |

### Build-time generation

1. PlatformIO runs `extra_scripts = pre:assets/captive-portal/gen_form_html.py` before compiling.
2. The generator reads `form.html.template` + `form.css`, inlines the CSS, substitutes tokens, escapes for C++ string-literal embedding.
3. Writes `firmware/lib/wifi_provision/src/form_html.h` as `const char FORM_HTML[] PROGMEM = "...";`.
4. `form_html.h` is `.gitignore`'d — it's a generated artifact.

### Local preview workflow

1. Edit `form.html.template` + `form.css`.
2. `python gen_form_html.py --preview --kid emory > /tmp/preview.html`.
3. Open `/tmp/preview.html` in Safari. Dropdown shows 4 fake SSIDs; timezone dropdown is real; submit button is inert.
4. Iterate until aesthetic is right.
5. Normal `pio run -e emory` picks up the edited template automatically.

### UX help line

Under the SSID dropdown:
> Your clock connects over 2.4 GHz WiFi. If you don't see your network, make sure 2.4 GHz is enabled on your router.

Guards against the "I can't see my network" failure mode when users have split-SSID setups or have disabled 2.4 GHz in a mesh system.

## Security considerations

### Open AP — accepted risks and mitigations

The `WordClock-Setup-XXXX` AP is open (no WPA) to avoid the UX friction of a setup password printed on a card. The attack surface is bounded by:

1. **Lifetime.** The AP broadcasts only during first boot (before setup) or after the user triggers a reset. Auto-shuts on commit or after 10 minutes. Outside those windows, no AP is broadcasting.
2. **Physical confirmation.** Credentials aren't committed until the Audio button is pressed on the clock itself — a remote attacker who submits fake creds can't actually commit without local physical access.
3. **Rate limit.** 5 submissions per AP session; exhausting the limit requires explicit reset.
4. **Single client.** Only one phone / laptop can be associated to the AP at a time.
5. **Validation before commit.** Bad credentials never reach NVS; they're discarded after the failed STA attempt.

### What this design does NOT defend against

- **Passive RF sniffing of the home WiFi password** during the 10-minute setup window. An attacker within WiFi range actively recording packets will see the user's WiFi password in cleartext during the `POST /submit` → form body. Mitigation options (not adopted): TLS with a self-signed cert (browser warning), WPA-protected AP (friction), or displaying a per-session passcode on the LED chain (chicken-and-egg). The realistic threat at a home setup event is near zero.
- **Physical theft + flash dumping.** An attacker with physical possession of the clock and a SPI flash programmer can dump NVS and recover the stored WiFi password. Defense: ESP32 flash encryption + secure boot. Flagged as future hardening, not implemented.
- **Home LAN attackers.** Once associated to the home network, the clock has the same LAN exposure as any other device. The clock is a pure WiFi client — no HTTP / mDNS / SSH listening — so there's no direct inbound attack surface, but anything compromising the router compromises the clock indirectly.

## Testing strategy

### Native tests (PlatformIO `test -e native`)

- `form_parse_test` — URL-encoded form body parsing. Normal input + edge cases (empty fields, special chars, SSID with spaces).
- `validate_credentials_test` — syntactic validation. Reject over-length SSID / password; accept empty password for open WiFi.
- `tz_string_lookup_test` — label-to-POSIX round-trip for every timezone option.
- `state_machine_test` — transitions as a pure function: `(state, event) → (next_state, side_effects)`. Covers every transition in the state diagram.
- `html_gen_test.py` — runs `gen_form_html.py` in-process, parses output, asserts kid name, all 8 TZ options present, Pacific selected, correct accent colors.

### Hardware-manual tests

Documented in `firmware/test/hardware_checks/wifi_provision_checks.md`:

1. Cold boot, empty NVS → AP `WordClock-Setup-XXXX` broadcasts.
2. iPhone joins AP → captive portal auto-pops (Safari opens with form).
3. Submit valid credentials → "press Audio button" page.
4. Press Audio button within 60 seconds → WiFi connects → AP shuts down → clock face lights up.
5. Power cycle → clock reconnects without prompting for credentials.
6. Hold Hour + Audio for 10 seconds → clock reboots into captive portal.
7. Submit deliberately wrong password → "Could not connect" error on the form → can retry.
8. Verify amber tint present on first boot before sync, clears to warm white after NTP sync.

### What's explicitly NOT automated

- iOS captive-portal popup behavior (closed-OS UI).
- Nest WiFi band steering edge cases.
- Signal strength / range.

## Out of scope

- BLE provisioning (recommended for mass-produced IoT in 2026, but overkill for a one-off gift clock).
- Timezone DST updates pushed OTA (POSIX strings include DST rules statically; if US DST rules change, firmware update distributes new strings).
- Remote settings web UI during normal operation (clock is STA-only during normal operation; no HTTP listening).
- OTA firmware updates (future feature; not blocking this module).
- Brightness / bedtime-dim runtime configuration (firmware constants per the main design spec).
- Multiple stored SSIDs / network roaming (one SSID at a time; reset to change).

## Open items

None at design-lock time. All key decisions resolved during brainstorming:

- Library choice: custom (not WiFiManager).
- AP security: open, with 5 layers of hardening around it.
- Form scope: minimal (SSID, password, timezone only).
- HTML storage: PROGMEM via build-time generator.
- Visual polish: per-kid themed (wood-species palette) with local-preview workflow.
- Reset mechanism: Hour + Audio held 10 seconds.
- Credential validation: connection attempt before NVS write.
- RTC-only signal: amber tint on lit words after 24 h without NTP sync.

## References

- `docs/superpowers/specs/2026-04-14-daughters-clocks-design.md` §WiFi provisioning
- `docs/superpowers/plans/2026-04-14-daughters-clocks-implementation.md` §Phase 2 `wifi_provision`
- `docs/hardware/pinout.md` §Power and §GPIO assignments (buttons at GPIO 14 / 32 / 33)
- Arduino-ESP32 core: `WiFi`, `DNSServer`, `WebServer`, `Preferences`
- Research sources consulted during brainstorming:
  - Harald Kreuzer — [ESP32 WiFi Provisioning: SoftAP and Captive Portal](https://www.haraldkreuzer.net/en/news/esp32-wifi-provisioning-soft-ap-and-captive-portal)
  - Engineering IoT — [ESP32 Captive Portal: Build a Redirect Wi-Fi Page](https://medium.com/engineering-iot/creating-a-captive-portal-on-esp32-a-complete-guide-9853a1534153)
  - Jordi Enric — [ESP32 Captive Portal Wi-Fi Provisioning](https://www.jordienric.com/writing/captive-portal-wifi-provisioning-in-an-esp32)
  - CDFER — [Captive-Portal-ESP32 reference implementation](https://github.com/CDFER/Captive-Portal-ESP32)
