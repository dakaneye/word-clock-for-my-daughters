// firmware/lib/ntp/include/ntp/validation.h
#pragma once

#include <cstdint>

namespace wc::ntp {

// 2026-01-01 00:00:00 UTC. Lowest plausible sync result; anything
// earlier is a packet error or the Y2106 wrap.
constexpr uint32_t MIN_PLAUSIBLE_EPOCH = 1767225600u;

// Pure. Returns true iff unix_epoch >= MIN_PLAUSIBLE_EPOCH. No
// upper bound — uint32 unix epoch maxes at 2106-02-07.
bool is_plausible_epoch(uint32_t unix_epoch);

} // namespace wc::ntp
