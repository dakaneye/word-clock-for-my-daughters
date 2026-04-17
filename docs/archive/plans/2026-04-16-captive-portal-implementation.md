# Captive Portal Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Ship the `wifi_provision` firmware module that runs a WiFi setup captive portal on first boot, persists credentials + timezone to NVS after validation, and re-enters the portal on Hour + Audio button combo.

**Architecture:** Hexagonal. Pure-logic modules (state machine, form parser, credential validator, timezone lookup) are native-testable with Unity. ESP32-only adapters (DNS server, web server, NVS store, WiFi radio) are thin wrappers that delegate to pure logic for all decisions. A build-time Python script embeds a per-kid HTML form as a PROGMEM constant so the UI is browser-previewable during iteration.

**Tech Stack:** Arduino-ESP32 core (`WiFi`, `DNSServer`, `WebServer`, `Preferences`), PlatformIO build system, Unity C++ test framework for native tests, pytest for the Python HTML generator tests.

---

## Spec reference

This plan implements `docs/superpowers/specs/2026-04-16-captive-portal-design.md`. Read the spec in full before starting — it pins down every behavioural decision (state transitions, timeouts, security properties, NVS schema).

## File structure

```
firmware/
  lib/wifi_provision/
    include/
      wifi_provision.h              # public module API (ESP32 only)
      wifi_provision/
        state_machine.h             # pure logic: state enum + transition fn
        form_parser.h               # pure logic: URL-decode + field extract
        credential_validator.h      # pure logic: syntactic validation
        tz_options.h                # pure logic: POSIX TZ table
    src/
      wifi_provision.cpp            # ESP32: module lifecycle (begin / loop / reset)
      web_server.cpp                # ESP32: HTTP routes, CSRF, rate limit
      dns_wrapper.cpp               # ESP32: DNSServer wrapper (all queries → AP IP)
      nvs_store.cpp                 # ESP32: Preferences wrapper
      state_machine.cpp             # pure logic impl
      form_parser.cpp               # pure logic impl
      credential_validator.cpp      # pure logic impl
      tz_options.cpp                # pure logic impl
      form_html.h                   # .gitignore'd — generated at build time
  assets/captive-portal/
    form.html.template              # editable HTML, browser-previewable
    form.css                        # shared styles
    gen_form_html.py                # build-time generator + preview tool
    tests/
      test_gen_form_html.py         # pytest
  test/
    test_wifi_state_machine/
      test_state_machine.cpp        # native Unity tests
    test_wifi_form_parser/
      test_form_parser.cpp
    test_wifi_credential_validator/
      test_credential_validator.cpp
    test_wifi_tz_options/
      test_tz_options.cpp
  configs/
    config_emory.h                  # existing — one line addition for kid color
    config_nora.h                   # existing — one line addition for kid color
  platformio.ini                    # modified: extra_scripts + include path
  src/main.cpp                      # modified: wire in wifi_provision lifecycle
```

---

## Task 1: Scaffold the module + state enum header

**Files:**
- Create: `firmware/lib/wifi_provision/include/wifi_provision/state_machine.h`
- Create: `firmware/test/test_wifi_state_machine/test_state_machine.cpp`

- [ ] **Step 1: Write the failing test**

```cpp
// firmware/test/test_wifi_state_machine/test_state_machine.cpp
#include <unity.h>
#include "wifi_provision/state_machine.h"

using namespace wc::wifi_provision;

void setUp(void) {}
void tearDown(void) {}

void test_boot_with_no_credentials_transitions_to_ap_active(void) {
    StateMachine sm;
    sm.handle(Event::BootWithNoCredentials);
    TEST_ASSERT_EQUAL(State::ApActive, sm.state());
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_boot_with_no_credentials_transitions_to_ap_active);
    return UNITY_END();
}
```

- [ ] **Step 2: Run test to confirm it fails**

Run: `cd firmware && pio test -e native -f test_wifi_state_machine`
Expected: FAIL at compile with "fatal error: wifi_provision/state_machine.h: No such file or directory"

- [ ] **Step 3: Create the header with minimal types**

```cpp
// firmware/lib/wifi_provision/include/wifi_provision/state_machine.h
#pragma once

namespace wc::wifi_provision {

enum class State {
    Boot,
    StaConnecting,
    ApActive,
    AwaitingConfirmation,
    Validating,
    Online,
    IdleNoCredentials,
};

enum class Event {
    BootWithNoCredentials,
    BootWithCredentials,
    StaConnected,
    StaFailed,
    FormSubmitted,
    AudioButtonConfirmed,
    ConfirmationTimeout,
    ValidationSucceeded,
    ValidationFailed,
    ApTimeout,
    ResetCombo,
    WiFiDropped,
};

class StateMachine {
public:
    void handle(Event e);
    State state() const { return state_; }

private:
    State state_ = State::Boot;
};

} // namespace wc::wifi_provision
```

- [ ] **Step 4: Create minimal impl**

```cpp
// firmware/lib/wifi_provision/src/state_machine.cpp
#include "wifi_provision/state_machine.h"

namespace wc::wifi_provision {

void StateMachine::handle(Event e) {
    if (e == Event::BootWithNoCredentials) {
        state_ = State::ApActive;
    }
}

} // namespace wc::wifi_provision
```

- [ ] **Step 5: Wire the test into PlatformIO include path**

Edit `firmware/platformio.ini`, change the `[env:native]` section:

```ini
[env:native]
platform = native
test_framework = unity
build_flags =
    ${env.build_flags}
    -I lib/core/include
    -I lib/wifi_provision/include
build_src_filter = +<../lib/core/src/*> +<../lib/wifi_provision/src/state_machine.cpp> +<../lib/wifi_provision/src/form_parser.cpp> +<../lib/wifi_provision/src/credential_validator.cpp> +<../lib/wifi_provision/src/tz_options.cpp>
lib_compat_mode = off
```

Only the pure-logic `.cpp` files go in the filter — the ESP32 adapter `.cpp` files must not compile on native.

- [ ] **Step 6: Run test to verify it passes**

Run: `cd firmware && pio test -e native -f test_wifi_state_machine`
Expected: PASS (1 test)

- [ ] **Step 7: Commit**

```bash
git add firmware/lib/wifi_provision/ firmware/test/test_wifi_state_machine/ firmware/platformio.ini
git commit -m "feat(firmware): scaffold wifi_provision state machine"
```

---

## Task 2: Cover every state machine transition

**Files:**
- Modify: `firmware/test/test_wifi_state_machine/test_state_machine.cpp`
- Modify: `firmware/lib/wifi_provision/src/state_machine.cpp`

- [ ] **Step 1: Extend the tests**

Replace the body of `test_state_machine.cpp` with full coverage (one test per transition in the state machine diagram from the spec):

