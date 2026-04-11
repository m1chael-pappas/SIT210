#!/usr/bin/env python3
"""Task 5.2C - GUI Light Controller with PWM dimming (gpiozero version)."""

import tkinter as tk
from tkinter import ttk
from gpiozero import PWMLED

# --- LED setup (BCM pin numbers) ---
leds = {
    "Living Room": PWMLED(17),
    "Bathroom":    PWMLED(27),
    "Closet":      PWMLED(22),
}


def update_leds():
    selected = room_var.get()
    for room, led in leds.items():
        if room == selected:
            led.value = brightness.get() / 100 if room == "Living Room" else 1.0
        else:
            led.value = 0
    status_label.config(text=f"ON: {selected}")


def on_slider(_value):
    if room_var.get() == "Living Room":
        leds["Living Room"].value = brightness.get() / 100


def exit_app():
    for led in leds.values():
        led.off()
        led.close()
    root.destroy()


# --- GUI ---
root = tk.Tk()
root.title("Linda's Light Controller")
root.geometry("340x360")
root.protocol("WM_DELETE_WINDOW", exit_app)

tk.Label(root, text="Room Lights", font=("Helvetica", 16, "bold")).pack(pady=10)

room_var = tk.StringVar(value="")
for room in leds:
    ttk.Radiobutton(
        root, text=room, variable=room_var, value=room, command=update_leds
    ).pack(anchor="w", padx=40, pady=4)

tk.Label(root, text="Living Room Brightness", font=("Helvetica", 11)).pack(pady=(15, 0))
brightness = tk.IntVar(value=100)
tk.Scale(
    root, from_=0, to=100, orient="horizontal",
    variable=brightness, command=on_slider, length=240,
).pack()

status_label = tk.Label(root, text="All lights OFF", font=("Helvetica", 11))
status_label.pack(pady=8)

tk.Button(root, text="Exit", width=12, command=exit_app).pack(pady=8)

root.mainloop()