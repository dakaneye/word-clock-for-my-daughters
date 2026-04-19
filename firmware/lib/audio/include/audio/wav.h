// firmware/lib/audio/include/audio/wav.h
#pragma once

#include <cstdint>
#include <cstddef>

namespace wc::audio {

// Canonical WAV constants — a file matching these is our only
// accepted format. See audio spec §WAV format validation.
constexpr uint16_t WAV_PCM_CODEC_TAG        = 0x0001;
constexpr uint16_t WAV_CHANNEL_COUNT_MONO   = 1;
constexpr uint32_t WAV_SAMPLE_RATE_HZ       = 44100;
constexpr uint16_t WAV_BITS_PER_SAMPLE      = 16;
constexpr size_t   WAV_CANONICAL_HEADER_LEN = 44;

enum class WavParseError : uint8_t {
    Ok                 = 0,
    BadRiffMagic       = 1,
    BadWaveMagic       = 2,
    BadFmtMagic        = 3,
    BadCodecTag        = 4,
    WrongChannelCount  = 5,
    WrongSampleRate    = 6,
    WrongBitsPerSample = 7,
    BadDataMagic       = 8,
    Truncated          = 9,
};

struct ParseResult {
    WavParseError error;
    uint32_t      data_offset;      // bytes from file start to PCM
    uint32_t      data_size_bytes;  // from the "data" chunk header
};

// Pure. Validates a canonical 44-byte RIFF/WAVE/fmt /data header.
// If buf_len < 44, returns Truncated. On success returns
// {Ok, 44, data_size}.
ParseResult parse_wav_header(const uint8_t* buf, size_t buf_len);

} // namespace wc::audio
