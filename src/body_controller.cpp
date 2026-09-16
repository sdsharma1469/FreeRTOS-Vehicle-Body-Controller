#include "body_controller.hpp"

namespace body {

BodyController::BodyController(ControllerConfig config) : config_(config) {}

bool BodyController::counter_advanced(std::uint8_t previous, std::uint8_t current) {
    const auto expected = static_cast<std::uint8_t>((previous + 1U) & 0x0FU);
    return static_cast<std::uint8_t>(current & 0x0FU) == expected;
}

void BodyController::receive_body_inputs(const BodyInputs& inputs, std::uint32_t now_ms) {
    if (last_inputs_counter_.has_value() &&
        !counter_advanced(*last_inputs_counter_, inputs.counter)) {
        inputs_counter_fault_ = true;
    }

    last_inputs_counter_ = static_cast<std::uint8_t>(inputs.counter & 0x0FU);
    inputs_ = inputs;
    last_inputs_ms_ = now_ms;
    have_inputs_ = true;
}

void BodyController::receive_driver_switches(const DriverSwitches& switches,
                                             std::uint32_t now_ms) {
    if (last_switches_counter_.has_value() &&
        !counter_advanced(*last_switches_counter_, switches.counter)) {
        switches_counter_fault_ = true;
    }

    last_switches_counter_ = static_cast<std::uint8_t>(switches.counter & 0x0FU);
    switches_ = switches;
    last_switches_ms_ = now_ms;
    have_switches_ = true;
}

void BodyController::evaluate_faults(std::uint32_t now_ms) {
    const bool inputs_stale = !have_inputs_ ||
        (now_ms - last_inputs_ms_ > config_.input_timeout_ms);
    const bool switches_stale = !have_switches_ ||
        (now_ms - last_switches_ms_ > config_.switch_timeout_ms);

    status_.diagnostic_fault = inputs_stale || switches_stale ||
        inputs_counter_fault_ || switches_counter_fault_;
}

void BodyController::update_outputs() {
    if (!inputs_.has_value() || !switches_.has_value()) {
        status_.mode = OperatingMode::Fault;
        status_.doors_locked = doors_locked_latch_;
        status_.interior_light = false;
        status_.headlights = false;
        status_.hazards = false;
        return;
    }

    const BodyInputs& in = *inputs_;
    const DriverSwitches& sw = *switches_;
    const bool any_door_ajar = in.driver_door_ajar || in.passenger_door_ajar;

    if (!status_.diagnostic_fault) {
        if (sw.unlock_request) {
            doors_locked_latch_ = false;
        } else if (sw.lock_request && !any_door_ajar) {
            doors_locked_latch_ = true;
        }
    }

    if (status_.diagnostic_fault) {
        status_.mode = OperatingMode::Fault;
        status_.doors_locked = doors_locked_latch_;
        status_.interior_light = false;
        status_.headlights = false;
        status_.hazards = sw.hazard_switch;
        return;
    }

    status_.doors_locked = doors_locked_latch_;
    status_.interior_light = any_door_ajar;
    status_.headlights = sw.manual_headlights ||
        (sw.auto_lights_enabled && in.ambient_light_percent <= config_.dark_threshold_percent);
    status_.hazards = sw.hazard_switch;
    status_.mode = in.ignition_on ? OperatingMode::Drive : OperatingMode::Off;
}

void BodyController::tick(std::uint32_t now_ms) {
    evaluate_faults(now_ms);
    update_outputs();
    status_.counter = static_cast<std::uint8_t>((status_.counter + 1U) & 0x0FU);
}

}  // namespace body
