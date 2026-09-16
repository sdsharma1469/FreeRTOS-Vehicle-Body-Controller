#pragma once

#include <cstdint>
#include <optional>

#include "messages.hpp"

namespace body {

struct ControllerConfig {
    std::uint32_t input_timeout_ms{250};
    std::uint32_t switch_timeout_ms{250};
    std::uint8_t dark_threshold_percent{30};
};

class BodyController {
public:
    explicit BodyController(ControllerConfig config = {});

    void receive_body_inputs(const BodyInputs& inputs, std::uint32_t now_ms);
    void receive_driver_switches(const DriverSwitches& switches, std::uint32_t now_ms);
    void tick(std::uint32_t now_ms);

    const BodyStatus& status() const noexcept { return status_; }
    bool inputs_counter_fault() const noexcept { return inputs_counter_fault_; }
    bool switches_counter_fault() const noexcept { return switches_counter_fault_; }

private:
    static bool counter_advanced(std::uint8_t previous, std::uint8_t current);
    void evaluate_faults(std::uint32_t now_ms);
    void update_outputs();

    ControllerConfig config_{};
    BodyStatus status_{};

    std::optional<BodyInputs> inputs_{};
    std::optional<DriverSwitches> switches_{};

    std::uint32_t last_inputs_ms_{0};
    std::uint32_t last_switches_ms_{0};
    bool have_inputs_{false};
    bool have_switches_{false};

    std::optional<std::uint8_t> last_inputs_counter_{};
    std::optional<std::uint8_t> last_switches_counter_{};
    bool inputs_counter_fault_{false};
    bool switches_counter_fault_{false};

    bool doors_locked_latch_{false};
};

}  // namespace body
