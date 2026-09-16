# FreeRTOS Vehicle Body Controller

A small vehicle body-control project that separates reusable control logic from an RTOS integration layer. The project models a body control module (BCM) responsible for door locks, interior lighting, exterior lighting, hazards, and diagnostics while demonstrating common embedded-software patterns: periodic tasks, queues, mutex-protected shared state, rolling counters, message watchdogs, and deterministic host-side testing.

> **Scope:** the `firmware/` directory shows how the controller is scheduled with FreeRTOS APIs. The repository does **not** claim validation on production automotive hardware. The default CMake build compiles a host simulator and unit tests so the behavior can be reproduced without a microcontroller toolchain.

## What it demonstrates

- C++17 body-control state machine
- CAN-style 8-byte messages with rolling counters
- message freshness/watchdog checks
- door-lock, interior-light, headlight, and hazard logic
- fail-safe fault state for stale/corrupt command traffic
- FreeRTOS task decomposition using `xTaskCreate`, queues, mutexes, and `vTaskDelayUntil`
- host simulation for repeatable fault scenarios
- unit tests and GitHub Actions CI
- DBC file documenting the example CAN interface

## Architecture

```text
     Body Inputs ECU                 Driver Switch ECU
   door / ambient / key            lock / lights / hazard
            |                               |
            +----------- CAN ---------------+
                            |
                       CAN RX task
                            |
                    +-------+-------+
                    |               |
               input queue      switch queue
                    |               |
                    +-------+-------+
                            |
                      Control task
                            |
                    BodyController
                            |
                 mutex-protected output
                            |
                  +---------+---------+
                  |                   |
             CAN TX task        Diagnostics task
```

See [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) for task periods, priorities, queue ownership, and fault handling.

## Repository layout

```text
.
├── CMakeLists.txt
├── include/
│   ├── body_controller.hpp
│   └── messages.hpp
├── src/
│   └── body_controller.cpp
├── sim/
│   └── main.cpp
├── tests/
│   └── test_body_controller.cpp
├── firmware/
│   └── main_freertos.cpp
├── config/
│   └── body_control.dbc
├── docs/
│   └── ARCHITECTURE.md
├── tools/
│   └── run_scenarios.py
└── .github/workflows/ci.yml
```

## Build and run

Requirements: CMake 3.16+ and a C++17 compiler.

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Run a scenario directly:

```bash
./build/body_controller_sim nominal
./build/body_controller_sim door_open
./build/body_controller_sim stale_inputs
./build/body_controller_sim bad_counter
```

Or run all end-to-end scenarios:

```bash
python3 tools/run_scenarios.py ./build/body_controller_sim
```

## Example behavior

The controller receives two periodic message classes:

- **BodyInputs (`0x210`)**: ignition state, door-ajar state, ambient-light estimate
- **DriverSwitches (`0x211`)**: lock/unlock request, manual headlight command, hazard switch, automatic-light enable

The control loop then produces a `BodyStatus (`0x310`)` message containing commanded locks, lights, hazards, operating mode, and a diagnostic fault flag.

Faults are raised when required input traffic becomes stale or a rolling counter stops advancing. In the host model, a fault forces nonessential commanded loads off while retaining explicit hazard operation.

## FreeRTOS integration

`firmware/main_freertos.cpp` contains the RTOS-facing task design. It expects the target project to provide board-specific functions for CAN I/O and GPIO output. The task graph uses:

- `CanRxTask` — receives and decodes incoming CAN frames
- `ControlTask` — executes the controller periodically
- `CanTxTask` — publishes body status
- `DiagnosticsTask` — evaluates task liveness and controller faults

The file is intentionally excluded from the host build because FreeRTOS headers, the MCU HAL, CAN peripheral configuration, and linker/startup files depend on the selected target board.

## Engineering notes

This repository is an educational body-controller implementation, not an ISO 26262-certified component. The rolling counters, watchdogs, safe-state behavior, and task-liveness checks are included to demonstrate defensive embedded design patterns rather than to claim production safety compliance.

## License

MIT
