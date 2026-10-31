# 016 — Parameter help: technical spec

## 1. Summary

Every field the panel lets the user change has a help text, opened with a button next to the field.

## 2. Rules

- **R1** `HELP[key]` in `app.js` holds the Persian text of every field of the settings, network and notification forms: what it is, the unit, the allowed range, the default and what changes when it is raised or lowered.
- **R2** Each text is checked against `settings.cpp` (range and default) by a host test that reads both files, so the help cannot drift from the validation.
- **R3** The button (`؟`, at least 44 px high) sits at the left end of the field row in the right-to-left layout; a tap opens or closes the text under the field; one open at a time is not required.

## 3. Data

None.

## 4. API / logic

None; static text in the panel.

## 5. Screens

The help buttons and texts.

## 6. Files to create or change

- `firmware/web/app.js`, `firmware/web/style.css`
- `firmware/test/test_cycle_016_help/test_main.cpp` (compares ranges and defaults in the help texts with `settings.cpp`)

## 7. Edge cases

- A field added later without a help text fails the test.

## Acceptance criteria

- **Must happen:** every field has a help button; the text names the same range and default as the validation.
- **Must not happen:** a help text that contradicts the limits.
- **Done when:** host tests pass and the panel shows the help.

## Documentation Impact

- none

## Open Questions

- none

## Decisions

- Help text lives in the panel file, not on the device API. Because it is static and the 150 KB budget is far away.
