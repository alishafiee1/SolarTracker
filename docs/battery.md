# Battery and mains sensing (cycle 023)

Hardware: 1 Li-ion cell, charger module + protection board on it, boost converter to 5 V into the ESP32 VIN.

## Wiring (two dividers, ESP32 pins are 3.3 V max)

| What | From | To pin | Parts |
|---|---|---|---|
| Battery voltage | cell + (after the protection board) | GPIO35 | 100 kΩ from cell + to the pin, 100 kΩ from the pin to GND, 100 nF across the lower resistor |
| Charger input (mains present) | IN+ of the charger module (the 5 V side) | GPIO36 | 10 kΩ from IN+ to the pin, 10 kΩ from the pin to GND, 100 nF across the lower resistor |

Both pins are ADC1 and input-only (ADC2 conflicts with Wi-Fi). The ratio 1:2 is in `src/main.cpp` (`kBatteryDividerRatio`, `kChargerDividerRatio`); change it there if you use other resistors. GND of the dividers is the common GND.
Do not connect the pins before the dividers are built: a bare 4.2 V or 5 V can damage the ESP32.

## What the panel and the phone get

- Dashboard tile «باتری»: volts and an approximate percent (`~`). The percent comes from the resting cell voltage, so it is only a hint while charging or while the actuator pulls current.
- Bale and log messages: mains lost / restored, battery low (< 3.6 V for 30 s), battery almost empty (< 3.4 V), back to normal (> 3.75 V). While the router is also dead, the mains-lost message is queued and arrives when the internet returns.
- The existing pin 34 («5 V supply») still watches the boost output; it stays at 5 V until the battery is nearly dead, so it does not replace the battery pin.
