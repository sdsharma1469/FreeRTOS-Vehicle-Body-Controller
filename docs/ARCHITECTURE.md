# Architecture

## Goal

The project models a small vehicle body control module while keeping the reusable control policy independent from any specific MCU or CAN peripheral. The same `BodyController` logic is exercised by the host simulator/tests and called from the FreeRTOS control task on a target.

## Task model

| Task | Nominal behavior | Priority | Communication |
|---|---|---:|---|
| `ControlTask` | Runs controller and applies outputs every 10 ms | 4 | consumes latest input/switch queues; writes status snapshot |
| `CanRxTask` | Blocks on CAN receive and decodes accepted frames | 3 | overwrites length-1 queues with newest values |
| `CanTxTask` | Publishes `BodyStatus` every 20 ms | 2 | reads mutex-protected status snapshot |
| `WatchdogTask` | Checks task heartbeats every 50 ms | 1 | services hardware watchdog only if critical tasks are alive |

The queues have length one because the controller needs the newest command/state, not a backlog of obsolete samples. `xQueueOverwrite()` makes this behavior explicit.

## Shared-state ownership

The controller object itself is owned exclusively by `ControlTask`; no mutex is required around it. A separate `BodyStatus` snapshot is shared with `CanTxTask` and protected with a mutex. This avoids locking the time-critical controller while CAN transmission occurs.

## Scheduling

`ControlTask`, `CanTxTask`, and `WatchdogTask` use `vTaskDelayUntil()` instead of relative delays. Periodic scheduling therefore remains tied to the original wake time rather than accumulating task-execution jitter every cycle.

The example task periods are:

- control: **10 ms**
- status transmission: **20 ms**
- watchdog supervision: **50 ms**
- task heartbeat deadline: **150 ms**

These values are demonstrative rather than production timing requirements.

## CAN interface

### `0x210 BodyInputs`

- byte 0 bit 0: ignition on
- byte 0 bit 1: driver door ajar
- byte 0 bit 2: passenger door ajar
- byte 1: ambient light, 0–100%
- byte 7 low nibble: rolling counter

### `0x211 DriverSwitches`

- byte 0 bit 0: lock request
- byte 0 bit 1: unlock request
- byte 0 bit 2: manual headlights
- byte 0 bit 3: hazard switch
- byte 0 bit 4: automatic lights enabled
- byte 7 low nibble: rolling counter

### `0x310 BodyStatus`

- byte 0 bit 0: doors locked
- byte 0 bit 1: interior light
- byte 0 bit 2: headlights
- byte 0 bit 3: hazards
- byte 0 bit 4: diagnostic fault
- byte 1: operating mode
- byte 7 low nibble: rolling counter

## Control behavior

The host implementation demonstrates several simple policies:

- lock requests latch only when both modeled doors are closed;
- unlock takes precedence if lock and unlock are requested simultaneously;
- opening either door turns on the interior light;
- automatic headlights activate below a configurable ambient-light threshold;
- manual headlights bypass the ambient-light decision;
- hazards follow an explicit driver switch.

## Communication diagnostics

Each incoming message uses a 4-bit rolling counter. After the first accepted message, the next expected value is `(previous + 1) mod 16`. A repeated or skipped counter raises a diagnostic fault in this educational model.

Both required message classes also have a freshness timeout. If either has not been received within 250 ms, the controller enters `FAULT` mode. Nonessential light commands are cleared while an explicit hazard request remains honored and the most recent door-lock latch is preserved.

The communication diagnostic is deliberately small and understandable. Real automotive networks may use CRCs, alive counters with configurable acceptance windows, E2E protection profiles, bus-off handling, diagnostic trouble codes, and recovery strategies not implemented here.

## Task watchdog

Every critical task updates a heartbeat tick. `WatchdogTask` services the platform hardware watchdog only if all required task heartbeats are newer than 150 ms. A deadlock or stalled task therefore prevents the kick and allows the hardware watchdog to reset the MCU.

## Host vs target build

The default CMake project builds only portable C++ sources:

- reusable controller logic;
- deterministic simulator;
- unit tests.

`firmware/main_freertos.cpp` is an integration template for a board project with FreeRTOS and a target HAL. It declares the following target-provided hooks:

- `platform_can_receive`
- `platform_can_transmit`
- `platform_apply_body_outputs`
- `platform_kick_hardware_watchdog`

Keeping these functions at the boundary makes it possible to port the controller to STM32, NXP, ESP32, or another target without embedding one vendor HAL into the reusable control layer.

## Safety scope

This project demonstrates defensive embedded-software patterns but is **not** a functional-safety implementation or ISO 26262 work product. Production vehicle software would require requirements traceability, safety analysis, coding-standard compliance, target-specific timing analysis, hardware validation, independent verification, and substantially more robust diagnostics.
