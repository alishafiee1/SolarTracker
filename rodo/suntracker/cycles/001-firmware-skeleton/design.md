# 001 — Firmware skeleton and HAL: design

> The overall firmware architecture. Later cycles refer to this file instead of repeating it.

## Problem

The controller drives two 220 V actuators. A software bug must never power both directions, never move the panel unexpectedly after a reset, and the movement logic must be testable before the hardware is installed (proposal phase D1). The proposal gives 27 h for the base firmware, so the structure must be simple.

## Proposed approach

Three layers:

1. **core** (`lib/core/`): pure C++ — sun position (002), angle math (003), `MotionController` (004), `ControlStateMachine` (005), wind filter (006), settings validation (007). No Arduino, no globals, time passed in.
2. **hal** (`lib/hal/`): small interfaces for every sensor/actuator/storage.
3. **drivers** (`lib/drivers/`) for the ESP32 and **sim** (`lib/sim/`) for the host. `main.cpp` only wires drivers into core.

Data flow per 100 ms tick:

    sensors (HAL) → AngleFilter / WindFilter → ControlStateMachine → MotionController → IRelays
                                                     ↑                     ↓
                                    commands from web (queue)     events → IEventLog, Status

The web server (async, its own task) never calls core directly: it reads a `Status` snapshot and pushes `Command`s into a single-producer queue consumed at the start of the next tick.

## Architecture impact

New `firmware/` tree. Everything else in the repo is untouched.

## Components affected

All future firmware cycles (002–013) build on these layers and interfaces.

## Data / API impact

None yet; `Settings`, `Event` and the REST API are defined in 007 and 008.

## Trade-offs

- HAL indirection costs a few virtual calls per tick — negligible at 10 Hz — and buys host tests and the simulator.
- A single control thread is easy to reason about but every driver call in the tick must be non-blocking (I2C reads are ~1 ms; fine).

## Risks

- ESPAsyncWebServer and LittleFS versions on Arduino-ESP32 change often → pin library versions in `platformio.ini`.
- Native builds diverge from device builds if core code uses platform types → CI-style rule: `lib/core` compiles with `-Wall -Wextra -Werror` on native.

## Alternatives considered

- **ESP-IDF directly**: more control (rollback, tasks) but slower to write; rejected for budget.
- **MicroPython**: quick to prototype, but weak timing guarantees and harder OTA of a single image; rejected.
- **One FreeRTOS task per subsystem**: more concurrency bugs around the relays; rejected.

## Decisions

- Layered core / hal / drivers / sim with a 100 ms cooperative control tick. Because it makes the safety logic testable on the host and keeps one owner for the relays. (also in MEMORY.md)
