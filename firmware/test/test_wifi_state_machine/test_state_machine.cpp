#include <unity.h>
#include "wifi_provision/state_machine.h"

using namespace wc::wifi_provision;

void setUp(void) {}
void tearDown(void) {}

void test_boot_with_no_credentials_transitions_to_ap_active(void) {
    StateMachine sm;
    sm.handle(Event::BootWithNoCredentials);
    TEST_ASSERT_EQUAL(State::ApActive, sm.state());
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_boot_with_no_credentials_transitions_to_ap_active);
    return UNITY_END();
}
