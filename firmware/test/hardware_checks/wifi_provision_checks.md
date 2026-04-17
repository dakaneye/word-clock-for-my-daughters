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
