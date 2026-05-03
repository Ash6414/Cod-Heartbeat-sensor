**Heartbeat Sensor**
## Teensy 4.0 RD-03D G550 Radar Display

A compact radar visualization project using a **Teensy 4.0**, an **RD-03D G550 mmWave radar module**, and a **Hiletgo 2.8 inch ILI9341 SPI TFT display**. The system reads multi target radar data over UART and displays detected targets on a 2D radar style screen.

## Project Status

Current build:

- Teensy 4.0 controls the display
- RD-03D G550 sends radar data over UART
- ILI9341 TFT shows a radar fan view
- Up to 3 targets can be parsed and displayed
- Button test mode has been removed because the radar is now attached
- Radar data is read through `Serial1`

## Hardware

### Main Components

| Part | Purpose |
|---|---|
| Teensy 4.0 | Main microcontroller |
| RD-03D G550 | mmWave radar sensor |
| Hiletgo 2.8 inch ILI9341 SPI TFT, 240x320 | Radar display |
| Jumper wires | Wiring |
| USB cable | Programming and serial monitor |

## Wiring

### TFT Display to Teensy 4.0

| TFT Pin | Teensy 4.0 Pin |
|---|---:|
| VCC | 3.3 V |
| GND | GND |
| CS | 10 |
| RESET / RST | 8 |
| DC / RS | 9 |
| SDI / MOSI | 11 |
| SCK | 13 |
| SDO / MISO | 12, optional |
| LED | 3.3 V |

### RD-03D G550 to Teensy 4.0

| RD-03D G550 Pin | Teensy 4.0 Pin |
|---|---:|
| 5V | VIN / 5 V |
| GND | GND |
| TX | Pin 0, RX1 |
| RX | Pin 1, TX1 |

Important:

- The RD-03D G550 should be powered from **5 V**, not the Teensy 3.3 V pin.
- The radar and Teensy must share a common ground.
- The radar TX pin goes to Teensy RX1.
- The radar RX pin goes to Teensy TX1.

## Software Requirements

### Arduino IDE

Install Teensy board support using the PJRC Boards Manager URL:

```text
https://www.pjrc.com/teensy/package_teensy_index.json
```

Then select:

```text
Tools → Board → Teensyduino → Teensy 4.0
```

### Required Libraries

The sketch uses:

```cpp
#include <Arduino.h>
#include <SPI.h>
#include <ILI9341_t3.h>
```

Use the Teensyduino provided version of `ILI9341_t3`.

If compilation fails with errors involving `KINETISK_SPI0`, Arduino is probably using an old incompatible copy of `ILI9341_t3`. Delete or rename the copy inside:

```text
Documents/Arduino/libraries/ILI9341_t3
```

and let Arduino use the Teensyduino bundled version instead.

## Serial Settings

The USB Serial Monitor uses:

```text
115200 baud
```

The radar UART uses:

```text
256000 baud
```

The radar is connected to:

```cpp
#define RADAR_SERIAL Serial1
#define RADAR_BAUD   256000
```

On Teensy 4.0:

| UART | Pin |
|---|---:|
| RX1 | 0 |
| TX1 | 1 |

## Display Behavior

The TFT shows a radar fan display with:

- Center forward direction at `0°`
- Left side around `-60°`
- Right side around `+60°`
- Range arcs
- Multiple target markers
- Compact target readout

Each detected target includes:

- Target slot number
- Angle in degrees
- Distance in meters
- Speed value from radar data

## Radar Frame Handling

The RD-03D G550 sends binary UART frames. The sketch expects a 30 byte frame:

```text
Header:  AA FF 03 00
Payload: 24 bytes
Footer:  55 CC
```

Some modules or firmware revisions may use:

```text
AD FF 03 00
```

instead of:

```text
AA FF 03 00
```

The sketch accepts both.

Each frame contains up to 3 target slots. Each target slot is 8 bytes:

```text
X_L X_H Y_L Y_H SPEED_L SPEED_H RES_L RES_H
```

