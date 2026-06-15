// firmware/lib/rtc/src/rtc.cpp
//
// Guarded with #ifdef ARDUINO — same pattern as wifi_provision /
// buttons / display adapters. PlatformIO LDF's deep+ mode would
// otherwise pull this TU into the native test build, where RTClib
// and Arduino.h don't exist.
#ifdef ARDUINO

#if !defined(ARDUINO_ARCH_ESP32)
  #error "rtc requires the Arduino-ESP32 framework (ARDUINO_ARCH_ESP32 not defined)"
#endif

#include <Arduino.h>
#include <Wire.h>
#include <RTClib.h>
#include <time.h>
#include <sys/time.h>   // settimeofday() — seed the system clock from the DS3231
#include <cstdlib>
#include "rtc.h"
#include "rtc/advance.h"
#include "rtc/date_time.h"
#include "rtc/epoch.h"
#include "pinmap.h"     // PIN_I2C_SDA, PIN_I2C_SCL

namespace wc::rtc {

static RTC_DS3231 ds3231;
static bool started = false;

// Interpret DateTime fields as LOCAL per the current TZ and return
// the corresponding UTC epoch. mktime respects tzset + POSIX DST
// rule when tm_isdst = -1. Only called from the advance paths
// (button-press frequency) — never from the hot render path.
// mktime reads the TZ state but does not mutate it, and
// wifi_provision::start_sta() calls tzset from the main-loop only,
// so no concurrent-mutation race applies here.
static time_t local_fields_to_utc_epoch(const DateTime& local) {
    struct tm t{};
    t.tm_year  = local.year - 1900;
    t.tm_mon   = local.month - 1;
    t.tm_mday  = local.day;
    t.tm_hour  = local.hour;
    t.tm_min   = local.minute;
    t.tm_sec   = local.second;
    t.tm_isdst = -1;
    return mktime(&t);
}

static DateTime from_tm(const struct tm& t) {
    return DateTime{
        static_cast<uint16_t>(t.tm_year + 1900),
        static_cast<uint8_t>(t.tm_mon + 1),
        static_cast<uint8_t>(t.tm_mday),
        static_cast<uint8_t>(t.tm_hour),
        static_cast<uint8_t>(t.tm_min),
        static_cast<uint8_t>(t.tm_sec),
    };
}

void begin() {
    if (started) return;
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
    if (!ds3231.begin()) {
        Serial.println("[rtc] ERROR: DS3231 not found on I²C bus");
        // Intentionally continue — lets the rest of the firmware
        // keep running on bad-time rather than halting.
    } else if (ds3231.lostPower()) {
        // OSF set: the oscillator stopped (chip never set, or the coin
        // cell died). The stored time is the 2000-01-01 default / garbage,
        // so do NOT seed the system clock from it — leave time(nullptr)
        // boot-relative so the render gate stays blank until NTP re-syncs
        // rather than painting a wrong year.
        Serial.println("[rtc] DS3231 lost power — time invalid, awaiting NTP");
    } else {
        // Seed the ESP32 system clock from the battery-backed DS3231 so
        // time(nullptr) returns a real UTC epoch from the very first loop,
        // BEFORE any NTP sync this session. Without this, a warm boot while
        // WiFi is unavailable leaves wifi_provision::seconds_since_last_sync()
        // pinned at UINT32_MAX and main.cpp blanks the face forever — even
        // though the DS3231 holds correct time — breaking the documented
        // "free-run on the DS3231 during WiFi drops" promise.
        ::DateTime n = ds3231.now();
        if (n.isValid()) {
            struct timeval tv{};
            tv.tv_sec  = static_cast<time_t>(n.unixtime());  // chip stores UTC
            tv.tv_usec = 0;
            settimeofday(&tv, nullptr);
        }
    }
    if (getenv("TZ") == nullptr) {
        Serial.println("[rtc] warning: TZ env var unset at begin() "
                       "— expected only during captive-portal boot; "
                       "if seen on warm boot, setup() ordering is wrong");
    }
    started = true;
}

DateTime now() {
    static DateTime last_good{2000, 1, 1, 0, 0, 0};
    static bool     have_good = false;

    ::DateTime dt_utc = ds3231.now();       // RTClib's DateTime
    if (!dt_utc.isValid()) {
        // An I²C NAK / bus glitch leaves RTClib's read buffer
        // uninitialized, and isValid() rejects the out-of-range fields.
        // Return the last good reading (the face freezes for a tick)
        // rather than decoding garbage and painting a wrong time.
        return have_good ? last_good : DateTime{2000, 1, 1, 0, 0, 0};
    }
    DateTime utc{
        static_cast<uint16_t>(dt_utc.year()),
        static_cast<uint8_t>(dt_utc.month()),
        static_cast<uint8_t>(dt_utc.day()),
        static_cast<uint8_t>(dt_utc.hour()),
        static_cast<uint8_t>(dt_utc.minute()),
        static_cast<uint8_t>(dt_utc.second()),
    };
    // Pure-arithmetic UTC fields → UTC epoch. No TZ mutation; safe
    // to call concurrently with the WiFi task's time functions.
    uint32_t utc_epoch = utc_epoch_from_fields(utc);
    time_t t = static_cast<time_t>(utc_epoch);
    struct tm local_tm{};
    localtime_r(&t, &local_tm);             // reads TZ state, no mutation
    last_good  = from_tm(local_tm);
    have_good  = true;
    return last_good;
}

void set_from_epoch(uint32_t unix_seconds) {
    ds3231.adjust(::DateTime(unix_seconds));
}

void advance_hour() {
    DateTime local    = now();
    DateTime advanced = advance_hour_fields(local);
    time_t   new_utc  = local_fields_to_utc_epoch(advanced);
    ds3231.adjust(::DateTime(static_cast<uint32_t>(new_utc)));
}

void advance_minute() {
    DateTime local    = now();
    DateTime advanced = advance_minute_fields(local);
    time_t   new_utc  = local_fields_to_utc_epoch(advanced);
    ds3231.adjust(::DateTime(static_cast<uint32_t>(new_utc)));
}

} // namespace wc::rtc

#endif // ARDUINO
