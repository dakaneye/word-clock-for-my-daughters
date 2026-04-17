#pragma once

namespace wc::wifi_provision {

enum class State {
    Boot,
    StaConnecting,
    ApActive,
    AwaitingConfirmation,
    Validating,
    Online,
    IdleNoCredentials,
};

enum class Event {
    BootWithNoCredentials,
    BootWithCredentials,
    StaConnected,
    StaFailed,
    FormSubmitted,
    AudioButtonConfirmed,
    ConfirmationTimeout,
    ValidationSucceeded,
    ValidationFailed,
    ApTimeout,
    ResetCombo,
    WiFiDropped,
};

class StateMachine {
public:
    void handle(Event e);
    State state() const { return state_; }

private:
    State state_ = State::Boot;
};

} // namespace wc::wifi_provision
