# Task 3.2C - MQTT Publish/Subscribe Gesture Control

An MQTT-based system that uses an ultrasonic sensor to detect hand gestures (wave and pat) and controls LEDs via publish/subscribe messaging through the EMQX public broker.

## System Overview

- **HC-SR04 ultrasonic sensor** detects hand distance to distinguish between wave and pat gestures
- **Arduino Nano 33 IoT** reads sensor data, detects gestures, and communicates via MQTT
- **EMQX public broker** (`broker.emqx.io:1883`) routes messages between publisher and subscriber
- **Two LEDs** represent hallway and bathroom lights, toggled by incoming MQTT messages

## How It Works

The Arduino acts as both an MQTT publisher and subscriber. When a gesture is detected:

- **Wave** (hand in range for less than 1.5 seconds): publishes to `ES/Wave`, which triggers LEDs ON
- **Pat** (hand in range for more than 1.5 seconds): publishes to `ES/Pat`, which triggers LEDs OFF

The broker sends the message back to the Arduino (since it subscribes to the same topics), and the callback function toggles the LEDs accordingly.

## Hardware

- Arduino Nano 33 IoT (VUSB jumper soldered for 5V output)
- HC-SR04 ultrasonic sensor
- 2x LEDs
- 2x 220 ohm resistors (for LEDs)
- 1x 1k ohm resistor (voltage divider)
- 1x 2k ohm resistor (voltage divider)
- Breadboard and jumper wires

## Wiring

| Component    | Pin         | Arduino Pin | Notes                             |
| ------------ | ----------- | ----------- | --------------------------------- |
| HC-SR04 VCC  | VCC         | VUSB (5V)   | Requires soldered VUSB jumper     |
| HC-SR04 GND  | GND         | GND         |                                   |
| HC-SR04 Trig | Trig        | D2          | Direct connection                 |
| HC-SR04 Echo | Echo        | D3          | Through voltage divider (1k + 2k) |
| Hallway LED  | Anode (+)   | D4          | Through 220 ohm resistor          |
| Bathroom LED | Anode (+)   | D5          | Through 220 ohm resistor          |
| Both LEDs    | Cathode (-) | GND         |                                   |

The voltage divider on the Echo pin is required because the HC-SR04 outputs 5V but the Nano 33 IoT GPIO pins are 3.3V. The 1k and 2k resistor divider drops the signal to approximately 3.3V.

## Libraries Required

Install via Arduino IDE Library Manager:

- **WiFiNINA** - WiFi connectivity for the Nano 33 IoT
- **ArduinoMqttClient** - MQTT client

## Configuration

Before uploading, update the WiFi credentials in the sketch:

```cpp
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";
```

The EMQX public broker requires no authentication.

## MQTT Topics

| Topic     | Trigger                        | Action        |
| --------- | ------------------------------ | ------------- |
| `ES/Wave` | Quick hand swipe (< 1.5s)      | LEDs turn ON  |
| `ES/Pat`  | Hand held near sensor (> 1.5s) | LEDs turn OFF |

## Gesture Detection

The system uses non-blocking state tracking to detect gestures based on how long an object remains within the detection range (12cm):

- Object enters range: start timer
- Object leaves range: calculate hold time
- Hold time < 1.5 seconds: wave
- Hold time > 1.5 seconds: pat
- Very short detections (< 50ms) are ignored as noise
