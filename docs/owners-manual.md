# Word Clock — Owner's Manual

One of two hand-built word clocks — Emory's (2030) and Nora's (2032).
Hardwood shell, 63 LEDs behind a laser-cut letter face, built to run for
40 years. This manual covers everything a caretaker needs; nothing inside
requires a soldering iron.

## Reading the clock

The face spells the time in words ("IT IS TWENTY TO EIGHT IN THE
EVENING"), updating each minute to the nearest five.

- **Evenings and nights (7 PM – 8 AM)** the letters dim to about a
  quarter brightness so they don't light up the room during sleep.
- **Amber-tinted letters** mean the clock hasn't been able to check the
  time over WiFi for 48+ hours. It still keeps good time on its internal
  battery clock — amber is a nudge to check the WiFi, not an alarm.
- **Holidays** bring special color palettes on their dates.
- **On her birthday**, at the minute she was born (Emory 6:10 PM,
  Nora 9:17 AM), the clock lights HAPPY-BIRTH-DAY with her name in
  rainbow and plays Dad's recorded birthday message. Once a year.

## First-time setup (or after moving / changing WiFi)

1. Plug the clock's USB-C cable into a power brick rated **3 A (15 W) or
   better**.
2. If the clock has no WiFi saved, it creates its own network named
   **WordClock-Setup-XXXX**. Join it from a phone; a setup page opens by
   itself (if not, browse to `192.168.4.1`).
3. Pick the home WiFi, enter its password, choose the timezone, submit.
4. **Press the AUDIO button on the back of the clock** when the page asks —
   this proves a person with hands on the clock approved the change.
5. The clock joins the WiFi, fetches the time, and starts telling it.
   Done — it needs nothing else, ever, day to day.

## The three buttons (back panel, engraved labels)

| Button | Press | Does |
|---|---|---|
| **AUDIO** | once | Play or stop the lullaby (two tracks, alternating) |
| **HOUR** | once | Advance the time one hour (for offline use — normally WiFi keeps time automatically) |
| **MIN** | once | Advance the time one minute |
| **HOUR + AUDIO** | hold 10 seconds | Forget the saved WiFi and reopen the setup network (step 2 above) |

## Battery replacement — every 5 years

The internal clock battery (a common **CR2032** coin cell) only works
during power outages, so it lasts many years — replace on a 5-year
calendar reminder rather than waiting for it to die.

1. Unplug the clock and lay it face-down on something soft.
2. Remove the **4 corner screws** on the back panel and lift the panel
   straight off. The inner plastic plate and the three button caps stay
   attached to it — **nothing falls out**.
3. The coin cell sits in a holder on the small clock board (the one with
   the battery). Slide the old cell out, press the new one in **+ side
   facing out of the holder**.
4. Before closing: press gently on each of the small plug-in boards to
   confirm they're firmly seated in their sockets (the clock board came
   loose once during assembly — a firm press is cheap insurance).
5. Set the panel back on, drive the 4 screws snug in a criss-cross
   order — firm, not gorilla-tight.
6. Plug in. The clock reconnects to WiFi and fixes its own time. Set the
   next calendar reminder for +5 years.

If a corner screw ever stops biting, drive it 0.5 mm deeper with a fresh
pilot hole or step up one screw length — the hardwood wall has plenty of
material.

## Troubleshooting

| Symptom | Likely cause → fix |
|---|---|
| Face completely dark | No power — check brick, outlet, and cable seating |
| Letters amber-tinted | No WiFi sync for 48 h — check the router; if the WiFi itself changed, hold HOUR+AUDIO 10 s and redo setup |
| Time wrong after an outage | Wait a minute for WiFi sync; if it stays wrong, the coin cell is likely dead — replace it (above) |
| No sound on AUDIO press | Open the back (4 screws) and reseat the SD card and the small audio board |
| A button feels gritty or stuck | Press it firmly a few times to seat it; if it persists, open the back and check the cap moves freely in its guide |
| Setup page never appears | Browse to `192.168.4.1` manually while joined to the WordClock-Setup network |

## Spare parts and repairs

Every custom part is reproducible — sources and print files live in this
repository under `enclosure/` (printed parts), `hardware/` (circuit
board), and `firmware/` (the program).

- **Button caps** (the only custom part likely to ever wear): spares were
  printed with the originals. To swap one: back panel off, circuit board
  lifted off its four posts, press the old cap's neck from the outside of
  the panel until it pops out of its socket, snap the new one in.
- **Coin cell**: CR2032, any brand.
- **Re-recording the audio** never requires opening the clock: the
  recordings load over the USB cable using the loader tool in the
  project repository (`firmware/tools/sd_load.py`). The back only
  comes off for hardware — battery, or a physically failing card.
- **Power**: any USB-C brick, 3 A / 15 W or better.

## Vital statistics

| | |
|---|---|
| Size | 192 × 192 mm face, hardwood shell |
| Power | 5 V USB-C, 3 A brick recommended; captive cable exits the back |
| Brain | ESP32; reprogrammable over the same USB-C cable, no disassembly |
| Time | WiFi (NTP) with battery-backed clock (DS3231) as keeper |
| Display | 63 addressable LEDs, one per word, warm white |
| Sound | Two recorded lullabies + a birthday message, from an SD card inside |
