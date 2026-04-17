#include <unity.h>
#include "wifi_provision/state_machine.h"

using namespace wc::wifi_provision;

void setUp(void) {}
void tearDown(void) {}

static StateMachine machineIn(State s) {
    // Convenience constructor-style helper — drive the machine into the
    // requested state via a deterministic sequence of events.
    StateMachine m;
    switch (s) {
        case State::Boot: break;
        case State::ApActive:
            m.handle(Event::BootWithNoCredentials); break;
        case State::StaConnecting:
            m.handle(Event::BootWithCredentials); break;
        case State::AwaitingConfirmation:
            m.handle(Event::BootWithNoCredentials);
            m.handle(Event::FormSubmitted); break;
        case State::Validating:
            m.handle(Event::BootWithNoCredentials);
            m.handle(Event::FormSubmitted);
            m.handle(Event::AudioButtonConfirmed); break;
        case State::Online:
            m.handle(Event::BootWithCredentials);
            m.handle(Event::StaConnected); break;
        case State::IdleNoCredentials:
            m.handle(Event::BootWithNoCredentials);
            m.handle(Event::ApTimeout); break;
    }
    return m;
}

void test_boot_no_creds_to_ap(void) {
    auto m = machineIn(State::Boot);
    m.handle(Event::BootWithNoCredentials);
    TEST_ASSERT_EQUAL(State::ApActive, m.state());
}

void test_boot_with_creds_to_sta_connecting(void) {
    auto m = machineIn(State::Boot);
    m.handle(Event::BootWithCredentials);
    TEST_ASSERT_EQUAL(State::StaConnecting, m.state());
}

void test_sta_connecting_to_online_on_connect(void) {
    auto m = machineIn(State::StaConnecting);
    m.handle(Event::StaConnected);
    TEST_ASSERT_EQUAL(State::Online, m.state());
}

void test_sta_connecting_stays_on_fail(void) {
    auto m = machineIn(State::StaConnecting);
    m.handle(Event::StaFailed);
    TEST_ASSERT_EQUAL(State::StaConnecting, m.state());
}

void test_ap_active_to_awaiting_on_submit(void) {
    auto m = machineIn(State::ApActive);
    m.handle(Event::FormSubmitted);
    TEST_ASSERT_EQUAL(State::AwaitingConfirmation, m.state());
}

void test_ap_active_to_idle_on_timeout(void) {
    auto m = machineIn(State::ApActive);
    m.handle(Event::ApTimeout);
    TEST_ASSERT_EQUAL(State::IdleNoCredentials, m.state());
}

void test_awaiting_to_validating_on_button(void) {
    auto m = machineIn(State::AwaitingConfirmation);
    m.handle(Event::AudioButtonConfirmed);
    TEST_ASSERT_EQUAL(State::Validating, m.state());
}

void test_awaiting_back_to_ap_on_timeout(void) {
    auto m = machineIn(State::AwaitingConfirmation);
    m.handle(Event::ConfirmationTimeout);
    TEST_ASSERT_EQUAL(State::ApActive, m.state());
}

void test_validating_to_online_on_success(void) {
    auto m = machineIn(State::Validating);
    m.handle(Event::ValidationSucceeded);
    TEST_ASSERT_EQUAL(State::Online, m.state());
}

void test_validating_back_to_ap_on_fail(void) {
    auto m = machineIn(State::Validating);
    m.handle(Event::ValidationFailed);
    TEST_ASSERT_EQUAL(State::ApActive, m.state());
}

void test_online_to_sta_connecting_on_drop(void) {
    auto m = machineIn(State::Online);
    m.handle(Event::WiFiDropped);
    TEST_ASSERT_EQUAL(State::StaConnecting, m.state());
}

void test_online_to_ap_on_reset_combo(void) {
    auto m = machineIn(State::Online);
    m.handle(Event::ResetCombo);
    TEST_ASSERT_EQUAL(State::ApActive, m.state());
}

void test_idle_to_ap_on_reset_combo(void) {
    auto m = machineIn(State::IdleNoCredentials);
    m.handle(Event::ResetCombo);
    TEST_ASSERT_EQUAL(State::ApActive, m.state());
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_boot_no_creds_to_ap);
    RUN_TEST(test_boot_with_creds_to_sta_connecting);
    RUN_TEST(test_sta_connecting_to_online_on_connect);
    RUN_TEST(test_sta_connecting_stays_on_fail);
    RUN_TEST(test_ap_active_to_awaiting_on_submit);
    RUN_TEST(test_ap_active_to_idle_on_timeout);
    RUN_TEST(test_awaiting_to_validating_on_button);
    RUN_TEST(test_awaiting_back_to_ap_on_timeout);
    RUN_TEST(test_validating_to_online_on_success);
    RUN_TEST(test_validating_back_to_ap_on_fail);
    RUN_TEST(test_online_to_sta_connecting_on_drop);
    RUN_TEST(test_online_to_ap_on_reset_combo);
    RUN_TEST(test_idle_to_ap_on_reset_combo);
    return UNITY_END();
}
