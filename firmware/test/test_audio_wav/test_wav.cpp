#include <unity.h>
#include <cstring>
#include "audio/wav.h"

using namespace wc::audio;

// --- Canonical WAV header fixture -------------------------------------
//
// RIFF-ChunkID      [0..3]   "RIFF"
// ChunkSize         [4..7]   little-endian uint32 (filesize - 8)
// Format            [8..11]  "WAVE"
// Subchunk1ID       [12..15] "fmt "
// Subchunk1Size     [16..19] little-endian uint32 (16 for PCM)
// AudioFormat       [20..21] little-endian uint16 (1 for PCM)
// NumChannels       [22..23] little-endian uint16
// SampleRate        [24..27] little-endian uint32
// ByteRate          [28..31] little-endian uint32 (sample_rate * channels * bits/8)
// BlockAlign        [32..33] little-endian uint16 (channels * bits/8)
// BitsPerSample     [34..35] little-endian uint16
// Subchunk2ID       [36..39] "data"
// Subchunk2Size     [40..43] little-endian uint32 (payload bytes)
static void build_canonical_header(uint8_t* h, uint32_t data_size) {
    std::memset(h, 0, 44);
    std::memcpy(h + 0,  "RIFF", 4);
    uint32_t chunk_size = data_size + 36;
    h[4]  = chunk_size & 0xFF;
    h[5]  = (chunk_size >> 8)  & 0xFF;
    h[6]  = (chunk_size >> 16) & 0xFF;
    h[7]  = (chunk_size >> 24) & 0xFF;
    std::memcpy(h + 8,  "WAVE", 4);
    std::memcpy(h + 12, "fmt ", 4);
    h[16] = 16;  // Subchunk1Size
    h[20] = 1;   // AudioFormat = PCM
    h[22] = 1;   // NumChannels = 1
    // SampleRate 44100
    h[24] = 44100 & 0xFF;
    h[25] = (44100 >> 8)  & 0xFF;
    h[26] = (44100 >> 16) & 0xFF;
    h[27] = (44100 >> 24) & 0xFF;
    // ByteRate 88200
    h[28] = 88200 & 0xFF;
    h[29] = (88200 >> 8)  & 0xFF;
    h[30] = (88200 >> 16) & 0xFF;
    h[31] = (88200 >> 24) & 0xFF;
    h[32] = 2;   // BlockAlign
    h[34] = 16;  // BitsPerSample
    std::memcpy(h + 36, "data", 4);
    h[40] = data_size & 0xFF;
    h[41] = (data_size >> 8)  & 0xFF;
    h[42] = (data_size >> 16) & 0xFF;
    h[43] = (data_size >> 24) & 0xFF;
}

void setUp(void)    {}
void tearDown(void) {}

void test_wav_truncated_buffer(void) {
    uint8_t buf[44] = {0};
    ParseResult r = parse_wav_header(buf, 43);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)WavParseError::Truncated,
                            (uint8_t)r.error);
}

void test_wav_valid_canonical(void) {
    uint8_t h[44];
    build_canonical_header(h, /*data_size=*/88200);
    ParseResult r = parse_wav_header(h, sizeof(h));
    TEST_ASSERT_EQUAL_UINT8((uint8_t)WavParseError::Ok, (uint8_t)r.error);
    TEST_ASSERT_EQUAL_UINT32(44u,    r.data_offset);
    TEST_ASSERT_EQUAL_UINT32(88200u, r.data_size_bytes);
}

void test_wav_bad_riff_magic(void) {
    uint8_t h[44];
    build_canonical_header(h, 0);
    std::memcpy(h + 0, "RIFX", 4);
    ParseResult r = parse_wav_header(h, sizeof(h));
    TEST_ASSERT_EQUAL_UINT8((uint8_t)WavParseError::BadRiffMagic,
                            (uint8_t)r.error);
}

void test_wav_bad_wave_magic(void) {
    uint8_t h[44];
    build_canonical_header(h, 0);
    std::memcpy(h + 8, "WAVX", 4);
    ParseResult r = parse_wav_header(h, sizeof(h));
    TEST_ASSERT_EQUAL_UINT8((uint8_t)WavParseError::BadWaveMagic,
                            (uint8_t)r.error);
}

void test_wav_bad_fmt_magic(void) {
    uint8_t h[44];
    build_canonical_header(h, 0);
    std::memcpy(h + 12, "fmtx", 4);
    ParseResult r = parse_wav_header(h, sizeof(h));
    TEST_ASSERT_EQUAL_UINT8((uint8_t)WavParseError::BadFmtMagic,
                            (uint8_t)r.error);
}

void test_wav_bad_codec_tag(void) {
    uint8_t h[44];
    build_canonical_header(h, 0);
    h[20] = 3;  // IEEE float, not PCM
    ParseResult r = parse_wav_header(h, sizeof(h));
    TEST_ASSERT_EQUAL_UINT8((uint8_t)WavParseError::BadCodecTag,
                            (uint8_t)r.error);
}

void test_wav_wrong_channels(void) {
    uint8_t h[44];
    build_canonical_header(h, 0);
    h[22] = 2;  // stereo
    ParseResult r = parse_wav_header(h, sizeof(h));
    TEST_ASSERT_EQUAL_UINT8((uint8_t)WavParseError::WrongChannelCount,
                            (uint8_t)r.error);
}

void test_wav_wrong_sample_rate(void) {
    uint8_t h[44];
    build_canonical_header(h, 0);
    // 48000 little-endian
    h[24] = 0x80; h[25] = 0xBB; h[26] = 0x00; h[27] = 0x00;
    ParseResult r = parse_wav_header(h, sizeof(h));
    TEST_ASSERT_EQUAL_UINT8((uint8_t)WavParseError::WrongSampleRate,
                            (uint8_t)r.error);
}

void test_wav_wrong_bits_per_sample(void) {
    uint8_t h[44];
    build_canonical_header(h, 0);
    h[34] = 24;
    ParseResult r = parse_wav_header(h, sizeof(h));
    TEST_ASSERT_EQUAL_UINT8((uint8_t)WavParseError::WrongBitsPerSample,
                            (uint8_t)r.error);
}

void test_wav_bad_data_magic(void) {
    uint8_t h[44];
    build_canonical_header(h, 0);
    std::memcpy(h + 36, "dbta", 4);
    ParseResult r = parse_wav_header(h, sizeof(h));
    TEST_ASSERT_EQUAL_UINT8((uint8_t)WavParseError::BadDataMagic,
                            (uint8_t)r.error);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_wav_truncated_buffer);
    RUN_TEST(test_wav_valid_canonical);
    RUN_TEST(test_wav_bad_riff_magic);
    RUN_TEST(test_wav_bad_wave_magic);
    RUN_TEST(test_wav_bad_fmt_magic);
    RUN_TEST(test_wav_bad_codec_tag);
    RUN_TEST(test_wav_wrong_channels);
    RUN_TEST(test_wav_wrong_sample_rate);
    RUN_TEST(test_wav_wrong_bits_per_sample);
    RUN_TEST(test_wav_bad_data_magic);
    return UNITY_END();
}
