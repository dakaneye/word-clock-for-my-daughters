# SD Loader — Design

Load or update the firmware-expected audio files (`lullaby1.wav`,
`lullaby2.wav`, `birth.wav`) on the SD card inside a **closed** clock,
through the captive USB cable, then restore the clock to exactly the
firmware it was running. No screws.

Motivation: the clocks are sealed for years at a time, but the audio
content is meant to be re-recordable up to delivery (2030/2032) and beyond.
The SD card is wired to the ESP32; the ESP32 is reachable over the captive
cable; therefore SD contents are cable-serviceable — this tool makes that a
one-command operation for the next 40 years. Immediate driver: Emory's
clock is live and `birth.wav` must be real before her birthday (Oct 6).

## Locked decisions (2026-07-13, with user)

| Decision | Choice |
|---|---|
| Firmware restore | **Dump & restore**: read the full 4 MB flash image before touching anything; write it back byte-identical afterwards. No toolchain, keychain flags, or git state needed at restore time — works in 2035. |
| Input handling | **Auto-convert**: accept raw recordings in any format `afconvert` reads (Voice Memos `.m4a`, MP3, …); tool converts to spec WAV and validates. |
| UX | **One command**: stage → backup → load → restore → boot-check, fully narrated, walk-away. |
| Transfer | **WiFi pull**: loader firmware joins the home WiFi and HTTP-GETs files from a temporary server on the Mac (~1–3 MB/s, TCP integrity). Serial is the control + progress channel only. Serial *transfer* rejected: 30+ min at 115200 through a clone CP2102 with a known replay bug. |

## Components

### 1. Loader firmware — `firmware/tools/sd_loader/`

Standalone PlatformIO project (the test-sketch pattern, but under `tools/`
because it is a permanent lifecycle tool). Pins `espressif32@6.8.0` and the
`build_unflags = -std=gnu++11` line exactly like the main env (both are
load-bearing; see CLAUDE.md), and uses the same default 4 MB partition
table so the NVS partition sits at the same offset — the loader reads the
clock's stored WiFi credentials from the `wifi` namespace directly
(`ssid`/`pw` keys, same schema as `nvs_store.cpp`). The loader never
writes NVS.

Boot sequence: mount SD (same SPI pins as the audio adapter — source them
from `pinmap.h` by including the main project's core include dir), join
WiFi from NVS creds if present, then print `READY sd=<ok|fail>
wifi=<ip|none|failed>` and serve line-based serial commands at 115200.
The host sends `W` iff `wifi` is not an IP:

| Command | Effect | Responses |
|---|---|---|
| `W <ssid>\t<pass>` | Join WiFi with explicit creds (unprovisioned board, e.g. Nora at bring-up). Tab-separated — passwords may contain spaces. | `OK wifi <ip>` / `ERR wifi <reason>` |
| `G <url>\t<name>\t<size>\t<crc32>` | Stream-download `url` to `/<name>.part` on the SD, computing CRC32 while writing; then re-read the file from the card and re-CRC; on match, delete any existing `/<name>` and rename `.part` over it. | `PROG <name> <pct>` every ~5%, then `OK got <name> <size> <crc32>` / `ERR get <reason>` |
| `L` | List SD root: name, size, CRC32 per file. | `FILE <name> <size> <crc32>` ×N, then `OK list` |
| `D <name>` | Delete a file (stale test audio, orphaned `.part`). | `OK del <name>` / `ERR del <reason>` |

CRC32 is zlib polynomial on both sides (small table implementation in the
loader; `zlib.crc32` on the host). All output is human-readable — the tool
parses it, a human can read it.

### 2. Host orchestrator — `firmware/tools/sd_load.py`

The one command:

```
python3 tools/sd_load.py --lullaby1 A.m4a --lullaby2 B.m4a --birth C.m4a
python3 tools/sd_load.py ~/recordings/emory/     # dir: files named lullaby1.*, lullaby2.*,
                                                 # birth.* — whichever are present (≥1 required)
python3 tools/sd_load.py --dry-run               # pipeline proof, no real slots touched
python3 tools/sd_load.py --restore-only <backup> # recover a clock left in loader state
```

Steps, each narrated and each a hard gate:

1. **Stage** — `afconvert -f WAVE -d LEI16@44100 -c 1` each input into a
   staging dir; parse and validate the output headers (RIFF/WAVE, PCM,
   mono, 44100 Hz, 16-bit); compute CRC32 + size. Any problem aborts here,
   before the clock is touched. Partial sets are allowed (update only
   `birth.wav`) — unspecified slots are left alone on the card.
2. **Backup** — `esptool read_flash 0 0x400000` to
   `tools/.flash_backups/<UTC-timestamp>.bin` (directory gitignored;
   backups never auto-deleted). Baud 921600, falling back 460800 → 115200
   on failure (clone CP2102).
3. **Load** — flash the loader (`pio run -d tools/sd_loader -t upload`),
   start `http.server` on the staging dir bound to the Mac's LAN IP on an
   ephemeral port, open serial with the replay-safe recipe (reset-hold →
   flush → release), wait for `READY`, send `W` if the loader reports no
   NVS creds, then one `G` per file; verify every `OK got` CRC. Finish
   with `L` and print the card's final contents. `--dry-run` instead
   transfers a generated 100 KB test WAV to `dryrun.tmp` and `D`s it.
4. **Restore** — `esptool write_flash --verify 0x0 <backup>`, reset, then
   reuse the boot-capture pattern to confirm health markers (boot banner,
   no RTC error, SD mounted, frame rendered). Only then declare success.

Failure posture: once the loader has been flashed, any error triggers an
automatic restore attempt (finally-block semantics). If even that fails,
the tool prints the exact `--restore-only` command with the backup path.
The clock is never silently left as a loader.

### 3. Audible verification (separate, post-restore)

The Audio button already plays `lullaby1.wav` — that is the human check
for the lullabies. For `birth.wav`, add a `'b'` serial command to the
existing TEMP bench-sim block in `main.cpp` that plays the birthday file
on demand. It lives and dies with the other TEMP commands (stripped before
production). CRC equality remains the formal proof; ears are the comfort.

## Error handling summary

| Failure | Behavior |
|---|---|
| Bad/missing input audio, afconvert error | Abort in step 1; clock untouched |
| read_flash fails at all baud rates | Abort; clock untouched (still running its firmware) |
| Loader can't join WiFi | Reported over serial; tool restores flash and exits nonzero |
| Download CRC mismatch | Retry ×3, then restore + fail |
| Power loss mid-transfer | Only `.part` on card; existing files intact (atomic rename); next run's `D` cleans up |
| Restore write fails | Retry at lower baud; if exhausted, print `--restore-only` recovery instructions |
| Post-restore boot check fails | Exit nonzero with the backup path and capture transcript; flash image is still on disk |

## Testing

- **pytest** (`firmware/tools/test_sd_load.py`, following the repo's
  existing pytest precedents): WAV header validation, CRC/staging, serial
  response parsing, command formatting — the pure-logic half of the host
  tool, structured so it imports without pyserial/esptool present.
- **Loader firmware**: compiles standalone; verified live via `--dry-run`
  against Emory's clock — that run is the acceptance test for the whole
  pipeline and is also the moment we finally list what is on Emory's card.
- **First real use**: Emory's actual recordings, before Oct 6.

## Out of scope

- Serial file transfer fallback (WiFi is a hard prerequisite; a clock
  without WiFi in reach gets `W`-supplied credentials to any AP/hotspot).
- Reading files *off* the card (add a `P <name>` dump command later if a
  need ever appears).
- Any GUI. One command, one transcript.