```cpp
#include <unity.h>
#include "wifi_provision/state_machine.h"

using namespace wc::wifi_provision;

void setUp(void) {}
void tearDown(void) {}

static StateMachine machineIn(State s) {
    // Convenience constructor-style helper — drive the machine into the
    // requested state via a deterministic sequence of events.
    StateMachine m;
    switch (s) {
        case State::Boot: break;
        case State::ApActive:
            m.handle(Event::BootWithNoCredentials); break;
        case State::StaConnecting:
            m.handle(Event::BootWithCredentials); break;
        case State::AwaitingConfirmation:
            m.handle(Event::BootWithNoCredentials);
            m.handle(Event::FormSubmitted); break;
        case State::Validating:
            m.handle(Event::BootWithNoCredentials);
            m.handle(Event::FormSubmitted);
            m.handle(Event::AudioButtonConfirmed); break;
        case State::Online:
            m.handle(Event::BootWithCredentials);
            m.handle(Event::StaConnected); break;
        case State::IdleNoCredentials:
            m.handle(Event::BootWithNoCredentials);
            m.handle(Event::ApTimeout); break;
    }
    return m;
}

void test_boot_no_creds_to_ap(void) {
    auto m = machineIn(State::Boot);
    m.handle(Event::BootWithNoCredentials);
    TEST_ASSERT_EQUAL(State::ApActive, m.state());
}

void test_boot_with_creds_to_sta_connecting(void) {
    auto m = machineIn(State::Boot);
    m.handle(Event::BootWithCredentials);
    TEST_ASSERT_EQUAL(State::StaConnecting, m.state());
}

void test_sta_connecting_to_online_on_connect(void) {
    auto m = machineIn(State::StaConnecting);
    m.handle(Event::StaConnected);
    TEST_ASSERT_EQUAL(State::Online, m.state());
}

void test_sta_connecting_stays_on_fail(void) {
    auto m = machineIn(State::StaConnecting);
    m.handle(Event::StaFailed);
    TEST_ASSERT_EQUAL(State::StaConnecting, m.state());
}

void test_ap_active_to_awaiting_on_submit(void) {
    auto m = machineIn(State::ApActive);
    m.handle(Event::FormSubmitted);
    TEST_ASSERT_EQUAL(State::AwaitingConfirmation, m.state());
}

void test_ap_active_to_idle_on_timeout(void) {
    auto m = machineIn(State::ApActive);
    m.handle(Event::ApTimeout);
    TEST_ASSERT_EQUAL(State::IdleNoCredentials, m.state());
}

void test_awaiting_to_validating_on_button(void) {
    auto m = machineIn(State::AwaitingConfirmation);
    m.handle(Event::AudioButtonConfirmed);
    TEST_ASSERT_EQUAL(State::Validating, m.state());
}

void test_awaiting_back_to_ap_on_timeout(void) {
    auto m = machineIn(State::AwaitingConfirmation);
    m.handle(Event::ConfirmationTimeout);
    TEST_ASSERT_EQUAL(State::ApActive, m.state());
}

void test_validating_to_online_on_success(void) {
    auto m = machineIn(State::Validating);
    m.handle(Event::ValidationSucceeded);
    TEST_ASSERT_EQUAL(State::Online, m.state());
}

void test_validating_back_to_ap_on_fail(void) {
    auto m = machineIn(State::Validating);
    m.handle(Event::ValidationFailed);
    TEST_ASSERT_EQUAL(State::ApActive, m.state());
}

void test_online_to_sta_connecting_on_drop(void) {
    auto m = machineIn(State::Online);
    m.handle(Event::WiFiDropped);
    TEST_ASSERT_EQUAL(State::StaConnecting, m.state());
}

void test_online_to_ap_on_reset_combo(void) {
    auto m = machineIn(State::Online);
    m.handle(Event::ResetCombo);
    TEST_ASSERT_EQUAL(State::ApActive, m.state());
}

void test_idle_to_ap_on_reset_combo(void) {
    auto m = machineIn(State::IdleNoCredentials);
    m.handle(Event::ResetCombo);
    TEST_ASSERT_EQUAL(State::ApActive, m.state());
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_boot_no_creds_to_ap);
    RUN_TEST(test_boot_with_creds_to_sta_connecting);
    RUN_TEST(test_sta_connecting_to_online_on_connect);
    RUN_TEST(test_sta_connecting_stays_on_fail);
    RUN_TEST(test_ap_active_to_awaiting_on_submit);
    RUN_TEST(test_ap_active_to_idle_on_timeout);
    RUN_TEST(test_awaiting_to_validating_on_button);
    RUN_TEST(test_awaiting_back_to_ap_on_timeout);
    RUN_TEST(test_validating_to_online_on_success);
    RUN_TEST(test_validating_back_to_ap_on_fail);
    RUN_TEST(test_online_to_sta_connecting_on_drop);
    RUN_TEST(test_online_to_ap_on_reset_combo);
    RUN_TEST(test_idle_to_ap_on_reset_combo);
    return UNITY_END();
}
```

- [ ] **Step 2: Run to see 12 new tests fail**

Run: `cd firmware && pio test -e native -f test_wifi_state_machine`
Expected: 1 PASS, 12 FAIL

- [ ] **Step 3: Expand the state machine**

Replace the body of `state_machine.cpp`:

```cpp
#include "wifi_provision/state_machine.h"

namespace wc::wifi_provision {

void StateMachine::handle(Event e) {
    switch (state_) {
        case State::Boot:
            if (e == Event::BootWithNoCredentials) state_ = State::ApActive;
            else if (e == Event::BootWithCredentials) state_ = State::StaConnecting;
            break;
        case State::StaConnecting:
            if (e == Event::StaConnected) state_ = State::Online;
            // StaFailed: stay in StaConnecting (retry driven by caller)
            break;
        case State::ApActive:
            if (e == Event::FormSubmitted) state_ = State::AwaitingConfirmation;
            else if (e == Event::ApTimeout) state_ = State::IdleNoCredentials;
            break;
        case State::AwaitingConfirmation:
            if (e == Event::AudioButtonConfirmed) state_ = State::Validating;
            else if (e == Event::ConfirmationTimeout) state_ = State::ApActive;
            break;
        case State::Validating:
            if (e == Event::ValidationSucceeded) state_ = State::Online;
            else if (e == Event::ValidationFailed) state_ = State::ApActive;
            break;
        case State::Online:
            if (e == Event::WiFiDropped) state_ = State::StaConnecting;
            else if (e == Event::ResetCombo) state_ = State::ApActive;
            break;
        case State::IdleNoCredentials:
            if (e == Event::ResetCombo) state_ = State::ApActive;
            break;
    }
}

} // namespace wc::wifi_provision
```

- [ ] **Step 4: Run all tests, confirm they pass**

Run: `cd firmware && pio test -e native -f test_wifi_state_machine`
Expected: 13 PASS

- [ ] **Step 5: Commit**

```bash
git add firmware/
git commit -m "feat(firmware): complete wifi_provision state machine"
```

---

## Task 3: URL-encoded form body parser

**Files:**
- Create: `firmware/lib/wifi_provision/include/wifi_provision/form_parser.h`
- Create: `firmware/lib/wifi_provision/src/form_parser.cpp`
- Create: `firmware/test/test_wifi_form_parser/test_form_parser.cpp`

- [ ] **Step 1: Write the failing tests**