The code decodes each valid target and displays it on the screen.

## Coordinate Convention

The display uses this convention:

| Radar Direction | Display Meaning |
|---|---|
| `0°` | Straight ahead |
| Negative angle | Left |
| Positive angle | Right |

If left and right appear reversed, change:

```cpp
const bool RADAR_FLIP_LEFT_RIGHT = false;
```

to:

```cpp
const bool RADAR_FLIP_LEFT_RIGHT = true;
```

## Main Files

Typical sketch structure:

```text
RadarDisplay.ino
```

Main code sections:

| Section | Purpose |
|---|---|
| TFT pin map | Defines screen wiring |
| Radar UART settings | Defines RD-03D serial port |
| Display geometry | Defines radar screen layout |
| Target structure | Stores decoded target data |
| Display functions | Draws radar fan, targets, and readout |
| RD-03D decoding | Parses binary radar frames |
| Main loop | Reads radar data and updates display |

## Upload Instructions

1. Connect the Teensy 4.0 by USB.
2. Open the sketch in Arduino IDE.
3. Select:

```text
Board: Teensy 4.0
USB Type: Serial
CPU Speed: 600 MHz
Port: Teensy port
```

4. Click upload.
5. Open Serial Monitor at:

```text
115200 baud
```

6. Move in front of the radar and watch for target data.

## Expected Serial Output

When targets are detected, the Serial Monitor should show lines similar to:

```text
System ready.
RD-03D G550 radar on Serial1.
Teensy pin 0 RX1 <- radar TX
Teensy pin 1 TX1 -> radar RX
Radar baud: 256000

[DISPLAY UPDATE] source=RADAR targets=2
  T1 theta=-14.2 deg dist=1.76 m speed=23 cm/s
  T2 theta=18.9 deg dist=2.41 m speed=-12 cm/s
```

## Troubleshooting

### Screen is white

A white screen usually means the backlight is powered but the display controller is not receiving valid SPI commands.

Check:

- TFT `VCC` is connected to 3.3 V
- TFT `GND` is connected to Teensy GND
- `CS`, `DC`, and `RST` match the sketch
- `MOSI` is on Teensy pin 11
- `SCK` is on Teensy pin 13
- The correct `ILI9341_t3` library is being used

### Radar gives no targets

Check:

- RD-03D has 5 V power
- RD-03D GND is connected to Teensy GND
- Radar TX goes to Teensy RX1 pin 0
- Radar RX goes to Teensy TX1 pin 1
- Baud rate is set to 256000
- You are moving in front of the radar

The RD-03D is motion based, so stationary objects may not appear reliably.

### Targets appear mirrored

Change:

```cpp
const bool RADAR_FLIP_LEFT_RIGHT = false;
```

to:

```cpp
const bool RADAR_FLIP_LEFT_RIGHT = true;
```

### Display flickers

The screen is redrawn periodically. To reduce flicker, increase:

```cpp
const unsigned long RADAR_DISPLAY_INTERVAL_MS = 100;
```

For example:

```cpp
const unsigned long RADAR_DISPLAY_INTERVAL_MS = 150;
```

### Targets stay on screen after leaving

The sketch clears stale radar targets after a timeout. Adjust:

```cpp
const unsigned long RADAR_TARGET_STALE_MS = 800;
```

A smaller number clears faster. A larger number holds targets longer.

## Known Limits

- The display currently shows a simplified 2D radar fan.
- The system does not yet track persistent target identity across frames.
- Target colors correspond to decoded target slots, not guaranteed real person identity.
- The RD-03D is better at detecting moving targets than stationary objects.
- The current screen update method redraws the full radar view each refresh.

## Future Improvements

Possible next upgrades:

- Smooth target motion between frames
- Draw fading trails
- Add velocity arrows
- Add persistent target IDs
- Add a configurable maximum range
- Add a startup diagnostics screen
- Add raw frame debug mode
- Add data logging over USB serial
- Add a cleaner enclosure or wearable display mount
