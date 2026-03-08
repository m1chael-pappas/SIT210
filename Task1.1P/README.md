# Task 1.1P – Switching ON Lights: Modular Programming Approach

## System Description

This system simulates a smart lighting solution for Linda, a resident at a retirement village who arrives home after dark. When Linda presses a push button at the entrance, two lights turn on automatically:

- **Porch light** (D2) – stays on for **30 seconds**
- **Hallway light** (D3) – stays on for **60 seconds**

The system is built using an Arduino Nano 33 IoT, two LEDs, a tactile push button, and two 470Ω resistors on a breadboard.

## Hardware Used

- Arduino Nano 33 IoT
- 2x LEDs (from Makerverse LED and Resistor Pack)
- 2x 470Ω resistors
- 1x Tactile push button
- Breadboard and jumper wires

## Wiring Summary

| Component   | Arduino Pin       |
| ----------- | ----------------- |
| Porch LED   | D2                |
| Hallway LED | D3                |
| Push Button | D4 (INPUT_PULLUP) |

Both LEDs are wired in series with a 470Ω current limiting resistor to GND. The button uses the Arduino's internal pull-up resistor -- one leg connects to D4 and the other to GND.

## Code Overview

The code is written using a **modular programming approach**, separating each responsibility into its own function:

- `setup()` – Initialises all pins as inputs or outputs
- `turnOnPorchLight()` – Sets D2 HIGH and records the start time using `millis()`
- `turnOnHallwayLight()` – Sets D3 HIGH and records the start time using `millis()`
- `updatePorchLight()` – Checks if 30 seconds have passed and turns D2 off if so
- `updateHallwayLight()` – Checks if 60 seconds have passed and turns D3 off if so
- `checkButton()` – Reads D4 and triggers both lights when the button is pressed
- `loop()` – Calls checkButton(), updatePorchLight() and updateHallwayLight() continuously

`millis()` is used instead of `delay()` so both lights can run their timers independently without blocking each other.