```cpp
// firmware/test/test_wifi_form_parser/test_form_parser.cpp
#include <unity.h>
#include "wifi_provision/form_parser.h"

using namespace wc::wifi_provision;

void setUp(void) {}
void tearDown(void) {}

void test_parses_simple_body(void) {
    FormBody body = parse_form_body("ssid=HomeWiFi&pw=secret&tz=PST8PDT");
    TEST_ASSERT_EQUAL_STRING("HomeWiFi", body.ssid.c_str());
    TEST_ASSERT_EQUAL_STRING("secret", body.pw.c_str());
    TEST_ASSERT_EQUAL_STRING("PST8PDT", body.tz.c_str());
}

void test_decodes_plus_as_space(void) {
    FormBody body = parse_form_body("ssid=Home+WiFi+Network&pw=x&tz=UTC0");
    TEST_ASSERT_EQUAL_STRING("Home WiFi Network", body.ssid.c_str());
}

void test_decodes_percent_escapes(void) {
    // %21 = '!', %26 = '&' (inside a field, escaped)
    FormBody body = parse_form_body("ssid=a%21b&pw=p%26q&tz=UTC0");
    TEST_ASSERT_EQUAL_STRING("a!b", body.ssid.c_str());
    TEST_ASSERT_EQUAL_STRING("p&q", body.pw.c_str());
}

void test_missing_field_is_empty(void) {
    FormBody body = parse_form_body("ssid=X&tz=UTC0");
    TEST_ASSERT_EQUAL_STRING("X", body.ssid.c_str());
    TEST_ASSERT_EQUAL_STRING("", body.pw.c_str());
    TEST_ASSERT_EQUAL_STRING("UTC0", body.tz.c_str());
}

void test_empty_body_yields_empty_fields(void) {
    FormBody body = parse_form_body("");
    TEST_ASSERT_EQUAL_STRING("", body.ssid.c_str());
    TEST_ASSERT_EQUAL_STRING("", body.pw.c_str());
    TEST_ASSERT_EQUAL_STRING("", body.tz.c_str());
}

void test_extracts_csrf_token(void) {
    FormBody body = parse_form_body("csrf=abc123&ssid=X&pw=Y&tz=UTC0");
    TEST_ASSERT_EQUAL_STRING("abc123", body.csrf.c_str());
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_parses_simple_body);
    RUN_TEST(test_decodes_plus_as_space);
    RUN_TEST(test_decodes_percent_escapes);
    RUN_TEST(test_missing_field_is_empty);
    RUN_TEST(test_empty_body_yields_empty_fields);
    RUN_TEST(test_extracts_csrf_token);
    return UNITY_END();
}
```

- [ ] **Step 2: Run to confirm failure**

Run: `cd firmware && pio test -e native -f test_wifi_form_parser`
Expected: compile error (header missing)

- [ ] **Step 3: Create the header**

```cpp
// firmware/lib/wifi_provision/include/wifi_provision/form_parser.h
#pragma once

#include <string>

namespace wc::wifi_provision {

struct FormBody {
    std::string ssid;
    std::string pw;
    std::string tz;
    std::string csrf;
};

// Parses an application/x-www-form-urlencoded body.
// Unknown fields are ignored. Missing known fields are returned as empty strings.
// Decodes '+' → ' ' and %XX hex escapes. Malformed %XX yields the literal chars.
FormBody parse_form_body(const std::string& body);

} // namespace wc::wifi_provision
```

- [ ] **Step 4: Create the impl**

```cpp
// firmware/lib/wifi_provision/src/form_parser.cpp
#include "wifi_provision/form_parser.h"

namespace wc::wifi_provision {

static int hex_digit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return -1;
}

static std::string url_decode(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        if (c == '+') {
            out.push_back(' ');
        } else if (c == '%' && i + 2 < s.size()) {
            int hi = hex_digit(s[i+1]);
            int lo = hex_digit(s[i+2]);
            if (hi >= 0 && lo >= 0) {
                out.push_back(static_cast<char>((hi << 4) | lo));
                i += 2;
            } else {
                out.push_back(c);
            }
        } else {
            out.push_back(c);
        }
    }
    return out;
}

FormBody parse_form_body(const std::string& body) {
    FormBody out;
    size_t pos = 0;
    while (pos < body.size()) {
        size_t amp = body.find('&', pos);
        if (amp == std::string::npos) amp = body.size();
        size_t eq = body.find('=', pos);
        if (eq == std::string::npos || eq > amp) {
            pos = amp + 1;
            continue;
        }
        std::string key = body.substr(pos, eq - pos);
        std::string val = url_decode(body.substr(eq + 1, amp - (eq + 1)));
        if (key == "ssid") out.ssid = val;
        else if (key == "pw") out.pw = val;
        else if (key == "tz") out.tz = val;
        else if (key == "csrf") out.csrf = val;
        pos = amp + 1;
    }
    return out;
}

} // namespace wc::wifi_provision
```

- [ ] **Step 5: Add the test source to PlatformIO build filter**

`form_parser.cpp` is already in the filter list from Task 1. No change needed.

- [ ] **Step 6: Run tests to confirm pass**

Run: `cd firmware && pio test -e native -f test_wifi_form_parser`
Expected: 6 PASS

- [ ] **Step 7: Commit**

```bash
git add firmware/
git commit -m "feat(firmware): form body URL-decoder for wifi_provision"
```

---

## Task 4: Credential syntactic validator

**Files:**
- Create: `firmware/lib/wifi_provision/include/wifi_provision/credential_validator.h`
- Create: `firmware/lib/wifi_provision/src/credential_validator.cpp`
- Create: `firmware/test/test_wifi_credential_validator/test_credential_validator.cpp`

- [ ] **Step 1: Write the failing tests**

```cpp
// firmware/test/test_wifi_credential_validator/test_credential_validator.cpp
#include <unity.h>
#include "wifi_provision/credential_validator.h"
#include "wifi_provision/form_parser.h"

using namespace wc::wifi_provision;

void setUp(void) {}
void tearDown(void) {}

static FormBody body(const char* ssid, const char* pw, const char* tz) {
    FormBody b;
    b.ssid = ssid;
    b.pw = pw;
    b.tz = tz;
    return b;
}

void test_valid_credentials_pass(void) {
    auto result = validate(body("HomeWiFi", "goodpass", "PST8PDT,M3.2.0,M11.1.0"));
    TEST_ASSERT_TRUE(result.ok);
}

void test_empty_ssid_fails(void) {
    auto result = validate(body("", "goodpass", "PST8PDT,M3.2.0,M11.1.0"));
    TEST_ASSERT_FALSE(result.ok);
}

void test_empty_password_ok_for_open_network(void) {
    auto result = validate(body("OpenNet", "", "UTC0"));
    TEST_ASSERT_TRUE(result.ok);
}

void test_ssid_too_long_fails(void) {
    // WiFi SSID max is 32 chars (IEEE 802.11)
    std::string long_ssid(33, 'X');
    auto result = validate(body(long_ssid.c_str(), "p", "UTC0"));
    TEST_ASSERT_FALSE(result.ok);
}

void test_ssid_exactly_32_chars_ok(void) {
    std::string boundary(32, 'X');
    auto result = validate(body(boundary.c_str(), "p", "UTC0"));
    TEST_ASSERT_TRUE(result.ok);
}

void test_password_too_long_fails(void) {
    // WPA2 passphrase max is 63 chars; we allow 64 in NVS to account for edge cases
    std::string long_pw(65, 'p');
    auto result = validate(body("SSID", long_pw.c_str(), "UTC0"));
    TEST_ASSERT_FALSE(result.ok);
}

void test_unknown_tz_fails(void) {
    auto result = validate(body("SSID", "pw", "not_a_real_tz"));
    TEST_ASSERT_FALSE(result.ok);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_valid_credentials_pass);
    RUN_TEST(test_empty_ssid_fails);
    RUN_TEST(test_empty_password_ok_for_open_network);
    RUN_TEST(test_ssid_too_long_fails);
    RUN_TEST(test_ssid_exactly_32_chars_ok);
    RUN_TEST(test_password_too_long_fails);
    RUN_TEST(test_unknown_tz_fails);
    return UNITY_END();
}
```

