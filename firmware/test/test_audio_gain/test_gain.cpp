#include <unity.h>
#include <cstdint>
#include "audio/gain.h"

using namespace wc::audio;

void setUp(void)    {}
void tearDown(void) {}

void test_gain_unity_256_identity(void) {
    int16_t buf[5] = { INT16_MIN, -1, 0, 1, INT16_MAX };
    int16_t expected[5] = { INT16_MIN, -1, 0, 1, INT16_MAX };
    apply_gain_q8(buf, 5, 256);
    for (size_t i = 0; i < 5; ++i) {
        TEST_ASSERT_EQUAL_INT16(expected[i], buf[i]);
    }
}

void test_gain_half_128_halves(void) {
    int16_t buf[1] = { 32000 };
    apply_gain_q8(buf, 1, 128);
    TEST_ASSERT_EQUAL_INT16(16000, buf[0]);
}

void test_gain_zero_mutes(void) {
    int16_t buf[2] = { 32000, -32000 };
    apply_gain_q8(buf, 2, 0);
    TEST_ASSERT_EQUAL_INT16(0, buf[0]);
    TEST_ASSERT_EQUAL_INT16(0, buf[1]);
}

void test_gain_unity_at_int16_extremes(void) {
    // gain_q8=256 hits the unity early-return, so the multiply-shift path
    // is NOT exercised here. This test only proves the early-return
    // preserves extremes. The multiply-shift path at extremes is covered
    // by test_gain_half_at_int16_extremes below.
    int16_t buf[2] = { INT16_MAX, INT16_MIN };
    apply_gain_q8(buf, 2, 256);
    TEST_ASSERT_EQUAL_INT16(INT16_MAX, buf[0]);
    TEST_ASSERT_EQUAL_INT16(INT16_MIN, buf[1]);
}

void test_gain_half_at_int16_extremes(void) {
    // gain_q8=128 takes the real multiply-shift path. Verifies the
    // int32 cast + signed right-shift behavior at both boundaries.
    // INT16_MAX = 32767; 32767 * 128 = 4194176; >> 8 = 16383.
    // INT16_MIN = -32768; -32768 * 128 = -4194304; >> 8 = -16384.
    int16_t buf[2] = { INT16_MAX, INT16_MIN };
    apply_gain_q8(buf, 2, 128);
    TEST_ASSERT_EQUAL_INT16(16383,  buf[0]);
    TEST_ASSERT_EQUAL_INT16(-16384, buf[1]);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_gain_unity_256_identity);
    RUN_TEST(test_gain_half_128_halves);
    RUN_TEST(test_gain_zero_mutes);
    RUN_TEST(test_gain_unity_at_int16_extremes);
    RUN_TEST(test_gain_half_at_int16_extremes);
    return UNITY_END();
}
