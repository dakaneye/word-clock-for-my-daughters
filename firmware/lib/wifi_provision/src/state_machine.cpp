#include "wifi_provision/state_machine.h"

namespace wc::wifi_provision {

void StateMachine::handle(Event e) {
    switch (state_) {
        case State::Boot:
            if (e == Event::BootWithNoCredentials) state_ = State::ApActive;
            else if (e == Event::BootWithCredentials) state_ = State::StaConnecting;
            break;
        case State::StaConnecting:
            if (e == Event::StaConnected) state_ = State::Online;
            // StaFailed: stay in StaConnecting (retry driven by caller)
            break;
        case State::ApActive:
            if (e == Event::FormSubmitted) state_ = State::AwaitingConfirmation;
            else if (e == Event::ApTimeout) state_ = State::IdleNoCredentials;
            break;
        case State::AwaitingConfirmation:
            if (e == Event::AudioButtonConfirmed) state_ = State::Validating;
            else if (e == Event::ConfirmationTimeout) state_ = State::ApActive;
            break;
        case State::Validating:
            if (e == Event::ValidationSucceeded) state_ = State::Online;
            else if (e == Event::ValidationFailed) state_ = State::ApActive;
            break;
        case State::Online:
            if (e == Event::WiFiDropped) state_ = State::StaConnecting;
            else if (e == Event::ResetCombo) state_ = State::ApActive;
            break;
        case State::IdleNoCredentials:
            if (e == Event::ResetCombo) state_ = State::ApActive;
            break;
    }
}

} // namespace wc::wifi_provision
