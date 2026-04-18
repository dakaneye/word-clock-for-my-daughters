// firmware/lib/ntp/src/validation.cpp
#include "ntp/validation.h"

namespace wc::ntp {

bool is_plausible_epoch(uint32_t unix_epoch) {
    return unix_epoch >= MIN_PLAUSIBLE_EPOCH;
}

} // namespace wc::ntp
