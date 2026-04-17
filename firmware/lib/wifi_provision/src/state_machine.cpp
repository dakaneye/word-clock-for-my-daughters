#include "wifi_provision/state_machine.h"

namespace wc::wifi_provision {

void StateMachine::handle(Event e) {
    if (e == Event::BootWithNoCredentials) {
        state_ = State::ApActive;
    }
}

} // namespace wc::wifi_provision
