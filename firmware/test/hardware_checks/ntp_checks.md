# NTP Module — Manual Hardware Checks

These checks exercise the ESP32 paths that the native test suite
cannot reach: real NTPClient UDP exchange, real `WiFi.hostByName`
resolution, real DS3231 round-trip after `set_from_epoch`. Run
during breadboard bring-up after the WiFi (`wifi_provision_checks.md`)
and DS3231 (`rtc_checks.md`) checklists are green.

Spec: `docs/superpowers/specs/2026-04-17-ntp-design.md`
(see §Hardware manual check for source).

## Prerequisites

- ESP32 flashed with the current emory or nora env build.
- Captive-portal flow completed at least once (NVS holds valid
  WiFi creds + TZ).
- DS3231 wired and known-good (rtc_checks.md passed).
- Serial monitor open at 115200 baud.

## Checks

1. **First-ever sync.** Wipe NVS (hold Hour+Audio for 10 s →
   `reset_to_captive`). Walk through captive portal, submit creds.
   After the `[wifi_provision] state -> Online` log line, watch for
   `[ntp] sync ok, epoch=<value>, next in ~24h` within a few
   seconds. Confirm `<value>` is a plausible 2026+ timestamp
   (>= 1767225600). Confirm `wc::rtc::now()` from a follow-up sketch
   reports wall-clock time correctly in the user's TZ.

2. **Sync persists.** Power cycle. After re-`Online`, confirm
   `seconds_since_last_sync()` reflects the wait time (not
   `UINT32_MAX`), and the new sync runs per the 24h schedule (does
   NOT fire immediately unless overdue). Verify by waiting at least
   a minute past `Online` and confirming no new `[ntp] sync ok`
   line appears.

3. **WiFi drop mid-loop.** While running, kill the WiFi (turn off
   AP). Observe serial: ntp module quietly stops attempting (no
   spam). Restore WiFi. Confirm next attempt fires per the
   prevailing schedule (NOT immediately on reconnect unless
   overdue).

4. **DNS failure handling.** Block `time.google.com` at the router
   (DNS sinkhole, e.g. via Pi-hole or `/etc/hosts` of an AP-side
   resolver). Observe `[ntp] DNS resolution failed (WiFi.status=...,
   consecutive_failures=...)` in serial on next attempt. Confirm
   `consecutive_failures` increments each cycle and the cadence
   slows: 30 s → 1 m → 2 m → 4 m → 8 m → 30 m. Unblock; confirm
   next attempt succeeds and the count resets to 0.

5. **Implausible-epoch rejection.** Hard to inject naturally;
   skipped unless a custom test NTP server is set up. Documented
   so a future me knows it's covered by native tests already
   (`test_ntp_validation`).

6. **24-hour cadence sanity.** Power on. Note epoch + millis at
   first sync. Wait 24h ± 30m. Confirm a second sync fires within
   that window. (Long-running test; may be deferred to burn-in
   phase.)

7. **Stale-sync tint.** Sync once successfully. Disconnect WiFi for
   >24h (or block `time.google.com` for the duration). Confirm
   display amber tint activates per the display module's contract.
   This is a cross-module integration check; the tint logic itself
   lives in display.

## Pass criteria

Checks 1–4 must pass before considering the module hardware-verified.
5 is documented-only. 6 + 7 are deferred to burn-in.
