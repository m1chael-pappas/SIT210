# Task 5.2C - GUI Light Controller with PWM Dimming

Extends Task 5.1P by adding a brightness slider for the **Living Room** LED. The slider drives a PWM signal on the GPIO pin so Linda can dim or brighten her living room light from the GUI. The Bathroom and Closet LEDs still behave as simple on/off lights.

Built for **SIT210 - Embedded Systems Development**, Deakin University.

## Hardware

Same wiring as Task 5.1P — no changes needed because GPIO 17, 27, and 22 all support software PWM via `RPi.GPIO`.

| Room        | GPIO Pin | Mode   |
| ----------- | -------- | ------ |
| Living Room | 17       | PWM    |
| Bathroom    | 27       | On/Off |
| Closet      | 22       | On/Off |

Each LED: GPIO pin → 330Ω resistor → LED anode → cathode → GND.

## How to Run

```bash
python3 light_gui_pwm.py
```

## How It Works

- **PWM setup** — every pin is wrapped in a `GPIO.PWM` object running at 1 kHz, stored in a `pwms` dictionary keyed by pin number. Starting them all at 0% duty cycle means every LED begins off.
- **`update_leds()`** — fired by the radio buttons. Loops through the pins and sets the selected room to its brightness (slider value for Living Room, 100% for the others) and everything else to 0%.
- **`on_slider()`** — fired live as the slider moves. It only updates the Living Room duty cycle if Living Room is the currently selected room, so dragging the slider while Bathroom is on doesn't do anything weird.
- **`exit_app()`** — stops every PWM channel and calls `GPIO.cleanup()` before destroying the window. Bound to both the Exit button and the window's close (X) button.

PWM works by switching the pin on and off very quickly (1000 times per second here). The percentage of time it's on (the duty cycle) controls the average power going to the LED, which the eye perceives as brightness.

## Files

- `light_gui_pwm.py` - the main application
- `README.md` - this file
