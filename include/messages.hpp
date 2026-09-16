#pragma once

#include <array>
#include <cstdint>
#include <stdexcept>

namespace body {

struct CanFrame {
    std::uint32_t id{0};
    std::array<std::uint8_t, 8> data{};
    std::uint8_t dlc{8};
};

constexpr std::uint32_t kBodyInputsId = 0x210;
constexpr std::uint32_t kDriverSwitchesId = 0x211;
constexpr std::uint32_t kBodyStatusId = 0x310;

struct BodyInputs {
    bool ignition_on{false};
    bool driver_door_ajar{false};
    bool passenger_door_ajar{false};
    std::uint8_t ambient_light_percent{100};
    std::uint8_t counter{0};
};

struct DriverSwitches {
    bool lock_request{false};
    bool unlock_request{false};
    bool manual_headlights{false};
    bool hazard_switch{false};
    bool auto_lights_enabled{true};
    std::uint8_t counter{0};
};

enum class OperatingMode : std::uint8_t {
    Off = 0,
    Accessory = 1,
    Drive = 2,
    Fault = 3,
};

struct BodyStatus {
    bool doors_locked{false};
    bool interior_light{false};
    bool headlights{false};
    bool hazards{false};
    bool diagnostic_fault{false};
    OperatingMode mode{OperatingMode::Off};
    std::uint8_t counter{0};
};

inline CanFrame encode_body_inputs(const BodyInputs& msg) {
    CanFrame frame{};
    frame.id = kBodyInputsId;
    frame.data[0] = static_cast<std::uint8_t>(
        (msg.ignition_on ? 0x01U : 0x00U) |
        (msg.driver_door_ajar ? 0x02U : 0x00U) |
        (msg.passenger_door_ajar ? 0x04U : 0x00U));
    frame.data[1] = msg.ambient_light_percent;
    frame.data[7] = static_cast<std::uint8_t>(msg.counter & 0x0FU);
    return frame;
}

inline BodyInputs decode_body_inputs(const CanFrame& frame) {
    if (frame.id != kBodyInputsId || frame.dlc != 8) {
        throw std::invalid_argument("invalid BodyInputs frame");
    }
    BodyInputs msg{};
    msg.ignition_on = (frame.data[0] & 0x01U) != 0U;
    msg.driver_door_ajar = (frame.data[0] & 0x02U) != 0U;
    msg.passenger_door_ajar = (frame.data[0] & 0x04U) != 0U;
    msg.ambient_light_percent = frame.data[1];
    msg.counter = static_cast<std::uint8_t>(frame.data[7] & 0x0FU);
    return msg;
}

inline CanFrame encode_driver_switches(const DriverSwitches& msg) {
    CanFrame frame{};
    frame.id = kDriverSwitchesId;
    frame.data[0] = static_cast<std::uint8_t>(
        (msg.lock_request ? 0x01U : 0x00U) |
        (msg.unlock_request ? 0x02U : 0x00U) |
        (msg.manual_headlights ? 0x04U : 0x00U) |
        (msg.hazard_switch ? 0x08U : 0x00U) |
        (msg.auto_lights_enabled ? 0x10U : 0x00U));
    frame.data[7] = static_cast<std::uint8_t>(msg.counter & 0x0FU);
    return frame;
}

inline DriverSwitches decode_driver_switches(const CanFrame& frame) {
    if (frame.id != kDriverSwitchesId || frame.dlc != 8) {
        throw std::invalid_argument("invalid DriverSwitches frame");
    }
    DriverSwitches msg{};
    msg.lock_request = (frame.data[0] & 0x01U) != 0U;
    msg.unlock_request = (frame.data[0] & 0x02U) != 0U;
    msg.manual_headlights = (frame.data[0] & 0x04U) != 0U;
    msg.hazard_switch = (frame.data[0] & 0x08U) != 0U;
    msg.auto_lights_enabled = (frame.data[0] & 0x10U) != 0U;
    msg.counter = static_cast<std::uint8_t>(frame.data[7] & 0x0FU);
    return msg;
}

inline CanFrame encode_body_status(const BodyStatus& msg) {
    CanFrame frame{};
    frame.id = kBodyStatusId;
    frame.data[0] = static_cast<std::uint8_t>(
        (msg.doors_locked ? 0x01U : 0x00U) |
        (msg.interior_light ? 0x02U : 0x00U) |
        (msg.headlights ? 0x04U : 0x00U) |
        (msg.hazards ? 0x08U : 0x00U) |
        (msg.diagnostic_fault ? 0x10U : 0x00U));
    frame.data[1] = static_cast<std::uint8_t>(msg.mode);
    frame.data[7] = static_cast<std::uint8_t>(msg.counter & 0x0FU);
    return frame;
}

}  // namespace body