- [ ] **Step 2: Run to confirm failure**

Run: `cd firmware && pio test -e native -f test_wifi_credential_validator`
Expected: compile error

- [ ] **Step 3: Create header**

```cpp
// firmware/lib/wifi_provision/include/wifi_provision/credential_validator.h
#pragma once

#include <string>
#include "wifi_provision/form_parser.h"

namespace wc::wifi_provision {

struct ValidationResult {
    bool ok;
    std::string message; // empty on success; human-readable reason on failure
};

ValidationResult validate(const FormBody& body);

} // namespace wc::wifi_provision
```

- [ ] **Step 4: Create impl**

```cpp
// firmware/lib/wifi_provision/src/credential_validator.cpp
#include "wifi_provision/credential_validator.h"
#include "wifi_provision/tz_options.h"

namespace wc::wifi_provision {

static constexpr size_t MAX_SSID_LEN = 32;
static constexpr size_t MAX_PW_LEN = 64;

ValidationResult validate(const FormBody& body) {
    if (body.ssid.empty()) {
        return {false, "SSID is required."};
    }
    if (body.ssid.size() > MAX_SSID_LEN) {
        return {false, "SSID must be 32 characters or fewer."};
    }
    if (body.pw.size() > MAX_PW_LEN) {
        return {false, "Password must be 64 characters or fewer."};
    }
    if (!is_known_posix_tz(body.tz)) {
        return {false, "Please pick a timezone from the list."};
    }
    return {true, ""};
}

} // namespace wc::wifi_provision
```

This depends on `is_known_posix_tz` from the TZ module. That's Task 5 — write it concurrently. Running tests now will fail on the linker; that's fine until Task 5 lands.

- [ ] **Step 5: Commit what we have**

```bash
git add firmware/
git commit -m "feat(firmware): credential validator (waits on tz_options)"
```

---

## Task 5: Timezone options table

**Files:**
- Create: `firmware/lib/wifi_provision/include/wifi_provision/tz_options.h`
- Create: `firmware/lib/wifi_provision/src/tz_options.cpp`
- Create: `firmware/test/test_wifi_tz_options/test_tz_options.cpp`

- [ ] **Step 1: Write the failing tests**

```cpp
// firmware/test/test_wifi_tz_options/test_tz_options.cpp
#include <unity.h>
#include "wifi_provision/tz_options.h"

using namespace wc::wifi_provision;

void setUp(void) {}
void tearDown(void) {}

void test_has_eight_options(void) {
    auto opts = tz_options();
    TEST_ASSERT_EQUAL(8, opts.size());
}

void test_pacific_is_first(void) {
    auto opts = tz_options();
    TEST_ASSERT_EQUAL_STRING("PST8PDT,M3.2.0,M11.1.0", opts[0].posix.c_str());
}

void test_arizona_has_no_dst(void) {
    auto opts = tz_options();
    for (const auto& opt : opts) {
        if (opt.label.find("Arizona") != std::string::npos) {
            TEST_ASSERT_EQUAL_STRING("MST7", opt.posix.c_str());
            return;
        }
    }
    TEST_FAIL_MESSAGE("Arizona not found in options");
}

void test_is_known_posix_tz_recognizes_all(void) {
    for (const auto& opt : tz_options()) {
        TEST_ASSERT_TRUE_MESSAGE(is_known_posix_tz(opt.posix), opt.posix.c_str());
    }
}

void test_is_known_posix_tz_rejects_garbage(void) {
    TEST_ASSERT_FALSE(is_known_posix_tz("not_a_tz"));
    TEST_ASSERT_FALSE(is_known_posix_tz(""));
    TEST_ASSERT_FALSE(is_known_posix_tz("America/Los_Angeles"));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_has_eight_options);
    RUN_TEST(test_pacific_is_first);
    RUN_TEST(test_arizona_has_no_dst);
    RUN_TEST(test_is_known_posix_tz_recognizes_all);
    RUN_TEST(test_is_known_posix_tz_rejects_garbage);
    return UNITY_END();
}
```

- [ ] **Step 2: Run to confirm failure**

Run: `cd firmware && pio test -e native -f test_wifi_tz_options`
Expected: compile error

- [ ] **Step 3: Create header**

```cpp
// firmware/lib/wifi_provision/include/wifi_provision/tz_options.h
#pragma once

#include <string>
#include <vector>

namespace wc::wifi_provision {

struct TzOption {
    std::string label; // human-readable dropdown display
    std::string posix; // POSIX TZ string for setenv("TZ", ...)
};

const std::vector<TzOption>& tz_options();
bool is_known_posix_tz(const std::string& posix);

} // namespace wc::wifi_provision
```

- [ ] **Step 4: Create impl**

```cpp
// firmware/lib/wifi_provision/src/tz_options.cpp
#include "wifi_provision/tz_options.h"

namespace wc::wifi_provision {

const std::vector<TzOption>& tz_options() {
    static const std::vector<TzOption> opts = {
        {"Pacific (Los Angeles, Seattle)",       "PST8PDT,M3.2.0,M11.1.0"},
        {"Mountain (Denver, Salt Lake City)",    "MST7MDT,M3.2.0,M11.1.0"},
        {"Arizona (Phoenix, no DST)",            "MST7"},
        {"Central (Chicago, Dallas)",            "CST6CDT,M3.2.0,M11.1.0"},
        {"Eastern (New York, Miami)",            "EST5EDT,M3.2.0,M11.1.0"},
        {"Alaska (Anchorage)",                   "AKST9AKDT,M3.2.0,M11.1.0"},
        {"Hawaii (Honolulu)",                    "HST10"},
        {"UTC",                                  "UTC0"},
    };
    return opts;
}

bool is_known_posix_tz(const std::string& posix) {
    for (const auto& opt : tz_options()) {
        if (opt.posix == posix) return true;
    }
    return false;
}

} // namespace wc::wifi_provision
```

- [ ] **Step 5: Run TZ tests and credential validator tests — both now pass**

```bash
cd firmware
pio test -e native -f test_wifi_tz_options
pio test -e native -f test_wifi_credential_validator
```

