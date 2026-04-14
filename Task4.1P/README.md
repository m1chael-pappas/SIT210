# Task 4.1P - Handling Interrupts

SIT210 Embedded Systems Development - Deakin University

## Description

An interrupt-driven lighting system built with the Arduino Nano 33 IoT. The system simulates a smart porch and hallway lighting setup for the "Linda" scenario:

- Automatically turns on lights when motion is detected and it is dark
- Provides a manual slider switch as a backup to toggle lights
- Automatically turns off lights after 30 seconds of no motion

Both inputs use hardware interrupts via `attachInterrupt()` for responsive, non-polling event handling.

## Hardware

- Arduino Nano 33 IoT
- HC-SR501 PIR Motion Sensor
- BH1750 Light Sensor (I2C)
- SPDT Slider Switch
- 2x LEDs with 220 ohm resistors
- Breadboard and jumper wires

## Pin Configuration

| Component       | Pin       |
| --------------- | --------- |
| PIR Sensor OUT  | D2        |
| Slider Switch   | D3 + GND  |
| LED 1 (Porch)   | D4 (220R) |
| LED 2 (Hallway) | D5 (220R) |
| BH1750 SDA      | A4        |
| BH1750 SCL      | A5        |
| BH1750 VCC      | 3V3       |
| PIR VCC         | 5V        |

## Dependencies

- [BH1750 Library](https://github.com/claws/BH1750) - Install via Arduino Library Manager

## How It Works

- **PIR Interrupt (RISING on D2):** Sets a flag when motion is detected. The main loop checks the BH1750 light level. If below 50 lux, both LEDs turn on. If bright, no action is taken. Lights auto-off after 30 seconds of no motion.
- **Switch Interrupt (CHANGE on D3):** Toggles both LEDs on or off as a manual backup, regardless of light level.

## Setup

1. Wire components as per the circuit diagram in the report
2. Install the BH1750 library via Arduino IDE Library Manager
3. Upload `Task4.1Interrupts.ino` to your Arduino Nano 33 IoT
4. Open Serial Monitor at 9600 baud
5. Wait 30 seconds for PIR sensor warmup
