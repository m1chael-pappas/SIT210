# Task 5.1P - GUI Light Controller

A simple Python Tkinter GUI for the Raspberry Pi that lets the user switch between three LEDs representing the **Living Room**, **Bathroom**, and **Closet** lights in Linda's house. Selecting a radio button turns on the matching LED and turns the others off. An Exit button cleans up the GPIO state and closes the window.

Built for **SIT210 - Embedded Systems Development**, Deakin University.

## Hardware

- Raspberry Pi (any model with GPIO)
- 3x LEDs
- 3x ~330Ω resistors
- Breadboard + jumper wires

### Wiring (BCM numbering)

| Room        | GPIO Pin |
| ----------- | -------- |
| Living Room | 17       |
| Bathroom    | 27       |
| Closet      | 22       |

Each LED: GPIO pin → 330Ω resistor → LED anode → LED cathode → GND.

If you wire to different pins, just edit the `LED_PINS` dictionary at the top of `light_gui.py`.

## Software

- Python 3
- `RPi.GPIO` (pre-installed on Raspberry Pi OS)
- `tkinter` (pre-installed on Raspberry Pi OS)

## How to Run

```bash
python3 light_gui.py
```

If you get a GPIO permission error:

```bash
sudo python3 light_gui.py
```

## How It Works

- **GPIO setup** configures the three pins as outputs and starts them LOW.
- **`update_leds()`** is the callback wired to each radio button. It reads the selected room from a shared `StringVar`, then loops through the `LED_PINS` dictionary turning the matching pin HIGH and all others LOW. This guarantees only one LED is ever on at a time.
- **`exit_app()`** turns all LEDs off, calls `GPIO.cleanup()`, and destroys the window. It is bound to both the Exit button and the window's close (X) button via `WM_DELETE_WINDOW` so the GPIO state is always cleaned up properly.
- The GUI itself is built from the `LED_PINS` dictionary, so adding another room is just one line.

## Files

- `light_gui.py` - the main GUI application
- `README.md` - this file
