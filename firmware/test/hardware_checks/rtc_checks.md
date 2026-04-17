<!-- firmware/test/hardware_checks/rtc_checks.md -->
# rtc — manual hardware verification

Run these after the DS3231 is wired onto the breadboard per
`docs/hardware/breadboard-bring-up-guide.md` §Step 6. I²C pins
SDA=GPIO 21, SCL=GPIO 22. CR2032 installed (after the charging-
resistor removal in #1 below).

Flash: `pio run -e emory -t upload`
Serial: `pio device monitor -e emory`

## 1. Charging-resistor removal — pre-flight SAFETY CHECK

**Do this BEFORE inserting a CR2032 coin cell.** The ZS-042 DS3231
module ships with a 200 Ω + 1N4148 trickle-charge circuit intended
for rechargeable LIR2032 cells. A non-rechargeable CR2032 installed
on this circuit can **leak, vent, or explode**. See
`docs/hardware/pinout.md` §Critical issues #1.

- [ ] Locate the 200 Ω resistor near the battery holder on the
      ZS-042 PCB, in series between Vcc and battery+.
- [ ] Desolder it, cut the trace, or snip it out with diagonals.
      Verify no 200 Ω path remains between Vcc and battery+.
- [ ] Insert a fresh CR2032 (+ side up).

Do this on every DS3231 board. One per clock.

## 2. I²C probe — DS3231 detected at 0x68

- [ ] With the DS3231 wired (SDA=21, SCL=22, VCC=3.3V, GND) and
      CR2032 installed, flash: `pio run -e emory -t upload`.
- [ ] Open serial: `pio device monitor -e emory`.
- [ ] Boot log should NOT show `[rtc] ERROR: DS3231 not found on
      I²C bus`. If it does: re-seat the SDA/SCL wires, verify 3.3V
      at the DS3231 VCC pad, verify the pullup resistors on the
      DS3231 module (4.7 kΩ–10 kΩ between SDA-Vcc and SCL-Vcc).
- [ ] Also verify: no `[rtc] warning: TZ env var unset` on a warm
      boot (creds already provisioned). That warning is expected
      only during captive-portal boot; on a warm boot it means
      `setup()` ordering is wrong (rtc::begin ran before
      wifi_provision::begin).

## 3. Round-trip read/write

Temporarily patch `setup()` in `firmware/src/main.cpp` just before
the `wc::buttons::begin(...)` line:

```cpp
wc::rtc::set_from_epoch(1776542400u);  // 2026-04-17 22:00:00 UTC
```

And add inside `loop()` just before `delay(1)`:

```cpp
static uint32_t last_log = 0;
if (millis() - last_log >= 1000) {
    auto dt = wc::rtc::now();
    Serial.printf("[rtc] %04u-%02u-%02u %02u:%02u:%02u\n",
                  dt.year, dt.month, dt.day,
                  dt.hour, dt.minute, dt.second);
    last_log = millis();
}
```

Reflash.

- [ ] First log line (after Online) projects 2026-04-17 22:00:00 UTC
      through the current TZ. With Pacific PDT (-7h at that date):
      `2026-04-17 15:00:00` (or ±1 minute depending on the
      NVS-to-tzset-to-first-read delay).
- [ ] Subsequent log lines increment `second` by 1 per tick.
- [ ] After 60 s: `minute` increments from 00 to 01; `second`
      rolls back to 00.

Revert the patch when this check passes.

## 4. Button advance — hour

- [ ] Leave the logging patch from #3 in place. Remove the
      `set_from_epoch` patch. Flash.
- [ ] Note the current `hour` from the log. Press the Hour button
      (GPIO 32) once. The next log line shows `hour` incremented
      by 1; `minute`, `day`, `month`, `year` preserved.
- [ ] Press Hour 23 more times (total 24 presses). Verify the
      `hour` value returned to its initial value and `day` /
      `month` / `year` are unchanged across the full cycle.
      Confirms `(hour + 1) % 24` with no date carry.

## 5. Button advance — minute

- [ ] Press the Minute button (GPIO 33). Log shows `minute` jumps
      by 5 and rounds to a 5-minute block (e.g. from :23 → :25,
      NOT :28). `second` zeroes.
- [ ] Press Minute repeatedly until :55. Press Minute one more
      time. Log shows `minute` wraps to `:00` and `hour`
      increments by 1. Date unchanged.
- [ ] If current time is near 23:55, one more Minute press should
      produce `hour=0` on the SAME day (no date carry).

Revert the `loop()` logging patch after this check passes. Keep
the patch out of committed code.

## 6. Battery-backed retention

- [ ] After a successful NTP sync (confirmed by log showing
      wall-clock time in the correct TZ), unplug USB power.
- [ ] Wait **5 minutes**. Short outages can be carried by the
      DS3231's onboard decoupling capacitance alone; 5 minutes
      forces the CR2032 to actually source current.
- [ ] Re-plug USB. Flash monitoring open: boot log should show
      `now()` resuming approximately `(time-before-unplug) + 5min`,
      NOT 1970-01-01 or 2000-01-01 (either would indicate the
      oscillator reset). Confirms CR2032 is providing backup.

## 7. NTP handoff — deferred until ntp module ships

Once the `ntp` firmware module is wired, boot the clock on WiFi
and confirm:

- [ ] `[rtc]` log line wall-clock time matches a reference clock
      (your phone) within ~10 seconds of boot.
- [ ] Over the next 6 hours (the NTP sync cadence), the clock
      stays visually aligned with the reference. Small sub-minute
      drift is expected; hour/minute drift is a bug.

## 8. DST transition — opportunistic

Hard to time in bring-up unless a transition is near. If one is:

- [ ] Temporarily `set_from_epoch(unix_seconds_5min_before_transition)`
      using the patch from #3.
- [ ] Observe log lines across the transition boundary without
      any further writes. Local fields should jump by ±1 hour at
      the transition moment; UTC on the chip stays correct.

Skip if no transition is within bring-up's window. The spec's
open-issues item (`mktime` + `tzset` DST behavior on ESP32
newlib) is verified here when the opportunity arises.