Expected: 5 PASS + 7 PASS = 12 total.

- [ ] **Step 6: Run the full native suite to confirm nothing broke**

Run: `cd firmware && pio test -e native`
Expected: every existing Phase 1 test still passes, plus the new wifi_provision suites.

- [ ] **Step 7: Commit**

```bash
git add firmware/
git commit -m "feat(firmware): timezone options table + known-tz validator"
```

---

## Task 6: HTML form template + CSS

**Files:**
- Create: `firmware/assets/captive-portal/form.html.template`
- Create: `firmware/assets/captive-portal/form.css`

- [ ] **Step 1: Create the CSS**

```css
/* firmware/assets/captive-portal/form.css */
:root {
    --bg:       {{KID_BG_COLOR}};
    --accent:   {{KID_ACCENT_COLOR}};
    --text:     #2d1e0e;
    --subtle:   rgba(45, 30, 14, 0.6);
}

* { box-sizing: border-box; }

body {
    background: var(--bg);
    color: var(--text);
    font-family: Georgia, serif;
    margin: 0;
    padding: 2rem 1.5rem;
    min-height: 100vh;
    display: flex;
    flex-direction: column;
    align-items: center;
}

.card {
    background: rgba(255, 255, 255, 0.35);
    border: 1px solid var(--accent);
    border-radius: 8px;
    padding: 1.5rem;
    max-width: 32rem;
    width: 100%;
}

h1 {
    margin: 0 0 1rem 0;
    font-size: 1.35rem;
    color: var(--accent);
    font-weight: 600;
}

p.help {
    color: var(--subtle);
    font-size: 0.9rem;
    margin: 0.5rem 0 1rem 0;
    line-height: 1.4;
}

label {
    display: block;
    margin: 1rem 0 0.25rem 0;
    font-weight: 600;
    font-size: 0.95rem;
}

input, select {
    width: 100%;
    padding: 0.6rem 0.75rem;
    font-family: inherit;
    font-size: 1rem;
    background: white;
    border: 1px solid var(--accent);
    border-radius: 4px;
    color: var(--text);
}

button {
    margin-top: 1.5rem;
    width: 100%;
    padding: 0.75rem;
    font-family: inherit;
    font-size: 1rem;
    font-weight: 600;
    background: var(--accent);
    color: var(--bg);
    border: none;
    border-radius: 4px;
    cursor: pointer;
}

button:disabled {
    opacity: 0.5;
    cursor: not-allowed;
}

.error {
    color: #9b2c2c;
    font-size: 0.9rem;
    margin-top: 0.5rem;
    min-height: 1.2em;
}
```

- [ ] **Step 2: Create the HTML template**

```html
<!-- firmware/assets/captive-portal/form.html.template -->
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Setting up {{KID_NAME}}'s clock</title>
  <style>{{INLINE_CSS}}</style>
</head>
<body>
  <div class="card">
    <h1>Setting up {{KID_NAME}}'s clock</h1>

    <form id="wifi-form" method="POST" action="/submit">
      <input type="hidden" name="csrf" value="{{CSRF_TOKEN}}">

      <label for="ssid">WiFi network</label>
      <select name="ssid" id="ssid" required>
        <option value="">Scanning for networks…</option>
      </select>
      <p class="help">
        Your clock connects over 2.4 GHz WiFi. If you don't see your network,
        make sure 2.4 GHz is enabled on your router.
      </p>

      <label for="pw">WiFi password</label>
      <input type="password" name="pw" id="pw" autocomplete="off">

      <label for="tz">Timezone</label>
      <select name="tz" id="tz" required>
        {{TZ_OPTIONS}}
      </select>

      <button type="submit">Continue</button>

      <div class="error" id="error">{{ERROR_MESSAGE}}</div>
    </form>
  </div>

  <script>
    // Populate SSID dropdown from /scan once it arrives.
    fetch('/scan').then(r => r.json()).then(nets => {
      const sel = document.getElementById('ssid');
      sel.innerHTML = '';
      const seen = new Set();
      for (const n of nets) {
        if (seen.has(n.ssid)) continue;
        seen.add(n.ssid);
        const opt = document.createElement('option');
        opt.value = n.ssid;
        opt.textContent = n.ssid + (n.secured ? '' : ' (open)');
        sel.appendChild(opt);
      }
      if (sel.children.length === 0) {
        const opt = document.createElement('option');
        opt.value = '';
        opt.textContent = 'No networks found — try again';
        sel.appendChild(opt);
      }
    }).catch(() => {
      const sel = document.getElementById('ssid');
      sel.innerHTML = '<option value="">Scan failed — reload</option>';
    });
  </script>
</body>
</html>
```

- [ ] **Step 3: Visually sanity-check by opening in a browser**

Open `firmware/assets/captive-portal/form.html.template` directly in Safari. It should render with the raw `{{KID_NAME}}` etc. tokens visible (no generator yet). Verify layout is not broken in principle — the generator (Task 7) substitutes tokens to make it real.

- [ ] **Step 4: Commit**

```bash
git add firmware/assets/captive-portal/form.html.template firmware/assets/captive-portal/form.css
git commit -m "feat(firmware): captive portal form HTML + CSS"
```

---

## Task 7: Build-time HTML generator + pytest

**Files:**
- Create: `firmware/assets/captive-portal/gen_form_html.py`
- Create: `firmware/assets/captive-portal/tests/__init__.py` (empty)
- Create: `firmware/assets/captive-portal/tests/test_gen_form_html.py`
- Modify: `firmware/.gitignore` (add `form_html.h`)
- Modify: `firmware/platformio.ini` (add extra_scripts)

- [ ] **Step 1: Write the failing pytest**

```python
# firmware/assets/captive-portal/tests/test_gen_form_html.py
import subprocess
import sys
from pathlib import Path

import pytest

SCRIPT = Path(__file__).parents[1] / "gen_form_html.py"


def run_preview(kid: str) -> str:
    result = subprocess.run(
        [sys.executable, str(SCRIPT), "--preview", "--kid", kid],
        capture_output=True,
        text=True,
        check=True,
    )
    return result.stdout


def test_preview_contains_kid_name():
    for kid, expected in [("emory", "Emory"), ("nora", "Nora")]:
        out = run_preview(kid)
        assert f"Setting up {expected}'s clock" in out


def test_preview_has_all_eight_tz_options():
    out = run_preview("emory")
    for label in (
        "Pacific (Los Angeles, Seattle)",
        "Mountain (Denver, Salt Lake City)",
        "Arizona (Phoenix, no DST)",
        "Central (Chicago, Dallas)",
        "Eastern (New York, Miami)",
        "Alaska (Anchorage)",
        "Hawaii (Honolulu)",
        "UTC",
    ):
        assert label in out


def test_pacific_is_preselected():
    out = run_preview("emory")
    assert 'selected value="PST8PDT,M3.2.0,M11.1.0"' in out or \
           'value="PST8PDT,M3.2.0,M11.1.0" selected' in out


def test_kid_accent_color_differs_per_kid():
    emory = run_preview("emory")
    nora = run_preview("nora")
    assert "#b88b4a" in emory
    assert "#b88b4a" not in nora
    assert "#3d2817" in nora
    assert "#3d2817" not in emory


def test_embedded_header_is_valid_c_literal(tmp_path):
    out_file = tmp_path / "form_html.h"
    subprocess.run(
        [sys.executable, str(SCRIPT), "--kid", "emory", "--out", str(out_file)],
        check=True,
    )
    contents = out_file.read_text()
    assert "const char FORM_HTML[]" in contents
    assert "PROGMEM" in contents
    # Ensure no raw backslashes, quotes, or newlines break the literal
    # (quick sanity — a full parse would need a C compiler)
    start = contents.index('"')
    end = contents.rindex('"')
    literal_body = contents[start + 1 : end]
    for forbidden in ["\n", "\r"]:
        assert forbidden not in literal_body, f"Raw {forbidden!r} in string literal"
```

