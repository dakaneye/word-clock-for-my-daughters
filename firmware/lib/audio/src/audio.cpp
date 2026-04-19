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
#include "audio/wav.h"
#include "audio/gain.h"
#include "rtc.h"
#include "wifi_provision.h"
#include "pinmap.h"

namespace wc::audio {

static constexpr i2s_port_t   I2S_PORT          = I2S_NUM_0;
static constexpr size_t       SD_READ_CHUNK     = 1024;   // bytes; 512 samples
static constexpr const char*  LULLABY_PATH      = "/lullaby.wav";
static constexpr const char*  BIRTH_PATH        = "/birth.wav";
static constexpr const char*  NVS_NAMESPACE     = "audio";
static constexpr const char*  NVS_KEY_LAST_YEAR = "last_birth_year";

// Compile-time guard for CLOCK_AUDIO_GAIN_Q8 lives in audio/gain.h
// (which defines a default of 256 if no config header overrides it).

enum class State : uint8_t { Idle, Playing };

static bool        started              = false;
static bool        sd_ok                = false;
static State       state_               = State::Idle;
static BirthConfig birth_               = {};
static uint16_t    last_birth_year_     = 0;
static File        current_file_;
static uint32_t    bytes_played_        = 0;
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

static void transition_to_playing(const char* path) {
    current_file_ = SD.open(path, FILE_READ);
    if (!current_file_) {
        Serial.printf("[audio] error: file %s not found\n", path);
        return;  // stay Idle
    }
    uint8_t header[WAV_CANONICAL_HEADER_LEN];
    size_t  got = current_file_.read(header, sizeof(header));
    ParseResult hdr = parse_wav_header(header, got);
    if (hdr.error != WavParseError::Ok) {
        Serial.printf("[audio] error: wav header invalid (code=%u)\n",
                      static_cast<unsigned>(hdr.error));
        current_file_.close();
        return;  // stay Idle
    }
    current_file_.seek(hdr.data_offset);  // safety; should already be here
    bytes_played_ = 0;
    state_ = State::Playing;
    Serial.printf("[audio] play %s\n", path);
}

static void transition_to_idle(const char* reason) {
    if (current_file_) current_file_.close();
    i2s_zero_dma_buffer(I2S_PORT);
    state_ = State::Idle;
    Serial.printf("[audio] %s (played %u bytes)\n",
                  reason, static_cast<unsigned>(bytes_played_));
}

// --- Public API --------------------------------------------------

void begin(const BirthConfig& birth) {
    if (started) return;
    birth_ = birth;

    if (!i2s_install()) {
        Serial.println("[audio] begin: I2S install FAILED (subsequent i2s_write calls will fail)");
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

    if (state_ == State::Playing) {
        // Pump: read chunk, gain, non-blocking write.
        int n = current_file_.read(read_buf_, sizeof(read_buf_));
        if (n <= 0) {
            transition_to_idle("finished");
            return;
        }
        // Round down to even: 16-bit PCM is 2 bytes/sample. Canonical
        // WAVs always yield even reads, but a truncated/corrupt file
        // could return odd n; we'd otherwise feed half a sample into
        // I²S. Discard at most 1 trailing byte.
        n &= ~1;
        if (n == 0) {
            transition_to_idle("finished");
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
            Serial.printf("[audio] error: i2s_write err=%d\n", err);
            transition_to_idle("aborted");
            return;
        }
        if (written < static_cast<size_t>(n)) {
            // Ring full — rewind the SD cursor so we retry next tick.
            current_file_.seek(current_file_.position() - (n - written));
        }
        bytes_played_ += written;
        return;
    }

    // Idle: check birthday auto-fire.
    if (!sd_ok) return;
    // audio::loop() reads rtc::now() directly here — deliberate
    // deviation from the display module's data-injection pattern
    // (display takes NowFields via RenderInput from main.cpp). The
    // auto-fire check runs only when Idle and only needs RTC once
    // per minute-ish; pushing it through main.cpp would make
    // main.cpp read rtc on every tick whether audio cares or not.
    auto dt = wc::rtc::now();
    NowFields nf{ dt.year, dt.month, dt.day, dt.hour, dt.minute };
    bool time_known =
        (wc::wifi_provision::seconds_since_last_sync() != UINT32_MAX);
    // state_ is Idle here by construction (outer guard returned on Playing).
    // Pass false literally — should_auto_fire's already_playing param is
    // for callers who might invoke it from a non-idle context.
    if (should_auto_fire(nf, birth_, last_birth_year_,
                         time_known, /*already_playing=*/false)) {
        Serial.printf("[audio] play birth.wav (year=%u)\n", nf.year);
        // Stamp NVS FIRST, gate playback on the write succeeding.
        if (!nvs_write_last_birth_year(nf.year)) {
            Serial.println("[audio] error: NVS stamp FAILED; skipping birth.wav this tick");
            return;
        }
        last_birth_year_ = nf.year;
        transition_to_playing(BIRTH_PATH);
    }
}

void play_lullaby() {
    if (!started || !sd_ok)    return;
    if (state_ != State::Idle) return;
    transition_to_playing(LULLABY_PATH);
}

void stop() {
    if (!started)                  return;
    if (state_ != State::Playing)  return;
    transition_to_idle("stopped");
}

bool is_playing() {
    return started && state_ == State::Playing;
}

} // namespace wc::audio

#endif // ARDUINO
