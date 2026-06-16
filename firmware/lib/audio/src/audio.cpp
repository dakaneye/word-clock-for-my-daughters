// firmware/lib/audio/src/audio.cpp
//
// ESP32 adapter — guarded with #ifdef ARDUINO so PlatformIO LDF's
// deep+ mode doesn't drag SD/driver-i2s into the native build.
// Same pattern as the wifi_provision / buttons / display / rtc / ntp
// adapters.
#ifdef ARDUINO

#if !defined(ARDUINO_ARCH_ESP32)
  #error "audio adapter requires the Arduino-ESP32 framework (ARDUINO_ARCH_ESP32 not defined)"
#endif

#include <Arduino.h>
#include <SD.h>
#include <Preferences.h>
#include <driver/i2s.h>
#include <cstdint>
#include "audio.h"              // pulls in audio/fire_guard.h transitively
#include "audio/playback.h"
#include "audio/wav.h"
#include "audio/gain.h"
#include "rtc.h"
#include "wifi_provision.h"
#include "pinmap.h"

namespace wc::audio {

static constexpr i2s_port_t   I2S_PORT          = I2S_NUM_0;
static constexpr size_t       SD_READ_CHUNK     = 1024;   // bytes; 512 samples
// File paths now live in audio/playback.h: LULLABY_ONE_PATH, LULLABY_TWO_PATH, BIRTH_PATH.
static constexpr const char*  NVS_NAMESPACE     = "audio";
static constexpr const char*  NVS_KEY_LAST_YEAR = "last_birth_year";

// Compile-time guard for CLOCK_AUDIO_GAIN_Q8 lives in audio/gain.h
// (which defines a default of 256 if no config header overrides it).

// State and Track are defined in audio/playback.h.

static bool        started              = false;
static bool        sd_ok                = false;
static bool        i2s_ok               = false;
static State       state_               = State::Idle;
static Track       track_               = Track::None;
static BirthConfig birth_               = {};
static uint16_t    last_birth_year_     = 0;
static File        current_file_;
static uint32_t    bytes_played_        = 0;
static uint32_t    data_remaining_      = 0;   // bytes left in the WAV data chunk
static uint8_t     read_buf_[SD_READ_CHUNK];

// --- NVS helpers -------------------------------------------------

static uint16_t nvs_read_last_birth_year() {
    Preferences p;
    if (!p.begin(NVS_NAMESPACE, /*readOnly=*/true)) return 0;
    uint16_t v = p.getUShort(NVS_KEY_LAST_YEAR, 0);
    p.end();
    return v;
}

// Returns true iff BOTH begin() opened writable AND putUShort returned
// a non-zero byte count. Auto-fire gates the birth.wav transition on
// this bool — a silent NVS miss would risk missing-or-double-fire
// across a power cycle.
static bool nvs_write_last_birth_year(uint16_t year) {
    Preferences p;
    if (!p.begin(NVS_NAMESPACE, /*readOnly=*/false)) return false;
    size_t written = p.putUShort(NVS_KEY_LAST_YEAR, year);
    p.end();
    return written > 0;
}

// --- I²S driver lifecycle ---------------------------------------

static bool i2s_install() {
    i2s_config_t cfg = {};
    cfg.mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
    cfg.sample_rate          = WAV_SAMPLE_RATE_HZ;
    cfg.bits_per_sample      = I2S_BITS_PER_SAMPLE_16BIT;
    cfg.channel_format       = I2S_CHANNEL_FMT_ONLY_LEFT;
    cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
    cfg.dma_buf_count        = 4;
    cfg.dma_buf_len          = 1024;    // units: samples (NOT bytes); 16-bit mono so 1024 samples = 2048 B/buffer

    cfg.use_apll             = false;
    cfg.tx_desc_auto_clear   = true;
    if (i2s_driver_install(I2S_PORT, &cfg, 0, nullptr) != ESP_OK) return false;

    i2s_pin_config_t pins = {};
    pins.bck_io_num   = PIN_I2S_BCLK;
    pins.ws_io_num    = PIN_I2S_LRC;
    pins.data_out_num = PIN_I2S_DIN;
    pins.data_in_num  = I2S_PIN_NO_CHANGE;
    if (i2s_set_pin(I2S_PORT, &pins) != ESP_OK) return false;

    i2s_zero_dma_buffer(I2S_PORT);
    return true;
}

// --- State transitions -------------------------------------------

// Open path on SD, validate WAV header, seek to data, reset byte counter.
// Returns true if the file is ready to pump samples; false on any error.
// Caller owns the state machine update — this function does NOT mutate
// state_ / track_.
static bool open_file_for_playback(const char* path) {
    current_file_ = SD.open(path, FILE_READ);
    if (!current_file_) {
        Serial.printf("[audio] error: file %s not found\n", path);
        return false;
    }
    uint8_t header[WAV_CANONICAL_HEADER_LEN];
    size_t  got = current_file_.read(header, sizeof(header));
    ParseResult hdr = parse_wav_header(header, got);
    if (hdr.error != WavParseError::Ok) {
        Serial.printf("[audio] error: wav header invalid (code=%u) for %s\n",
                      static_cast<unsigned>(hdr.error), path);
        current_file_.close();
        return false;
    }
    current_file_.seek(hdr.data_offset);
    bytes_played_   = 0;
    data_remaining_ = hdr.data_size_bytes;   // stop at the data chunk, not file EOF
    Serial.printf("[audio] play %s\n", path);
    return true;
}

// Drive the state machine for the given event. Calls next_transition()
// from the pure-logic playback module, executes the returned action,
// and updates state_/track_ on success. On file-open failure, drops to
// Idle as a safe fallback.
static void dispatch_event(PlaybackEvent::Kind kind) {
    PlaybackTransition t = next_transition(state_, track_, PlaybackEvent{kind});
    using A = PlaybackTransition::Action;

    bool io_ok = true;
    switch (t.action) {
    case A::None:
        // No I/O. State/track may still update (e.g. Idle stop is a no-op
        // but stays Idle). Apply below.
        break;
    case A::OpenFile:
        io_ok = open_file_for_playback(t.path);
        break;
    case A::CloseFile: {
        if (current_file_) current_file_.close();
        // Cut the DMA immediately only on an explicit stop. On natural
        // end-of-file let the ~93 ms already queued in the I²S DMA play
        // out (tx_desc_auto_clear fills silence after) instead of
        // truncating the final note.
        if (kind == PlaybackEvent::Kind::StopRequested) {
            i2s_zero_dma_buffer(I2S_PORT);
        }
        const char* reason =
            (kind == PlaybackEvent::Kind::FileEnded)    ? "finished" :
            (kind == PlaybackEvent::Kind::StopRequested) ? "stopped"  :
                                                            "closed";
        Serial.printf("[audio] %s (played %u bytes)\n",
                      reason, static_cast<unsigned>(bytes_played_));
        break;
    }
    case A::SwitchFile:
        if (current_file_) {
            Serial.printf("[audio] switching (played %u bytes)\n",
                          static_cast<unsigned>(bytes_played_));
            current_file_.close();
        }
        // A birthday interrupting a playing lullaby must not bleed the
        // lullaby's queued DMA tail into birth.wav — cut it. A normal
        // lullaby1 -> lullaby2 advance (FileEnded) keeps the tail for a
        // gapless transition.
        if (kind == PlaybackEvent::Kind::BirthdayFired) {
            i2s_zero_dma_buffer(I2S_PORT);
        }
        io_ok = open_file_for_playback(t.path);
        break;
    }

    if (!io_ok) {
        // SD open / WAV parse failed — bail to Idle.
        i2s_zero_dma_buffer(I2S_PORT);
        state_ = State::Idle;
        track_ = Track::None;
        return;
    }

    state_ = t.next_state;
    track_ = t.next_track;
}

// --- Public API --------------------------------------------------

void begin(const BirthConfig& birth) {
    if (started) return;
    birth_ = birth;

    i2s_ok = i2s_install();
    if (!i2s_ok) {
        Serial.println("[audio] begin: I2S install FAILED — audio disabled until next boot");
    }
    // Log BEFORE the SD.begin call so a corrupt-FAT hang (SD.begin
    // spinning on SPI bus polling) is diagnosable from serial — the
    // post-call line below won't emit if begin never returns.
    Serial.println("[audio] begin: mounting SD...");
    sd_ok = SD.begin(PIN_SD_CS);
    if (!sd_ok) {
        Serial.println("[audio] begin: SD FAILED, playback disabled until next boot");
    } else {
        Serial.println("[audio] begin: SD ok, I2S ok");
    }
    last_birth_year_ = nvs_read_last_birth_year();
    started = true;
}

void loop() {
    if (!started) return;

    // SD remount retry: a mount failure at boot would otherwise latch
    // audio off until the next power cycle. Retry on a slow cadence so a
    // late / reseated card recovers. SD.begin() can block on a bad bus,
    // so never retry faster than every 30 s.
    if (!sd_ok) {
        static uint32_t last_sd_retry_ms = 0;
        uint32_t nowm = millis();
        if (last_sd_retry_ms == 0 || nowm - last_sd_retry_ms >= 30'000u) {
            last_sd_retry_ms = nowm;
            sd_ok = SD.begin(PIN_SD_CS);
            if (sd_ok) Serial.println("[audio] SD remounted");
        }
    }

    // Auto-fire check — a birthday can interrupt a playing lullaby (spec
    // design decision 1), so it is NOT gated on Idle. Throttled: it only
    // needs minute resolution, and rtc::now() is a blocking I²C read —
    // running it every ~1 ms loop tick for the device's life loads the
    // bus and adds jitter. A 10 s cadence still catches the 60 s
    // birth-minute window.
    if (sd_ok && i2s_ok) {
        static uint32_t last_autofire_ms = 0;
        uint32_t nowm = millis();
        if (last_autofire_ms == 0 || nowm - last_autofire_ms >= 10'000u) {
            last_autofire_ms = nowm;
            auto dt = wc::rtc::now();
            NowFields nf{ dt.year, dt.month, dt.day, dt.hour, dt.minute };
            bool time_known =
                (wc::wifi_provision::seconds_since_last_sync() != UINT32_MAX);
            // already_playing=false: birthday fires even mid-lullaby.
            if (should_auto_fire(nf, birth_, last_birth_year_,
                                 time_known, /*already_playing=*/false)) {
                Serial.printf("[audio] birthday fire (year=%u)\n", nf.year);
                dispatch_event(PlaybackEvent::Kind::BirthdayFired);
                // Stamp the NVS year ONLY after birth.wav actually opened
                // (state_ == Playing). A missing/corrupt birth.wav drops
                // dispatch to Idle; leaving the year unstamped lets the
                // next check retry until the message really plays once,
                // rather than silently burning the birthday.
                if (state_ == State::Playing) {
                    if (!nvs_write_last_birth_year(nf.year)) {
                        Serial.println("[audio] warning: NVS birth-year stamp failed");
                    }
                    last_birth_year_ = nf.year;
                }
                return;  // don't pump in the same tick as the switch
            }
        }
    }

    // Pump audio if Playing.
    if (state_ == State::Playing) {
        size_t want = sizeof(read_buf_);
        if (data_remaining_ < want) want = data_remaining_;
        if (want == 0) {                   // reached end of the WAV data chunk
            dispatch_event(PlaybackEvent::Kind::FileEnded);
            return;
        }
        int n = current_file_.read(read_buf_, want);
        if (n <= 0) {
            // want>0 means data_remaining_>0, so a read returning <=0 is NOT a
            // clean end (that goes through the want==0 branch above) — it's a
            // truncated file or, the realistic 40-year case, a card that
            // dropped out mid-playback. Stop cleanly (no playlist advance) and
            // drop sd_ok so the remount retry re-establishes the card instead
            // of latching audio off until the next reboot.
            Serial.println("[audio] read fault before data end; remounting SD");
            dispatch_event(PlaybackEvent::Kind::StopRequested);
            sd_ok = false;
            SD.end();
            return;
        }
        n &= ~1;                           // whole 16-bit samples only
        if (n == 0) {
            dispatch_event(PlaybackEvent::Kind::FileEnded);
            return;
        }
        apply_gain_q8(reinterpret_cast<int16_t*>(read_buf_),
                      static_cast<size_t>(n) / 2,
                      CLOCK_AUDIO_GAIN_Q8);
        size_t written = 0;
        esp_err_t err = i2s_write(I2S_PORT, read_buf_,
                                  static_cast<size_t>(n),
                                  &written, /*ticks=*/0);
        if (err != ESP_OK) {
            // A real I²S fault — not a benign full DMA buffer, which
            // returns ESP_OK with written==0. Stop cleanly instead of
            // treating it as FileEnded, which would advance the playlist
            // to the next track.
            Serial.printf("[audio] error: i2s_write err=%d — stopping\n", err);
            dispatch_event(PlaybackEvent::Kind::StopRequested);
            return;
        }
        if (written < static_cast<size_t>(n)) {
            // DMA was full; re-read the unwritten source bytes next tick.
            current_file_.seek(current_file_.position() - (n - written));
        }
        data_remaining_ -= written;
        bytes_played_   += written;
    }
}

void play_lullaby() {
    if (!started || !sd_ok || !i2s_ok) return;
    dispatch_event(PlaybackEvent::Kind::PlayLullabyRequested);
}

void stop() {
    if (!started) return;
    dispatch_event(PlaybackEvent::Kind::StopRequested);
}

bool is_playing() {
    return started && state_ == State::Playing;
}

} // namespace wc::audio

#endif // ARDUINO