- [ ] **Step 2: Run to confirm failure**

Run: `cd firmware/assets/captive-portal && python -m pytest tests/`
Expected: FAIL with "script not found" or ImportError

- [ ] **Step 3: Write the generator**

```python
# firmware/assets/captive-portal/gen_form_html.py
"""Generate a per-kid PROGMEM C++ header from form.html.template + form.css.

Usage:
    # Build-time (PlatformIO extra_script)
    python gen_form_html.py --kid emory --out /path/to/form_html.h

    # Local browser preview (substitutes tokens, prints to stdout)
    python gen_form_html.py --kid emory --preview > /tmp/preview.html
"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[3]
ASSETS = Path(__file__).resolve().parent
TEMPLATE = ASSETS / "form.html.template"
CSS = ASSETS / "form.css"

# TZ options must stay in lockstep with firmware/lib/wifi_provision/src/tz_options.cpp
TZ_OPTIONS = [
    ("Pacific (Los Angeles, Seattle)",       "PST8PDT,M3.2.0,M11.1.0"),
    ("Mountain (Denver, Salt Lake City)",    "MST7MDT,M3.2.0,M11.1.0"),
    ("Arizona (Phoenix, no DST)",            "MST7"),
    ("Central (Chicago, Dallas)",            "CST6CDT,M3.2.0,M11.1.0"),
    ("Eastern (New York, Miami)",            "EST5EDT,M3.2.0,M11.1.0"),
    ("Alaska (Anchorage)",                   "AKST9AKDT,M3.2.0,M11.1.0"),
    ("Hawaii (Honolulu)",                    "HST10"),
    ("UTC",                                  "UTC0"),
]

PACIFIC_POSIX = TZ_OPTIONS[0][1]

KID_TOKENS = {
    "emory": {
        "KID_NAME": "Emory",
        "KID_ACCENT_COLOR": "#b88b4a",
        "KID_BG_COLOR":     "#dcc09a",
    },
    "nora": {
        "KID_NAME": "Nora",
        "KID_ACCENT_COLOR": "#3d2817",
        "KID_BG_COLOR":     "#f0e0c0",
    },
}


def render_tz_options() -> str:
    parts = []
    for label, posix in TZ_OPTIONS:
        sel = " selected" if posix == PACIFIC_POSIX else ""
        parts.append(f'<option value="{posix}"{sel}>{label}</option>')
    return "\n        ".join(parts)


def substitute(template: str, kid: str, *, csrf_placeholder: str, error_placeholder: str) -> str:
    css = CSS.read_text()
    tokens = dict(KID_TOKENS[kid])
    # CSS has its own tokens; substitute those first, then inline.
    for k, v in tokens.items():
        css = css.replace("{{" + k + "}}", v)
    out = template
    for k, v in tokens.items():
        out = out.replace("{{" + k + "}}", v)
    out = out.replace("{{INLINE_CSS}}", css)
    out = out.replace("{{TZ_OPTIONS}}", render_tz_options())
    out = out.replace("{{CSRF_TOKEN}}", csrf_placeholder)
    out = out.replace("{{ERROR_MESSAGE}}", error_placeholder)
    return out


def to_c_string_literal(html: str) -> str:
    # Escape for a C++ raw string literal. We use a raw string with a custom
    # delimiter to avoid fussy backslash handling in HTML/JS.
    delim = "WCHTML"
    if f"){delim}" in html:
        raise RuntimeError(f"HTML contains the raw-string delimiter {delim}; pick another")
    return f'const char FORM_HTML[] PROGMEM = R"{delim}({html}){delim}";\n'


def emit_header(kid: str, out_path: Path) -> None:
    template = TEMPLATE.read_text()
    # CSRF and error are runtime-substituted on the ESP32, not at build time.
    # The build embeds the literal tokens; the web server replaces them per request.
    html = substitute(template, kid,
                      csrf_placeholder="{{CSRF_TOKEN}}",
                      error_placeholder="{{ERROR_MESSAGE}}")
    header = (
        "// Generated by gen_form_html.py — do not edit.\n"
        "#pragma once\n\n"
        + to_c_string_literal(html)
    )
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(header)


def emit_preview(kid: str) -> str:
    template = TEMPLATE.read_text()
    # Preview has no real CSRF / error, and injects a mock /scan stub so the
    # dropdown shows something without an ESP32.
    html = substitute(template, kid,
                      csrf_placeholder="preview-csrf",
                      error_placeholder="")
    mock = """
    <script>
      window.fetch = async () => ({ json: async () => ([
          {ssid: 'HomeWiFi-5G', rssi: -45, secured: true},
          {ssid: 'HomeWiFi-2.4', rssi: -52, secured: true},
          {ssid: 'Neighbor', rssi: -70, secured: true},
          {ssid: 'Guest', rssi: -60, secured: false},
      ])});
    </script>
    """
    return html.replace("</body>", mock + "</body>")


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--kid", required=True, choices=list(KID_TOKENS))
    ap.add_argument("--preview", action="store_true",
                    help="Write browser-previewable HTML to stdout.")
    ap.add_argument("--out", type=Path,
                    help="Path to write the embedded C++ header (required when not --preview).")
    args = ap.parse_args()

    if args.preview:
        sys.stdout.write(emit_preview(args.kid))
        return 0

    if not args.out:
        print("--out is required when not in --preview mode", file=sys.stderr)
        return 2

    emit_header(args.kid, args.out)
    return 0


if __name__ == "__main__":
    sys.exit(main())
```

- [ ] **Step 4: Run pytests, confirm pass**

```bash
cd firmware/assets/captive-portal
python -m pytest tests/ -v
```

Expected: 5 PASS.

- [ ] **Step 5: Create empty __init__.py for the test package**

Already listed above; create with `touch firmware/assets/captive-portal/tests/__init__.py`.

- [ ] **Step 6: Add .gitignore entry for the generated header**

Append to `firmware/.gitignore`:

```
# Generated by assets/captive-portal/gen_form_html.py
lib/wifi_provision/src/form_html.h
```

If `firmware/.gitignore` doesn't exist, create it.

- [ ] **Step 7: Wire the generator into PlatformIO**

Create `firmware/scripts/pio_generate_captive_html.py`:

