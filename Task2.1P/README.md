# Task 2.1P - Sending Temperature and Soil Moisture Data to the Web

**Unit:** SIT210 Embedded Systems Development
**Platform:** Arduino Nano 33 IoT + ThingSpeak

## Overview

This task demonstrates collecting environmental sensor data from an Arduino Nano 33 IoT and sending it to the web via ThingSpeak every 30 seconds. A DHT22 sensor reads temperature and a soil moisture sensor reads analogue moisture levels, with both values pushed to a ThingSpeak channel using HTTP POST requests.

## Hardware

| Component          | Details                             |
| ------------------ | ----------------------------------- |
| Microcontroller    | Arduino Nano 33 IoT                 |
| Temperature sensor | DHT22 (data pin on D2)              |
| Moisture sensor    | Soil moisture module, SIG pin on A0 |
| Other              | Breadboard, jumper wires            |

## Wiring

| Sensor      | Pin  | Arduino Pin |
| ----------- | ---- | ----------- |
| DHT22       | VCC  | 3.3V        |
| DHT22       | GND  | GND         |
| DHT22       | DATA | D2          |
| Soil sensor | VCC  | 3.3V        |
| Soil sensor | GND  | GND         |
| Soil sensor | SIG  | A0          |

## Software Dependencies

Install the following libraries via Arduino IDE (Tools > Manage Libraries):

- `DHT sensor library` by Adafruit
- `Adafruit Unified Sensor` by Adafruit
- `ThingSpeak` by MathWorks
- `WiFiNINA` by Arduino

## Configuration

Before uploading, update these values in `Task2_1WebHook.ino`:

```cpp
const char* WIFI_SSID     = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
const unsigned long CHANNEL_ID = 0000000UL;
const char* WRITE_API_KEY = "YOUR_WRITE_API_KEY";
```

## ThingSpeak Channel

| Field   | Data                       |
| ------- | -------------------------- |
| Field 1 | Temperature (C)            |
| Field 2 | Soil Moisture (raw 0-1023) |

## How It Works

The Arduino connects to WiFi on startup and begins reading both sensors every 30 seconds using a `millis()`-based non-blocking timer. Readings are sent to ThingSpeak via an HTTP POST request through the `sendToThingSpeak()` function, which acts as the webhook client in this solution. Output is also printed to the Serial Monitor at 9600 baud for debugging.
