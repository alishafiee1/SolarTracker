# 004 — Safe actuator motion: design

## Problem

Two parallel 220 V actuators are driven by two relays: CH1 = power, CH2 = direction (changeover contact). Hardware already makes "both directions powered" impossible. Software must still avoid: switching CH2 under load (contact wear, arcs), reversing a still-moving motor (mechanical shock), relay chatter, and endless runs.

## Proposed approach

A small explicit state machine, ticked every 100 ms:

    Idle ──move──▶ Selecting ──200 ms──▶ Running ──stop/target/timeout──▶ Settling ──settle+gap ok──▶ Idle
      ▲                │ stop                                          │
      └────────────────┘                                               └─ emergencyStop from any state

Timing table (defaults from the proposal):

| Interlock | Value | Applies |
|---|---|---|
| CH2 → CH1 delay | 200 ms | every start |
| Settle after stop | 3 s (1–10 s) | every stop |
| Reverse condition | settle + current < 0.2 A + angle stable 1 s | direction change |
| Gap between moves | 10 s | any two moves |
| Max move | 120 s | every move |

## Architecture impact

New core module `lib/core/motion/`. `ControlLoop` (001) calls it each tick; `ControlStateMachine` (005) is its only client.

## Components affected

005 (requests), 006 (wind requests via 005), 008 (manual via 005), 009 (current input).

## Data / API impact

Uses timing fields of `Settings` (007). No API exposed to the web directly.

## Trade-offs

- The 10 s gap makes manual jogging feel slow; accepted for actuator life (proposal requires it in manual mode too).
- Holding requests instead of rejecting them hides a delay from the user; the web panel shows "waiting (safety delay)" with the remaining seconds.

## Risks

- Simulator timing may differ from real actuator coast-down → verify settle time on the bench and keep it configurable.

## Alternatives considered

- Timers in each mode's code: duplicated and easy to forget in one mode; rejected.
- Hardware timer relay for delays: extra parts and cost; rejected (software + hardware interlock is enough).

## Decisions

- Single `MotionController` owning all interlocks. Because every mode must obey the same rules. (MEMORY.md)
