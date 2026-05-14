


#!/usr/bin/env python3
"""
Task 8.1HD - Voice Activated Lighting System (Raspberry Pi side)
SIT210 Embedded Systems Development
"""

import os
import sys
import asyncio
from ctypes import CFUNCTYPE, c_char_p, c_int, cdll

# Silence ALSA error messages (the noisy underrun/JACK probing chatter
# that PyAudio prints on startup). Has no effect on actual audio.
ERROR_HANDLER_FUNC = CFUNCTYPE(None, c_char_p, c_int, c_char_p, c_int, c_char_p)

def _py_error_handler(filename, line, function, err, fmt):
    pass

try:
    asound = cdll.LoadLibrary('libasound.so.2')
    c_error_handler = ERROR_HANDLER_FUNC(_py_error_handler)
    asound.snd_lib_error_set_handler(c_error_handler)
except OSError:
    pass

os.environ['JACK_NO_AUDIO_RESERVATION'] = '1'

import speech_recognition as sr
from bleak import BleakClient, BleakScanner

# Must match the UUIDs in the Arduino sketch
DEVICE_NAME      = "VoiceLights"
COMMAND_UUID     = "19B10001-E8F2-537E-4F6C-D104768A1214"
STATUS_UUID      = "19B10002-E8F2-537E-4F6C-D104768A1214"

# Keyword table for parsing recognised text into commands.
# Order matters: more specific phrases come first.
COMMAND_MAP = [
    ("bathroom",   "off",  "bathroom_off"),
    ("bathroom",   "on",   "bathroom_on"),
    ("hallway",    "off",  "hallway_off"),
    ("hallway",    "on",   "hallway_on"),
    ("fan",        "off",  "fan_off"),
    ("fan",        "on",   "fan_on"),
    ("exhaust",    "off",  "fan_off"),
    ("exhaust",    "on",   "fan_on"),
    ("all lights", "off",  "all_off"),
    ("all lights", "on",   "all_on"),
    ("everything", "off",  "all_off"),
    ("everything", "on",   "all_on"),
    ("good night", "",     "all_off"),
    ("wake up",    "",     "all_on"),
]


def parse_command(text):
    """Map a recognised phrase to one of the Arduino commands."""
    text = text.lower()
    for keyword, action, command in COMMAND_MAP:
        if keyword in text and (action == "" or action in text):
            return command
    return None


def listen_once(recogniser, mic):
    """Capture one phrase from the mic and return the transcribed text."""
    with mic as source:
        print("Listening...")
        recogniser.adjust_for_ambient_noise(source, duration=0.5)
        try:
            audio = recogniser.listen(source, timeout=5, phrase_time_limit=4)
        except sr.WaitTimeoutError:
            return None

    try:
        text = recogniser.recognize_google(audio)
        print(f"Heard: {text}")
        return text
    except sr.UnknownValueError:
        print("Could not understand audio.")
    except sr.RequestError as e:
        print(f"Speech API error: {e}")
    return None


async def find_device():
    """Scan for the Arduino by its advertised name."""
    print(f"Scanning for '{DEVICE_NAME}'...")
    device = await BleakScanner.find_device_by_name(DEVICE_NAME, timeout=10.0)
    if device is None:
        print("Could not find the Arduino. Is it powered on and advertising?")
        sys.exit(1)
    print(f"Found {device.name} at {device.address}")
    return device


def on_status(_handle, data):
    """Handle status notifications coming back from the Arduino."""
    print(f"  Arduino status -> {data.decode().strip()}")


async def main():
    device = await find_device()

    async with BleakClient(device) as client:
        print("Connected over BLE.")
        await client.start_notify(STATUS_UUID, on_status)

        recogniser = sr.Recognizer()
        mic = sr.Microphone()

        print("\nReady. Say something like:")
        print("  'turn on the bathroom light'")
        print("  'turn off the hallway light'")
        print("  'turn on everything'")
        print("  'good night'")
        print("Press Ctrl+C to quit.\n")

        try:
            while True:
                text = listen_once(recogniser, mic)
                if not text:
                    continue

                command = parse_command(text)
                if command is None:
                    print("No matching command, ignoring.")
                    continue

                print(f"Sending: {command}")
                await client.write_gatt_char(
                    COMMAND_UUID, command.encode(), response=True
                )
                await asyncio.sleep(0.5)
        except KeyboardInterrupt:
            print("\nStopping.")
        finally:
            await client.stop_notify(STATUS_UUID)


if __name__ == "__main__":
    asyncio.run(main())