// FreeRTOS integration example for an MCU target.
//
// This file is intentionally not part of the default host CMake build. A board
// project must provide FreeRTOS, a CAN driver, GPIO/output functions, startup
// code, and the platform_* functions declared below.

#include <cstdint>

extern "C" {
#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"
#include "task.h"
}

#include "body_controller.hpp"
#include "messages.hpp"

namespace {

using body::BodyController;
using body::BodyInputs;
using body::BodyStatus;
using body::CanFrame;
using body::DriverSwitches;

// Board-specific functions supplied by the MCU/HAL layer.
extern bool platform_can_receive(CanFrame* frame, TickType_t timeout_ticks);
extern bool platform_can_transmit(const CanFrame& frame);
extern void platform_apply_body_outputs(const BodyStatus& status);
extern void platform_kick_hardware_watchdog();

QueueHandle_t g_input_queue = nullptr;
QueueHandle_t g_switch_queue = nullptr;
SemaphoreHandle_t g_status_mutex = nullptr;
BodyStatus g_status_snapshot{};

struct TaskHeartbeats {
    volatile TickType_t can_rx{0};
    volatile TickType_t control{0};
    volatile TickType_t can_tx{0};
};

TaskHeartbeats g_heartbeats{};

constexpr TickType_t kControlPeriod = pdMS_TO_TICKS(10);
constexpr TickType_t kTxPeriod = pdMS_TO_TICKS(20);
constexpr TickType_t kWatchdogPeriod = pdMS_TO_TICKS(50);
constexpr TickType_t kHeartbeatDeadline = pdMS_TO_TICKS(150);

std::uint32_t now_ms() {
    return static_cast<std::uint32_t>(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

bool heartbeat_is_fresh(TickType_t now, TickType_t beat) {
    return static_cast<TickType_t>(now - beat) <= kHeartbeatDeadline;
}

void CanRxTask(void*) {
    CanFrame frame{};
    for (;;) {
        if (platform_can_receive(&frame, pdMS_TO_TICKS(20))) {
            try {
                if (frame.id == body::kBodyInputsId) {
                    const BodyInputs inputs = body::decode_body_inputs(frame);
                    (void)xQueueOverwrite(g_input_queue, &inputs);
                } else if (frame.id == body::kDriverSwitchesId) {
                    const DriverSwitches switches = body::decode_driver_switches(frame);
                    (void)xQueueOverwrite(g_switch_queue, &switches);
                }
            } catch (...) {
                // Embedded builds normally replace exceptions with an error return
                // or a no-exceptions decoding API. This example drops malformed frames.
            }
        }
        g_heartbeats.can_rx = xTaskGetTickCount();
    }
}

void ControlTask(void*) {
    BodyController controller{};
    BodyInputs inputs{};
    DriverSwitches switches{};
    TickType_t last_wake = xTaskGetTickCount();

    for (;;) {
        if (xQueueReceive(g_input_queue, &inputs, 0) == pdPASS) {
            controller.receive_body_inputs(inputs, now_ms());
        }
        if (xQueueReceive(g_switch_queue, &switches, 0) == pdPASS) {
            controller.receive_driver_switches(switches, now_ms());
        }

        controller.tick(now_ms());
        const BodyStatus status = controller.status();
        platform_apply_body_outputs(status);

        if (xSemaphoreTake(g_status_mutex, pdMS_TO_TICKS(2)) == pdPASS) {
            g_status_snapshot = status;
            xSemaphoreGive(g_status_mutex);
        }

        g_heartbeats.control = xTaskGetTickCount();
        vTaskDelayUntil(&last_wake, kControlPeriod);
    }
}

void CanTxTask(void*) {
    TickType_t last_wake = xTaskGetTickCount();

    for (;;) {
        BodyStatus snapshot{};
        if (xSemaphoreTake(g_status_mutex, pdMS_TO_TICKS(2)) == pdPASS) {
            snapshot = g_status_snapshot;
            xSemaphoreGive(g_status_mutex);
            (void)platform_can_transmit(body::encode_body_status(snapshot));
        }

        g_heartbeats.can_tx = xTaskGetTickCount();
        vTaskDelayUntil(&last_wake, kTxPeriod);
    }
}

void WatchdogTask(void*) {
    TickType_t last_wake = xTaskGetTickCount();

    for (;;) {
        const TickType_t now = xTaskGetTickCount();
        const bool healthy = heartbeat_is_fresh(now, g_heartbeats.can_rx) &&
                             heartbeat_is_fresh(now, g_heartbeats.control) &&
                             heartbeat_is_fresh(now, g_heartbeats.can_tx);

        // Only service the hardware watchdog if the critical tasks are alive.
        // If a task deadlocks, the MCU watchdog is allowed to reset the system.
        if (healthy) {
            platform_kick_hardware_watchdog();
        }

        vTaskDelayUntil(&last_wake, kWatchdogPeriod);
    }
}

}  // namespace

extern "C" void body_controller_freertos_start() {
    g_input_queue = xQueueCreate(1, sizeof(BodyInputs));
    g_switch_queue = xQueueCreate(1, sizeof(DriverSwitches));
    g_status_mutex = xSemaphoreCreateMutex();

    configASSERT(g_input_queue != nullptr);
    configASSERT(g_switch_queue != nullptr);
    configASSERT(g_status_mutex != nullptr);

    // Priorities intentionally keep control above communication/diagnostics.
    configASSERT(xTaskCreate(ControlTask, "bcm_ctrl", 768, nullptr, 4, nullptr) == pdPASS);
    configASSERT(xTaskCreate(CanRxTask, "can_rx", 768, nullptr, 3, nullptr) == pdPASS);
    configASSERT(xTaskCreate(CanTxTask, "can_tx", 512, nullptr, 2, nullptr) == pdPASS);
    configASSERT(xTaskCreate(WatchdogTask, "bcm_wdg", 512, nullptr, 1, nullptr) == pdPASS);

    vTaskStartScheduler();
}