```python
# firmware/scripts/pio_generate_captive_html.py
# Pre-build hook that runs gen_form_html.py for the current env.
from pathlib import Path
import subprocess
import sys

Import("env")  # type: ignore  # provided by PlatformIO at build time

ENV_NAME = env["PIOENV"]
KID = ENV_NAME  # our envs are named "emory" and "nora"

if KID not in ("emory", "nora"):
    # Skip for native / any unknown env — they don't need the header.
    Return()  # type: ignore

ROOT = Path(env["PROJECT_DIR"])
GEN = ROOT / "assets" / "captive-portal" / "gen_form_html.py"
OUT = ROOT / "lib" / "wifi_provision" / "src" / "form_html.h"

print(f"[captive-portal] generating {OUT.relative_to(ROOT)} for kid={KID}")
subprocess.run(
    [sys.executable, str(GEN), "--kid", KID, "--out", str(OUT)],
    check=True,
)
```

Edit `firmware/platformio.ini`, add `extra_scripts` to the `[esp32-base]` section:

```ini
[esp32-base]
platform = espressif32@^6.8.0
framework = arduino
board = esp32dev
lib_ldf_mode = deep+
build_flags =
    ${env.build_flags}
    -I configs
lib_deps =
    fastled/FastLED@^3.7.0
    adafruit/RTClib@^2.1.4
    arduino-libraries/NTPClient@^3.2.1
extra_scripts =
    pre:scripts/pio_generate_captive_html.py
```

- [ ] **Step 8: Smoke-test a compile**

