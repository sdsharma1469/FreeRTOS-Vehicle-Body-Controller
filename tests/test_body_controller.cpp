#include <cstdlib>
#include <iostream>

#include "body_controller.hpp"
#include "messages.hpp"

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void test_can_round_trip() {
    body::BodyInputs input{};
    input.ignition_on = true;
    input.driver_door_ajar = true;
    input.ambient_light_percent = 23;
    input.counter = 7;

    const auto decoded = body::decode_body_inputs(body::encode_body_inputs(input));
    expect(decoded.ignition_on, "ignition bit should round-trip");
    expect(decoded.driver_door_ajar, "door bit should round-trip");
    expect(decoded.ambient_light_percent == 23, "ambient signal should round-trip");
    expect(decoded.counter == 7, "counter should round-trip");
}

void test_nominal_lock_and_auto_lights() {
    body::BodyController controller{};
    body::BodyInputs input{};
    body::DriverSwitches switches{};
    input.ignition_on = true;
    input.ambient_light_percent = 15;
    switches.lock_request = true;
    switches.auto_lights_enabled = true;

    controller.receive_body_inputs(input, 0);
    controller.receive_driver_switches(switches, 0);
    controller.tick(0);

    expect(!controller.status().diagnostic_fault, "nominal traffic should not fault");
    expect(controller.status().doors_locked, "lock request should latch with doors closed");
    expect(controller.status().headlights, "auto lights should turn on when ambient light is low");
    expect(controller.status().mode == body::OperatingMode::Drive, "ignition on should enter DRIVE");
}

void test_lock_rejected_with_open_door() {
    body::BodyController controller{};
    body::BodyInputs input{};
    body::DriverSwitches switches{};
    input.driver_door_ajar = true;
    switches.lock_request = true;

    controller.receive_body_inputs(input, 0);
    controller.receive_driver_switches(switches, 0);
    controller.tick(0);

    expect(!controller.status().doors_locked, "controller should not latch lock with a door ajar");
    expect(controller.status().interior_light, "interior light should turn on with a door ajar");
}

void test_unlock_overrides_lock() {
    body::BodyController controller{};
    body::BodyInputs input{};
    body::DriverSwitches switches{};
    switches.lock_request = true;

    controller.receive_body_inputs(input, 0);
    controller.receive_driver_switches(switches, 0);
    controller.tick(0);
    expect(controller.status().doors_locked, "initial lock should latch");

    input.counter = 1;
    switches.counter = 1;
    switches.unlock_request = true;
    controller.receive_body_inputs(input, 20);
    controller.receive_driver_switches(switches, 20);
    controller.tick(20);
    expect(!controller.status().doors_locked, "unlock should take precedence over simultaneous lock");
}

void test_timeout_fault_and_hazard_retention() {
    body::BodyController controller{};
    body::BodyInputs input{};
    body::DriverSwitches switches{};

    controller.receive_body_inputs(input, 0);
    controller.receive_driver_switches(switches, 0);
    controller.tick(0);

    switches.counter = 1;
    switches.hazard_switch = true;
    controller.receive_driver_switches(switches, 100);
    controller.tick(300);

    expect(controller.status().diagnostic_fault, "stale BodyInputs should raise a fault");
    expect(controller.status().mode == body::OperatingMode::Fault, "timeout should enter FAULT mode");
    expect(controller.status().hazards, "explicit hazard request should remain active in fault mode");
    expect(!controller.status().headlights, "nonessential headlight command should be cleared in fault mode");
}

void test_frozen_counter_fault() {
    body::BodyController controller{};
    body::BodyInputs input{};
    body::DriverSwitches switches{};

    controller.receive_body_inputs(input, 0);
    controller.receive_driver_switches(switches, 0);
    controller.tick(0);

    switches.counter = 1;
    controller.receive_body_inputs(input, 20);  // counter intentionally frozen at 0
    controller.receive_driver_switches(switches, 20);
    controller.tick(20);

    expect(controller.inputs_counter_fault(), "duplicate input counter should be detected");
    expect(controller.status().diagnostic_fault, "counter fault should propagate to diagnostics");
}

void test_counter_wrap_is_valid() {
    body::BodyController controller{};
    body::BodyInputs input{};
    body::DriverSwitches switches{};
    input.counter = 15;
    switches.counter = 15;

    controller.receive_body_inputs(input, 0);
    controller.receive_driver_switches(switches, 0);
    controller.tick(0);

    input.counter = 0;
    switches.counter = 0;
    controller.receive_body_inputs(input, 20);
    controller.receive_driver_switches(switches, 20);
    controller.tick(20);

    expect(!controller.inputs_counter_fault(), "15-to-0 input counter wrap should be valid");
    expect(!controller.switches_counter_fault(), "15-to-0 switch counter wrap should be valid");
    expect(!controller.status().diagnostic_fault, "valid wrap should not create diagnostic fault");
}

}  // namespace

int main() {
    test_can_round_trip();
    test_nominal_lock_and_auto_lights();
    test_lock_rejected_with_open_door();
    test_unlock_overrides_lock();
    test_timeout_fault_and_hazard_retention();
    test_frozen_counter_fault();
    test_counter_wrap_is_valid();

    if (failures != 0) {
        std::cerr << failures << " test(s) failed\n";
        return EXIT_FAILURE;
    }

    std::cout << "All body controller tests passed\n";
    return EXIT_SUCCESS;
}
