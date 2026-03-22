# Task 3.1P - Terrarium Sunlight Monitor

A trigger and notification system that monitors sunlight exposure on a terrarium using an Arduino Nano 33 IoT and a BH1750 light sensor. When sunlight is detected or stops, notifications are sent to Telegram via MQTT and Node-RED.

## System Overview

- **BH1750 light sensor** reads lux values over I2C
- **Arduino Nano 33 IoT** checks readings against a sunlight threshold and detects state changes
- **HiveMQ Cloud** acts as the MQTT broker (TLS on port 8883)
- **Node-RED** subscribes to the MQTT topic and forwards messages to Telegram
- **Telegram Bot API** delivers notifications to the user

## Hardware

- Arduino Nano 33 IoT
- BH1750 light sensor
- Breadboard and jumper wires

## Wiring

| BH1750 Pin | Arduino Pin |
| ---------- | ----------- |
| SDA        | A4          |
| SCL        | A5          |
| VCC        | 3.3V        |
| GND        | GND         |

ADDR pin left unconnected (defaults to I2C address 0x23).

## Libraries Required

Install via Arduino IDE Library Manager:

- **WiFiNINA** - WiFi connectivity for the Nano 33 IoT
- **ArduinoMqttClient** - MQTT client
- **BH1750** by Christopher Laws - light sensor driver
- **Wire** - I2C communication (included with Arduino)

## Configuration

Before uploading, update the following constants in the sketch:

```cpp
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";
const char* mqttBroker = "YOUR_CLUSTER_URL";
const char* mqttUser = "YOUR_HIVEMQ_USERNAME";
const char* mqttPass = "YOUR_HIVEMQ_PASSWORD";
```

The sunlight threshold can be adjusted:

```cpp
const float SUNLIGHT_THRESHOLD = 500.0; // lux
```

## Node-RED Flow

The Node-RED flow consists of three nodes:

1. **mqtt in** - subscribes to `terrarium/light` on the HiveMQ broker
2. **function** - constructs a curl command with the Telegram Bot API URL, chat ID, and message
3. **exec** - executes the curl command to send the Telegram notification

## Usage

1. Wire the BH1750 to the Arduino as shown above
2. Install the required libraries
3. Update the WiFi and MQTT credentials in the sketch
4. Upload the sketch to the Arduino Nano 33 IoT
5. Start Node-RED with the configured flow
6. The system will send a Telegram notification when sunlight is detected and when it stops