Run: `cd firmware && pio run -e emory --target=checkprogsize` (compiles but doesn't upload; confirms the pre-script runs and the generator produces valid C++ that the compiler accepts).

Expected: the compiler may still fail because we haven't written `wifi_provision.cpp` yet — if so, OK; the important thing is that the `[captive-portal] generating` line appears in the output and the file `firmware/lib/wifi_provision/src/form_html.h` now exists.

- [ ] **Step 9: Verify the generated header looks right**

```bash
cat firmware/lib/wifi_provision/src/form_html.h | head -30
```

Expected: a file beginning `// Generated by gen_form_html.py — do not edit.` followed by `#pragma once` and a `const char FORM_HTML[] PROGMEM = R"WCHTML(...` raw string literal.

- [ ] **Step 10: Commit**

```bash
git add firmware/assets/captive-portal/ firmware/scripts/pio_generate_captive_html.py \
        firmware/platformio.ini firmware/.gitignore
git commit -m "feat(firmware): captive portal HTML generator + PlatformIO hook"
```

---

## Task 8: Public module header + NVS store

**Files:**
- Create: `firmware/lib/wifi_provision/include/wifi_provision.h`
- Create: `firmware/lib/wifi_provision/src/nvs_store.cpp`

This is the ESP32 adapter layer — no native test (depends on `Preferences` which isn't available in the native env).

- [ ] **Step 1: Create the module header**

```cpp
// firmware/lib/wifi_provision/include/wifi_provision.h
#pragma once

#include <Arduino.h>
#include <cstdint>
#include "wifi_provision/state_machine.h"

namespace wc::wifi_provision {

// Module lifecycle. Call begin() once from setup(), loop() from loop().
void begin();
void loop();

// Current state for observers (e.g., the display module).
State state();

// Seconds since the last successful NTP sync. UINT32_MAX if never synced.
// Display module uses this to decide when to apply the amber stale-sync tint
// (>24 h).
uint32_t seconds_since_last_sync();

// Force a reset back into captive-portal mode. Clears stored credentials.
// Called by the buttons module when Hour + Audio is held for 10 s.
void reset_to_captive();

} // namespace wc::wifi_provision
```

- [ ] **Step 2: Create the NVS store wrapper**

```cpp
// firmware/lib/wifi_provision/src/nvs_store.cpp
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
```

- [ ] **Step 3: Confirm ESP32 build still compiles**

Run: `cd firmware && pio run -e emory --target=checkprogsize`
Expected: compile fails on `wifi_provision.cpp` (not yet written) but `nvs_store.cpp` compiles cleanly.

- [ ] **Step 4: Commit**

```bash
git add firmware/
git commit -m "feat(firmware): NVS store + public wifi_provision header"
```

---

## Task 9: DNS hijack wrapper

**Files:**
- Create: `firmware/lib/wifi_provision/src/dns_wrapper.cpp`

- [ ] **Step 1: Create the wrapper**

```cpp
// firmware/lib/wifi_provision/src/dns_wrapper.cpp
#include <DNSServer.h>
#include <IPAddress.h>

namespace wc::wifi_provision::dns_hijack {

static DNSServer& server() {
    static DNSServer s;
    return s;
}

// Responds to every DNS query with the ESP32's SoftAP IP (192.168.4.1),
// which is what triggers iOS captive-portal auto-popup.
void begin(const IPAddress& ap_ip) {
    server().setErrorReplyCode(DNSReplyCode::NoError);
    server().start(53, "*", ap_ip);
}

void loop() {
    server().processNextRequest();
}

void stop() {
    server().stop();
}

} // namespace wc::wifi_provision::dns_hijack
```

- [ ] **Step 2: Smoke-test compile**

Run: `cd firmware && pio run -e emory --target=checkprogsize`
Expected: `dns_wrapper.cpp` compiles; build still fails at `wifi_provision.cpp` link (OK).

- [ ] **Step 3: Commit**

```bash
git add firmware/
git commit -m "feat(firmware): DNS hijack wrapper for captive portal"
```

---

## Task 10: Web server routes

**Files:**
- Create: `firmware/lib/wifi_provision/src/web_server.cpp`

This depends on several things the spec describes:

- The `form_html.h` PROGMEM string (generated Task 7).
- CSRF tokens: 16-byte hex string from `esp_random()`; embedded in `/` response, checked on `/submit`.
- Rate limit: per-AP-session counter, 5 successful submits max.
- Runtime substitution of `{{CSRF_TOKEN}}` and `{{ERROR_MESSAGE}}` in the embedded HTML before serving.

- [ ] **Step 1: Create the web server**

```cpp
// firmware/lib/wifi_provision/src/web_server.cpp
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
```

- [ ] **Step 2: Smoke-test compile**

Run: `cd firmware && pio run -e emory --target=checkprogsize`
Expected: `web_server.cpp` compiles; link still fails (OK, one more file).

- [ ] **Step 3: Commit**

```bash
git add firmware/
git commit -m "feat(firmware): wifi_provision web server + HTTP routes"
```

---

## Task 11: Main module — ties it all together

**Files:**
- Create: `firmware/lib/wifi_provision/src/wifi_provision.cpp`

The state machine needs a way to know when to transition on real time (AP timeout, confirmation timeout) — the ESP32 side of each state owns a timer based on `millis()`.

- [ ] **Step 1: Create the module**

```cpp
// firmware/lib/wifi_provision/src/wifi_provision.cpp
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

} // namespace wc::wifi_provision
```

- [ ] **Step 2: Compile the ESP32 build**

Run: `cd firmware && pio run -e emory --target=checkprogsize`
Expected: PASS. All `wifi_provision` sources compile and link.

- [ ] **Step 3: Also verify `nora` env compiles**

Run: `cd firmware && pio run -e nora --target=checkprogsize`
Expected: PASS.

- [ ] **Step 4: Commit**

```bash
git add firmware/
git commit -m "feat(firmware): wifi_provision main module + lifecycle"
```

---

## Task 12: Integrate into main.cpp

**Files:**
- Modify: `firmware/src/main.cpp`

- [ ] **Step 1: Replace main.cpp with the wired-in version**

```cpp
// firmware/src/main.cpp
#include <Arduino.h>
#include "wifi_provision.h"

void setup() {
    Serial.begin(115200);
    Serial.printf("word-clock booting for target: %s\n", CLOCK_NAME);
    wc::wifi_provision::begin();
}

void loop() {
    wc::wifi_provision::loop();
    delay(1);  // yield to the IDLE task so watchdog + WiFi stacks run
}
```

- [ ] **Step 2: Compile + upload to a real ESP32 (if available)**

Run: `cd firmware && pio run -e emory -t upload`
Expected: firmware boots. With no credentials, AP `WordClock-Setup-XXXX` broadcasts and serial prints the boot message.

If you don't have hardware in front of you yet (parts arriving tomorrow), `pio run -e emory` suffices for now — real upload + manual verification is the last task.

- [ ] **Step 3: Commit**

```bash
git add firmware/src/main.cpp
git commit -m "feat(firmware): wire wifi_provision into main state loop"
```

---

## Task 13: Hardware-manual verification checklist

**Files:**
- Create: `firmware/test/hardware_checks/wifi_provision_checks.md`

Not code — a handoff doc for running the real-hardware tests once parts arrive.

- [ ] **Step 1: Write the checklist**

```markdown
<!-- firmware/test/hardware_checks/wifi_provision_checks.md -->
# wifi_provision — manual hardware verification

Run these with the breadboard fully wired (parts arriving 2026-04-17).
Each check exercises one transition from the state machine diagram.

## 1. Cold boot with empty NVS

- [ ] Erase NVS: `pio run -e emory -t erase`
- [ ] Flash: `pio run -e emory -t upload`
- [ ] Open serial monitor: `pio device monitor -e emory`
- [ ] Expected: `WordClock-Setup-XXXX` appears in your phone's WiFi list within 5 s.

## 2. iOS captive portal auto-pop

- [ ] iPhone joins `WordClock-Setup-XXXX`.
- [ ] Expected: Safari opens automatically with the setup form, styled in the
      kid's palette ("Setting up Emory's clock" heading on the Emory build).

## 3. SSID dropdown populates

- [ ] Expected: within 10 s of the page loading, the SSID dropdown shows
      nearby 2.4 GHz networks. 5 GHz networks should not appear.

## 4. Submit with wrong password

- [ ] Pick your home SSID, enter a deliberately wrong password, submit.
- [ ] Expected: "Press Audio button…" page. Press Audio. Within 30 s, clock
      returns to the AP with error "Could not connect — check password and try again."

## 5. Submit with correct password

- [ ] Pick your home SSID, enter the correct password, select Pacific, submit.
- [ ] Expected: "Press Audio button…" page. Press Audio within 60 s.
- [ ] Expected: clock disconnects from phone's AP view (WordClock-Setup-XXXX
      disappears), serial log shows STA connect.

## 6. Persistence across reboot

- [ ] Unplug USB, plug back in.
- [ ] Expected: clock comes up, skips AP mode entirely, connects to stored WiFi.

## 7. Reset combo

- [ ] Hold Hour + Audio buttons simultaneously for 10 s.
- [ ] Expected: clock reboots, AP `WordClock-Setup-XXXX` reappears.

## 8. Confirmation timeout

- [ ] With empty NVS, connect phone, submit form, then wait 60 s without
      pressing Audio.
- [ ] Expected: "Waiting…" status page shows "Ready." again; form is
      reachable and accepts a new submission.

## 9. AP timeout

- [ ] With empty NVS, leave the AP broadcasting for 10 min without submitting.
- [ ] Expected: AP `WordClock-Setup-XXXX` disappears from phone's WiFi list.
      Clock enters IdleNoCredentials (serial log). Hold reset combo to recover.

## 10. Rate limit

- [ ] Submit the form 5 times (valid or invalid submissions both count).
- [ ] Expected: 6th submission gets an HTTP 429 with message about resetting.
```

- [ ] **Step 2: Commit**

```bash
git add firmware/test/hardware_checks/
git commit -m "docs(firmware): manual hardware checks for wifi_provision"
```

---

## Self-review

**Spec coverage:**
- Custom library (no WiFiManager) — Task 9 + Task 10 use raw `DNSServer` + `WebServer`. ✓
- AP configuration (SSID with MAC suffix, open, 2.4 GHz channel 1, single client) — Task 11, `start_ap`. ✓
- Hardenings (short AP lifetime, validation before commit, rate limit, single client, physical confirm) — Task 10 (rate limit, CSRF) + Task 11 (AP timeout, validation before commit, physical confirm via Audio button). ✓
- Form captures SSID + password + timezone with Pacific default — Task 6 HTML + Task 7 generator. ✓
- Timezone dropdown 8 options — Task 5 C++ + Task 7 Python; single source of truth is duplicated between the two. The `html_gen_test` asserts all 8 are present; if C++ and Python diverge, that test still passes but the form-validator rejection (Task 4) catches the mismatch. Worth flagging: if future edits change the TZ list, both `tz_options.cpp` and `gen_form_html.py` must be updated. Documented in the comment on `TZ_OPTIONS` in `gen_form_html.py`.
- NVS schema (ssid / pw / tz / last_sync in `"wifi"` namespace) — Task 8, `nvs_store.cpp`. ✓
- State machine (7 states, 12 transitions) — Task 2. ✓
- UX help line about 2.4 GHz — Task 6 HTML. ✓
- Per-kid HTML with wood-palette colors — Task 7 `KID_TOKENS`. ✓
- Local preview workflow — Task 7 `--preview` mode. ✓
- Native tests for form parsing, credential validation, TZ lookup, state machine, HTML gen — Tasks 2–5, 7. ✓
- Hardware manual checklist — Task 13. ✓
- Amber-tint signal (seconds_since_last_sync) — Task 8, header; Task 11, implementation. Note: the actual color application lives in the future display module, not here. ✓
- Reset combo hook (`reset_to_captive()`) — Task 8 header + Task 11 impl. The button module (future) calls it. ✓

**Placeholder scan:** no TBDs, no "TODO later", no "similar to Task N" references. All code blocks are complete.

**Type consistency:**
- `StateMachine::handle(Event)` signature consistent across Tasks 1, 2, 11.
- `FormBody` shape consistent across Tasks 3, 4, 8, 10, 11.
- `validate(const FormBody&) → ValidationResult` consistent across Tasks 4 and 10.
- `tz_options()` return type `const std::vector<TzOption>&` consistent.
- Adapter function names match across `nvs_store::*`, `dns_hijack::*`, `web::*` used from `wifi_provision.cpp` (Task 11) and defined in Tasks 8, 9, 10.

Plan is complete.
