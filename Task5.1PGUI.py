#!/usr/bin/env python3
"""
Task 5.1P - GUI Light Controller
Tkinter GUI to control 3 LEDs (living room, bathroom, closet) on a Raspberry Pi.
"""

import tkinter as tk
from tkinter import ttk
import RPi.GPIO as GPIO

# --- GPIO pin config (BCM numbering) ---
LED_PINS = {
    "Living Room": 17,
    "Bathroom":    27,
    "Closet":      22,
}

# --- GPIO setup ---
GPIO.setmode(GPIO.BCM)
GPIO.setwarnings(False)
for pin in LED_PINS.values():
    GPIO.setup(pin, GPIO.OUT)
    GPIO.output(pin, GPIO.LOW)


def update_leds():
    """Turn on the selected room's LED, turn all others off."""
    selected = room_var.get()
    for room, pin in LED_PINS.items():
        GPIO.output(pin, GPIO.HIGH if room == selected else GPIO.LOW)
    status_label.config(text=f"ON: {selected}")


def exit_app():
    """Clean up GPIO and close the window."""
    for pin in LED_PINS.values():
        GPIO.output(pin, GPIO.LOW)
    GPIO.cleanup()
    root.destroy()


# --- Build the GUI ---
root = tk.Tk()
root.title("Linda's Light Controller")
root.geometry("320x260")
root.protocol("WM_DELETE_WINDOW", exit_app)  # handle window close button

title = tk.Label(root, text="Room Lights", font=("Helvetica", 16, "bold"))
title.pack(pady=10)

room_var = tk.StringVar(value="")  # nothing selected at startup

for room in LED_PINS:
    rb = ttk.Radiobutton(
        root,
        text=room,
        variable=room_var,
        value=room,
        command=update_leds,
    )
    rb.pack(anchor="w", padx=40, pady=4)

status_label = tk.Label(root, text="All lights OFF", font=("Helvetica", 11))
status_label.pack(pady=10)

exit_btn = tk.Button(root, text="Exit", width=12, command=exit_app)
exit_btn.pack(pady=10)

root.mainloop()