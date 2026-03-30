# Task 4.2D - Calling a Function from the Web

## Overview

A remote light control system for an assisted living facility. Carers can toggle three lights (living room, bathroom, closet) in Linda's home from a web dashboard using MQTT messaging.

## Hardware

- Arduino Nano 33 IoT
- 3x LEDs connected to pins D3 (living room), D4 (bathroom), D5 (closet)
- 3x 220 ohm resistors
- Breadboard and jumper wires

## Software

- Arduino IDE with `WiFiNINA` and `PubSubClient` libraries
- Node-RED with `@flowfuse/node-red-dashboard`
- HiveMQ Cloud (free MQTT broker)

## How It Works

1. Node-RED serves a web dashboard with three toggle buttons at `http://localhost:1880/dashboard`
2. Clicking a button publishes a string message (e.g. "living room") to the MQTT topic `linda/lights` on HiveMQ Cloud
3. The Arduino subscribes to that topic and toggles the corresponding LED via the `mqttCallback()` function

## Setup

### Arduino

1. Open `LindasLights.ino` in Arduino IDE
2. Install `WiFiNINA` and `PubSubClient` (by Nick O'Leary) from Library Manager
3. Fill in your WiFi and HiveMQ credentials
4. Upload to your Nano 33 IoT

### Node-RED

1. Run `npx node-red` in your terminal
2. Open `http://localhost:1880`
3. Import `node-red-flow.json` (Menu > Import > select file)
4. Update the MQTT broker node with your HiveMQ credentials
5. Hit Deploy
6. Open `http://localhost:1880/dashboard` to control the lights
