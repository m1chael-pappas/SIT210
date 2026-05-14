# Task 8.1HD - Voice Activated Lighting System

**Unit:** SIT210 Embedded Systems Development
**Student:** Michael Pappas

A voice activated lighting system that lets Linda switch the bathroom and hallway lights on or off and trigger the exhaust fan using spoken commands. A USB microphone on the Raspberry Pi picks up the voice command, the Pi converts it to text and parses it into a short instruction, and that instruction is sent over Bluetooth Low Energy to an Arduino Nano 33 IoT. The Arduino reads a BH1750 ambient light sensor before turning on a light, so the LEDs only switch on when the room is actually dark.

## Hardware

| Device | Role |
| --- | --- |
| Raspberry Pi 5 | Captures audio, runs speech-to-text, acts as the BLE central |
| USB microphone | Audio input on the Pi |
| Arduino Nano 33 IoT | BLE peripheral, drives the LEDs, reads the light sensor, controls the servo |
| BH1750 light sensor | Reads ambient lux (I2C on A4 / A5) |
| LED on D3 | Bathroom light |
| LED on D4 | Hallway light |
| 9g micro servo on D9 | Stand-in for the exhaust fan |

The servo is used in place of a real exhaust fan because I do not have a transistor or relay to drive a real fan from a GPIO pin. The servo demonstrates the same control logic (actuator on, actuator off) and gives a clear visual indication that the "fan" has been activated.

## Wiring

| Pin (Nano 33 IoT) | Component |
| --- | --- |
| 3V3 | BH1750 VCC |
| GND | BH1750 GND, LED cathodes, servo brown wire |
| A4 (SDA) | BH1750 SDA |
| A5 (SCL) | BH1750 SCL |
| D3 | Bathroom LED anode (through 220 ohm) |
| D4 | Hallway LED anode (through 220 ohm) |
| D9 | Servo signal (orange) |
| 5V (VUSB) | Servo power (red) |

## Data Flow

```
+--------------+      audio       +--------------------+
|   USB Mic    | ---------------> |  Raspberry Pi 5    |
+--------------+                  |                    |
                                  |  speech_recognition|
                                  |  parse_command()   |
                                  +---------+----------+
                                            | BLE write
                                            v
                                  +--------------------+
                                  |  Arduino Nano 33   |
                                  |       IoT          |
                                  |                    |
                                  |  ArduinoBLE        |
                                  |  handleCommand()   |
                                  +---------+----------+
                                            |
                       +--------------------+---------------------+
                       |                    |                     |
                       v                    v                     v
                +-------------+      +-------------+        +-----------+
                |   BH1750    |      | LEDs (D3,4) |        |  Servo D9 |
                |  ambient    |      | bathroom +  |        |   (fan)   |
                |  light gate |      |  hallway    |        |           |
                +-------------+      +-------------+        +-----------+
```

## Voice Commands

The recogniser does not need exact wording. It picks out the keywords below from whatever phrase it transcribes.

| Spoken phrase (examples) | Resulting command |
| --- | --- |
| "turn on the bathroom light" | `bathroom_on` |
| "turn off the bathroom light" | `bathroom_off` |
| "switch on the hallway light" | `hallway_on` |
| "hallway off" | `hallway_off` |
| "turn on the fan" / "exhaust on" | `fan_on` |
| "turn on everything" / "all lights on" | `all_on` |
| "wake up" | `all_on` |
| "good night" | `all_off` |

If the BH1750 reads more than 50 lux when an "on" command arrives, the Arduino skips the LED because the room is already bright enough. This threshold lives in `DARK_THRESHOLD_LUX` at the top of the sketch and is easy to tune.

## Software Setup

### Arduino side

Install the following libraries through the Arduino IDE Library Manager:

- `ArduinoBLE` (BLE peripheral support for the Nano 33 IoT)
- `BH1750` by Christopher Laws
- `Servo` (ships with the IDE)

Open `arduino/voice_lights.ino`, select the Arduino Nano 33 IoT board, and upload.

### Raspberry Pi side

```bash
sudo apt update
sudo apt install portaudio19-dev python3-pyaudio flac
pip install -r pi/requirements.txt
```

Then run:

```bash
python3 pi/voice_lights.py
```

The Pi will scan for a BLE device advertising as `VoiceLights`, connect, and start listening on the default microphone.

## Files

- `arduino/voice_lights.ino` - BLE peripheral sketch, light sensor + LED + servo control
- `pi/voice_lights.py` - speech recogniser and BLE central
- `pi/requirements.txt` - Python dependencies
- `README.md` - this file

## Notes

- The speech recognition uses Google's free Web Speech API, which needs the Pi to be online. For a fully offline version Vosk can be dropped in by replacing `recognize_google()` with `recognize_vosk()`.
- The BLE central does not pair with the Arduino, it just connects and writes to a characteristic. No bonding or PIN setup is needed.
- If the Pi cannot find the Arduino, check that the BLE radio is enabled (`sudo systemctl status bluetooth`) and that no other process is holding the adapter.