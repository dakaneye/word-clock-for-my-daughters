// firmware/lib/audio/src/wav.cpp
#include "audio/wav.h"

#include <cstring>

namespace wc::audio {

// Little-endian read helpers. The RIFF spec is strictly LE, and the
// daughters' clocks are run on ESP32 (LE) — but reading byte-by-byte
// keeps the parser portable and makes the native-test build work on
// any host byte order.
static uint16_t read_u16_le(const uint8_t* p) {
    return static_cast<uint16_t>(p[0]) |
           (static_cast<uint16_t>(p[1]) << 8);
}

static uint32_t read_u32_le(const uint8_t* p) {
    return  static_cast<uint32_t>(p[0])        |
           (static_cast<uint32_t>(p[1]) << 8)  |
           (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}

ParseResult parse_wav_header(const uint8_t* buf, size_t buf_len) {
    ParseResult r{};
    if (buf_len < WAV_CANONICAL_HEADER_LEN) {
        r.error = WavParseError::Truncated;
        return r;
    }
    if (std::memcmp(buf + 0,  "RIFF", 4) != 0) {
        r.error = WavParseError::BadRiffMagic;  return r;
    }
    if (std::memcmp(buf + 8,  "WAVE", 4) != 0) {
        r.error = WavParseError::BadWaveMagic;  return r;
    }
    if (std::memcmp(buf + 12, "fmt ", 4) != 0) {
        r.error = WavParseError::BadFmtMagic;   return r;
    }
    if (read_u16_le(buf + 20) != WAV_PCM_CODEC_TAG) {
        r.error = WavParseError::BadCodecTag;   return r;
    }
    if (read_u16_le(buf + 22) != WAV_CHANNEL_COUNT_MONO) {
        r.error = WavParseError::WrongChannelCount; return r;
    }
    if (read_u32_le(buf + 24) != WAV_SAMPLE_RATE_HZ) {
        r.error = WavParseError::WrongSampleRate;   return r;
    }
    if (read_u16_le(buf + 34) != WAV_BITS_PER_SAMPLE) {
        r.error = WavParseError::WrongBitsPerSample; return r;
    }
    if (std::memcmp(buf + 36, "data", 4) != 0) {
        r.error = WavParseError::BadDataMagic;  return r;
    }

    r.error           = WavParseError::Ok;
    r.data_offset     = static_cast<uint32_t>(WAV_CANONICAL_HEADER_LEN);
    r.data_size_bytes = read_u32_le(buf + 40);
    return r;
}

} // namespace wc::audio
