# Captive portal scan fix

**Status:** v2 — shipped 2026-04-20; §Bench verification findings added after on-device testing uncovered additional defects
**Scope:** delta against shipped `2026-04-16-captive-portal-design.md`. That doc remains the captive portal's design of record; this doc covers the changes shipped on 2026-04-20 and the rationale behind each.

## Problem

On bench bring-up (2026-04-20) of the shipped `wifi_provision` module, the captive portal's SSID dropdown never populated — it stayed stuck at the placeholder "Scanning for networks…". Two additional issues surfaced:

1. Only one device could be connected to the AP at a time, blocking the "switch from phone to laptop if troubleshooting" workflow.
2. The browser-side JS fires `/scan` exactly once. If the first request returns before the ESP32's async scan completes, the dropdown shows "No networks found — try again" with no actual retry path; the user has to know to reload the page.

## Root causes

### RC1 — `WiFi.mode(WIFI_AP)` disables station-mode scanning

`firmware/lib/wifi_provision/src/wifi_provision.cpp:105`:
```cpp
WiFi.mode(WIFI_AP);
```

In pure AP mode, `WiFi.scanNetworks()` does not function. It either times out (blocking mode) or returns a failure code (async mode). Confirmed via:

- [Arduino-ESP32 issue #8916: "ESP32 AP+STA and scanNetworks fails if STA not connected"](https://github.com/espressif/arduino-esp32/issues/8916)
- [ESP32 Forum: "Wifi AP scanning in AP mode"](https://esp32.com/viewtopic.php?t=2582)
- [Arduino Forum discussion](https://forum.arduino.cc/t/can-esp32-execute-a-scan-while-a-client-is-connected-to-its-ap/1259238)

The radio must have the STA side active to execute a scan; `WIFI_AP_STA` dual mode is the correct configuration.

### RC2 — `max_connection = 1` rejects a second client

`firmware/lib/wifi_provision/src/wifi_provision.cpp:111`:
```cpp
/* ssid_hidden = */ 0, /* max_connection = */ 1);
```

Only one client can connect to the AP at a time. During breadboard troubleshooting (2026-04-20), the user's laptop connected first, and the phone was unable to connect until the laptop disconnected. Raising to 4 accommodates multi-device provisioning (Mac + phone both attached during setup, or troubleshooting by device-switching).

### RC3 — Browser-side `/scan` has no polling

`firmware/assets/captive-portal/form.html.template:40-63`:
```javascript
fetch('/scan').then(r => r.json()).then(nets => { ... });
```

Fires exactly once on page load. The ESP32's async `WiFi.scanNetworks()` typically takes ~5 s to complete; plus the server's 10 s cooldown on back-to-back scans, the first client-side `/scan` often lands on a still-running scan and receives `[]`. Current JS renders "No networks found — try again" in that case with no interactive retry — the user must manually reload the page.

## Changes

### Change 1 — `WIFI_AP` → `WIFI_AP_STA`

Single-line edit in `wifi_provision.cpp:105`:

```diff
- WiFi.mode(WIFI_AP);
+ WiFi.mode(WIFI_AP_STA);
```

### Change 2 — `max_connection = 1` → `4`

Single-line edit in `wifi_provision.cpp:111`:

```diff
-             /* ssid_hidden = */ 0, /* max_connection = */ 1);
+             /* ssid_hidden = */ 0, /* max_connection = */ 4);
```

### Change 3 — `/scan` polling loop in form JS

Replace the one-shot fetch in `form.html.template:40-63` with a polling state machine:

- **Interval:** 2000 ms between `/scan` requests.
- **Timeout:** 30000 ms (15 attempts) of empty/failed responses before giving up.
- **Stop condition:** first non-empty response stops polling; dropdown stays populated for the rest of the session. Important because the server's `handle_scan()` calls `WiFi.scanDelete()` after returning results — a subsequent poll would return `[]` and, without this stop condition, erase the populated dropdown.
- **Timeout handling:** replace the dropdown contents with "No networks found — click Refresh" and inject a visible `<button>Refresh networks</button>` that, when clicked, restarts polling from zero.
- **Fetch errors:** transient network failures are treated the same as empty responses — they don't terminate polling until the 30-second timeout.

Full JS replacement is specified in the implementation plan; design excerpt shown in [review context](#design-excerpt-js-polling-loop).

## Non-goals

Explicitly **out of scope** for this delta:

- **Manual SSID entry fallback.** Useful for hidden SSIDs and robustness against scan-system regressions, but adds meaningful UI complexity. User deferred.
- **Scan-interrupts-AP beacon mitigation.** In `WIFI_AP_STA` mode, `WiFi.scanNetworks()` briefly pauses AP beacons. Existing 10-second cooldown (`SCAN_COOLDOWN_MS`) already throttles this, and stop-on-success polling further limits total scan calls to ~1-2 per session. Watch for actual disconnect symptoms during testing — not pre-emptively mitigated.
- **Refactoring `handle_scan()` for C++ native testability.** Would require extracting pure response-building from WiFi-integrated code. High refactor cost for limited gain (the WiFi-integration side is what actually failed and native tests can't cover that anyway). Skipped.

## Regression analysis

### State machine transitions — no impact

Walked the state machine (`state_machine.cpp`) and `wifi_provision.cpp` transition flows. All transitions into/out of AP-active states call `start_ap()` and `stop_ap()` which now correctly set `WIFI_AP_STA` (on enter) and `WIFI_STA` (on exit to validating/online). Transitions that stay within AP states (e.g., AwaitingConfirmation → ApActive on confirm timeout) do not re-call `start_ap()`, so no mode bounce.

| Transition | Mode before | Mode after | Impact |
|---|---|---|---|
| Boot → ApActive | — | `WIFI_AP_STA` (new) | ✓ |
| ApActive → AwaitingConfirmation | `WIFI_AP_STA` | `WIFI_AP_STA` | no change |
| AwaitingConfirmation → Validating | `WIFI_AP_STA` | `WIFI_STA` (via `stop_ap()`) | unchanged |
| Validating → Online | `WIFI_STA` | `WIFI_STA` | unchanged |
| Validating → ApActive (fail) | `WIFI_STA` | `WIFI_AP_STA` (new) | ✓ |
| Online → StaConnecting (WiFi drop) | `WIFI_STA` | `WIFI_STA` | unchanged |
| Any → IdleNoCredentials | AP or STA | `WIFI_STA` (via `stop_ap()`) | unchanged |

### AP broadcast stability — acceptable residual risk

`WiFi.scanNetworks()` briefly interrupts AP beacon activity during its ~5 s window. Mitigations:

- Existing `SCAN_COOLDOWN_MS = 10000` throttles scans to once per 10 s.
- Stop-on-success polling in Change 3 means ~1-2 scans per provisioning session in the typical path.
- Modern TCP clients recover from transient link loss via retry.

Residual risk is [LOW]; not pre-emptively mitigated. Manual hardware check #10 covers the specific scenario.

### Build-time HTML generation — survives

`gen_form_html.py` collapses the template to a single line via `_collapse_newlines()` and emits a C++ raw string literal. The new JS must:

- Use no `//` line comments (would consume the rest of the collapsed line) — verified in spec.
- Not introduce `{{...}}` sequences that would conflict with template substitution tokens — verified (single `{`/`}` for JS blocks only).
- Not introduce the raw-string delimiter `)WCHTML` — verified (trivially).

Structural pytest coverage (Tier 1 below) catches regressions on all three.

### Python pytest suite (`test_gen_form_html.py`) — passes unchanged

Reviewed all 6 existing tests. None inspect JS internals. All continue to pass after Change 3.

### Native C++ test suite (`test_wifi_*`) — passes unchanged

All four test suites (`state_machine`, `form_parser`, `credential_validator`, `tz_options`) test pure logic. None touch WiFi mode, web server, or scan. All continue to pass.

## Testing plan

Three layers.

### Tier 1 — structural pytest (required)

Add to `firmware/assets/captive-portal/tests/test_gen_form_html.py`:

- `test_polling_interval_is_2000ms` — regex for `POLL_INTERVAL_MS = 2000`
- `test_polling_timeout_is_30s` — regex for `TIMEOUT_MS = 30000`
- `test_polling_stops_on_nonempty_result` — regex for `if (nets.length > 0)` + `clearTimeout`
- `test_timeout_shows_refresh_button` — regex for `Refresh networks` button path
- `test_no_single_line_comments_in_js` — scans the template for `//` patterns outside string literals; would break `_collapse_newlines`
- `test_generated_js_syntactically_valid` — runs `node --check` on the extracted JS in the preview output (requires Node.js available in CI, which PlatformIO already needs)

Rationale: structural checks catch accidental constant-edit or logic-deletion regressions. Cheap, no new dependencies beyond what's already in the build.

### Tier 2 — Playwright behavior tests (required)

Add `firmware/assets/captive-portal/tests/test_form_behavior.py` using `playwright` + `pytest-playwright`:

- `test_dropdown_populates_on_immediate_success` — mock `/scan` returns 4 networks immediately; assert dropdown shows 4 options.
- `test_dropdown_populates_after_polling` — mock returns `[]` twice, then networks on the third call; assert dropdown populates within ~6 s.
- `test_timeout_shows_refresh_button` — mock returns `[]` forever; assert "Refresh networks" button is present after 30 s.
- `test_refresh_button_restarts_polling` — after timeout, click refresh; mock switches to returning networks; assert dropdown populates.
- `test_polling_stops_after_success` — mock returns networks on first call; assert only 1-2 `/scan` requests are made over 10 s (no extraneous polls); dropdown is NOT reset to empty.

Rationale: Tier 1 only verifies structure is present. Tier 2 verifies the state machine actually behaves. Adds `playwright` + `pytest-playwright` as dev dependencies and `playwright install chromium` as a setup step. Worth it for a 40-year keepsake.

### Tier 3 — manual hardware checks

Append to `firmware/test/hardware_checks/wifi_provision_checks.md`:

```
## 8. Multi-device connection (max_connection=4 regression)
- [ ] Connect laptop to WordClock-Setup-XXXX.
- [ ] Without disconnecting laptop, connect phone to same SSID.
- [ ] Expected: both devices show as connected; form loads on phone too.

## 9. Scan dropdown populates (WIFI_AP_STA regression)
- [ ] Load form on a fresh boot. Wait up to 30 sec.
- [ ] Expected: dropdown populates with nearby 2.4 GHz networks.
- [ ] Expected: dropdown does NOT regress to empty after displaying networks.

## 10. Timeout retry path (polling regression)
- [ ] In a location with no 2.4 GHz networks visible (or simulate by
      blocking WiFi), load form.
- [ ] Expected: after 30 sec, dropdown shows "No networks found — click Refresh"
      + visible Refresh button.
- [ ] Click Refresh → polling restarts (verify via serial showing new /scan requests).
```

## Design excerpt (JS polling loop)

For reference — the full drop-in replacement for the one-shot fetch in `form.html.template`:

```javascript
// Poll /scan every 2s until populated or 30s timeout.
const POLL_INTERVAL_MS = 2000;
const TIMEOUT_MS = 30000;
let startedAt = 0;
let timerId = null;

function renderSsidList(nets) {
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
}

function renderTimeoutRetry() {
  const sel = document.getElementById('ssid');
  sel.innerHTML = '';
  const opt = document.createElement('option');
  opt.value = '';
  opt.textContent = 'No networks found — click Refresh';
  sel.appendChild(opt);
  const btn = document.createElement('button');
  btn.type = 'button';
  btn.textContent = 'Refresh networks';
  btn.onclick = () => { btn.remove(); startPolling(); };
  sel.parentNode.insertBefore(btn, sel.nextSibling);
}

function pollScanOnce() {
  fetch('/scan').then(r => r.json()).then(nets => {
    if (nets.length > 0) {
      clearTimeout(timerId);
      timerId = null;
      renderSsidList(nets);
      return;
    }
    if (Date.now() - startedAt >= TIMEOUT_MS) {
      renderTimeoutRetry();
      return;
    }
    timerId = setTimeout(pollScanOnce, POLL_INTERVAL_MS);
  }).catch(() => {
    if (Date.now() - startedAt >= TIMEOUT_MS) {
      renderTimeoutRetry();
      return;
    }
    timerId = setTimeout(pollScanOnce, POLL_INTERVAL_MS);
  });
}

function startPolling() {
  startedAt = Date.now();
  pollScanOnce();
}

startPolling();
```

## Files touched

| File | Change |
|---|---|
| `firmware/lib/wifi_provision/src/wifi_provision.cpp` | 2 one-line edits (mode, max_connection) |
| `firmware/assets/captive-portal/form.html.template` | JS block replaced (~40 lines changed) |
| `firmware/assets/captive-portal/tests/test_gen_form_html.py` | 6 new Tier 1 tests appended |
| `firmware/assets/captive-portal/tests/test_form_behavior.py` | NEW — 5 Tier 2 Playwright tests |
| `firmware/assets/captive-portal/tests/requirements.txt` | NEW — `playwright`, `pytest-playwright` |
| `firmware/test/hardware_checks/wifi_provision_checks.md` | 3 new hardware checks appended (#8, #9, #10) |

## Open questions

None.

## Bench verification findings (2026-04-20)

On-device testing of the v1 plan on an iPhone uncovered six additional defects and one design gap. All fixed in the same commit series as the v1 scope. Documented here so the history is traceable; the original spec scope (three changes + testing plan) was correctly implemented and shipped.

### BV1 — CSRF token regenerated on every GET `/`

`render_form()` assigned `current_csrf = rand_hex(16)` on every call, including the one iOS's captive-portal assistant triggers when it re-probes `/hotspot-detect.html` in the background and follows our 302 → `/`. The user's open form tab's CSRF went stale; submit returned 400 "Let's try that again — the form timed out" despite correct input.

**Fix:** seed CSRF once in `web::begin()` and leave it stable for the AP session. Drop the regeneration from `render_form()`. Security impact is negligible — CSRF is scoped to the AP itself, which is the trust boundary for a captive portal.

**Commit:** `8a391a3` — `fix(wifi_provision): stop regenerating CSRF on every form GET`

### BV2 — UI cycling between "Press Audio" page and the form

After submitting, the iOS captive popup periodically re-fetches `/hotspot-detect.html` to detect captive-portal resolution. Our handler 302-redirects to `/`. The popup navigates back to `/` → `handle_root` served the FORM (not the waiting page) → user's "Press Audio" view was replaced with the fresh form every ~30 seconds.

**Fix:** made `handle_root` state-aware. A `submit_accepted` flag (set in `handle_submit`, cleared at each `web::begin()` call) controls which page `/` serves. Once the user has submitted, `/` serves the waiting page regardless of who's requesting it — iOS probes, manual reloads, or direct navigation all land on the stable "Press Audio" view.

**Commit:** `a87b2b2` — `fix(wifi_provision): revert grace window, add state-aware routing`

### BV3 — Mojibake in the waiting page

Em-dashes (`—`) and ellipses (`…`) in the inline HTML for the waiting page rendered as `â€"` and similar on iOS. The hand-rolled HTML didn't declare `<meta charset="utf-8">` and the response lacked a `charset=utf-8` content-type.

**Fix:** added charset header, charset meta tag, and replaced literal em-dash / ellipsis with HTML entities (`&mdash;`, plain `...`) as defense in depth.

**Commit:** `1867457` — `fix(wifi_provision): charset=utf-8 + meaningful ApActive status`

### BV4 — "Ready." default status was meaningless after failure

`confirmation_message()`'s `default:` branch returned "Ready." — displayed to the user when state is `ApActive` after a failed validation. Meaningless; user was confused about what happened.

**Fix:** added an explicit `ApActive` case returning "Didn't connect. Check your password and try again." The `/status` page only polls after a successful form POST, so reaching `ApActive` on that page unambiguously means "returned from failed validation" — safe to say so.

**Commit:** `1867457` (same as BV3)

### BV5 — Grace-window experiment broke STA reliability

Initial attempt to give the user "Connected!" feedback in the browser: keep AP up during `Validating` by using `WIFI_AP_STA` throughout, with a 5-second Online grace window before tearing down the AP. ESP32's AP_STA mode forces both sides onto the STA's channel; when `WiFi.begin()` located the home WiFi on a non-channel-1 channel, the AP was forced to move, dropping the iPhone's association. Worse, the STA connection itself became unreliable on known-good credentials in this mode.

**Fix:** reverted `start_validating()` to the pre-v1 pattern: `stop_ap()` → `WiFi.disconnect(wifioff=true)` → `WiFi.mode(WIFI_STA)` → `WiFi.begin()`. Clean mode transitions, reliable STA.

Compensate for the lost UX feedback by (a) setting `last_error` on validation failure so the form re-renders with "Didn't connect..." when the AP comes back, and (b) rewriting the waiting page to FRONT-LOAD the success signal explanation instead of burying it in fine print.

**Commit:** `a87b2b2`

### BV6 — `VALIDATING_TIMEOUT_MS` (30 s) too tight

A first-try STA handshake occasionally took longer than 30 s on a mildly congested 2.4 GHz band, producing an intermittent "Didn't connect" error. Retry almost always succeeded.

**Fix:** bumped timeout to 60 s. Pure reliability win; no downside on fast networks because success fires as soon as `WiFi.status() == WL_CONNECTED`.

**Commit:** `e829c8f` — `chore: serial_capture helper + bump validation timeout to 60s`

### BV7 — Waiting-page copy buried the success signal

Even with BV2 fixed (no more cycling), the user saw the page vanish on successful provisioning and wasn't sure whether it had worked. The fine-print explanation of "the WordClock-Setup network will disappear from your phone — that's the success signal" was too visually de-emphasized.

**Fix:** rewrote the waiting page with a highlighted success-green panel listing "What happens next" as four numbered steps, including "this page will close by itself — that's the success signal" as step 2. Previous fine-print language preserved but now acts as supporting detail.

**Commit:** `e0be85c` — `feat(captive-portal): rewrite waiting page with explicit success signal`

## Updated files touched list (rolls up v1 + bench-verification commits)

| File | Change |
|---|---|
| `firmware/lib/wifi_provision/src/wifi_provision.cpp` | WIFI_AP_STA, max_connection=4, 60s validation timeout, heartbeat log, last_error on failure |
| `firmware/lib/wifi_provision/src/web_server.cpp` | CSRF-stability, state-aware handle_root, waiting-page rewrite, charset fixes, set_error API |
| `firmware/assets/captive-portal/form.html.template` | `/scan` polling + timeout + Refresh |
| `firmware/assets/captive-portal/gen_form_html.py` | preview mock parameterized + charset-safe injection point |
| `firmware/assets/captive-portal/tests/test_gen_form_html.py` | 6 Tier 1 structural tests |
| `firmware/assets/captive-portal/tests/test_form_behavior.py` | NEW — 5 Tier 2 Playwright tests |
| `firmware/assets/captive-portal/tests/conftest.py` | NEW — Playwright fixtures + local HTTP server |
| `firmware/assets/captive-portal/tests/requirements.txt` | NEW |
| `firmware/scripts/serial_capture.py` | NEW — bench diagnostic helper |
| `firmware/test/hardware_checks/wifi_provision_checks.md` | #11-#13 appended |

## References

- Parent captive-portal spec: `docs/superpowers/specs/2026-04-16-captive-portal-design.md`
- Parent captive-portal plan (archived): `docs/archive/plans/2026-04-16-captive-portal-implementation.md`
- Bench bring-up session context: 2026-04-20. Steps 1-6 of `docs/hardware/breadboard-bring-up-guide.md` passed first-try; Step 7 revealed the original three defects (v1 scope) and surfaced the seven bench-verification findings above. Full provisioning flow verified end-to-end on iPhone with a real home WiFi network.
