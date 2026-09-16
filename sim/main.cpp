#include <cstdint>
#include <iostream>
#include <string>

#include "body_controller.hpp"

namespace {

const char* mode_name(body::OperatingMode mode) {
    switch (mode) {
        case body::OperatingMode::Off: return "OFF";
        case body::OperatingMode::Accessory: return "ACCESSORY";
        case body::OperatingMode::Drive: return "DRIVE";
        case body::OperatingMode::Fault: return "FAULT";
    }
    return "UNKNOWN";
}

void print_status(const std::string& scenario, const body::BodyStatus& status) {
    std::cout << "scenario=" << scenario
              << " mode=" << mode_name(status.mode)
              << " fault=" << status.diagnostic_fault
              << " locked=" << status.doors_locked
              << " interior=" << status.interior_light
              << " headlights=" << status.headlights
              << " hazards=" << status.hazards
              << '\n';
}

int run(const std::string& scenario) {
    body::BodyController controller{};
    body::BodyInputs inputs{};
    body::DriverSwitches switches{};

    inputs.ignition_on = true;
    inputs.ambient_light_percent = 70;
    inputs.counter = 0;
    switches.auto_lights_enabled = true;
    switches.counter = 0;

    if (scenario == "nominal") {
        inputs.ambient_light_percent = 20;
        switches.lock_request = true;
        controller.receive_body_inputs(inputs, 0);
        controller.receive_driver_switches(switches, 0);
        controller.tick(0);
    } else if (scenario == "door_open") {
        inputs.driver_door_ajar = true;
        switches.lock_request = true;
        controller.receive_body_inputs(inputs, 0);
        controller.receive_driver_switches(switches, 0);
        controller.tick(0);
    } else if (scenario == "stale_inputs") {
        controller.receive_body_inputs(inputs, 0);
        controller.receive_driver_switches(switches, 0);
        controller.tick(0);

        switches.counter = 1;
        switches.hazard_switch = true;
        controller.receive_driver_switches(switches, 100);
        controller.tick(300);
    } else if (scenario == "bad_counter") {
        controller.receive_body_inputs(inputs, 0);
        controller.receive_driver_switches(switches, 0);
        controller.tick(0);

        // A frozen counter represents a repeated/stale CAN producer.
        inputs.counter = 0;
        switches.counter = 1;
        controller.receive_body_inputs(inputs, 20);
        controller.receive_driver_switches(switches, 20);
        controller.tick(20);
    } else {
        std::cerr << "unknown scenario: " << scenario << '\n';
        return 2;
    }

    print_status(scenario, controller.status());
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    const std::string scenario = argc > 1 ? argv[1] : "nominal";
    return run(scenario);
}
