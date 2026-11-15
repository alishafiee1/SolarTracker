# 020 — Sun marker on the dashboard gauge: technical spec

## 1. Summary

The yellow dot of the dashboard gauge was the panel's target, which falls back to the current angle in a fault. It becomes the sun: its place comes only from the sun position the device already sends, so faults and warnings do not move it.

## 2. Rules

- **R1** The sun angle on the gauge is `theta = atan2(cos(el) * sin(az), sin(el))` in degrees (`el`, `az` = `sunElevationDeg`, `sunAzimuthDeg`), positive to the RIGHT. East is always on the right: the same convention as the old target (`uiAngle`), the east button and the gauge labels.
- **R2** The sun sits on a dotted orbit of radius 120 around the hub (150) ; the panel range arc (+-45) is radius 96. Below the horizon (`el < 0`) it is drawn dim and dashed, behind the ground band. Below the horizon the text "زیر افق" with the depression in degrees is shown.
- **R3** Without a valid clock (`sunElevationDeg` is null) the sun is hidden, never a wrong place.
- **R4** Colours are flat and follow `el`: sky from night navy (el <= -12) through twilight to day blue (el >= 30); a warm horizon band for -6 < el < 12; sun from orange near the horizon to yellow at 30 degrees. A one-second transition moves the sun; a slow glow pulse is switched off for `prefers-reduced-motion`.
- **R5** The old target dot is removed. The target number stays in the dashboard tile.

## 3. Data

None. `/api/status` already carries `sunElevationDeg`, `sunAzimuthDeg`.

## 4. API / logic

Only `firmware/web/app.js` (`buildGauge`, `moveGauge`, new `moveSun`), `firmware/web/style.css`.

## 5. Screens

Dashboard gauge only.

## 6. Files to create or change

- `firmware/web/app.js`, `firmware/web/style.css`

## 7. Edge cases

- At night the angle passes +-180 degrees: the move is applied without animation when the jump is larger than 180.
- A fault, a warning or a missing calibration do not change the sun.

## Acceptance criteria

- **Must happen:** sunrise at Kerman is on the right, noon at the top, sunset on the left (R1); at night the dashed sun is below the horizon (R2); no clock hides it (R3).
- **Must not happen:** the sun following the panel target or the fault state (R5).
- **Done when:** the three states are checked in the browser (simulator) and `test_cycle_016*` stays green.

## Documentation Impact

- none

## Open Questions

- none

## Decisions

- Colours inside the scene are fixed (not theme variables). Because a dark sky is the whole point of the picture and the labels on it must stay readable in the light theme.
- No JS unit test; the math is checked with a node script against the firmware's sun numbers and in the browser. Because the project has no JS test runner. (default, not asked)
