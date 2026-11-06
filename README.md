# DIY ESP32-C3 Retro Arcade Handheld Console
**Project Creator:** Subhadip  
**Platform:** ESP32-C3 Super Mini (RISC-V)  
**Display:** 2.4" ST7789 SPI Color TFT (320x240 Landscape)  
**Audio:** Passive Piezo Buzzer (Software PWM Pulse Engine)  
**Form Factor:** Horizontal Nintendo Switch / Game Boy Advance Style  

---

## 1. Project Overview

This project is a standalone, battery-powered 5-in-1 retro handheld console built around the **ESP32-C3 Super Mini** microcontroller and a **2.4-inch SPI ST7789 TFT display**. It features physical analog joystick navigation, dedicated tactile action buttons, an independent pause button, menu navigation, non-blocking chiptune audio feedback, and persistent flash storage for high scores via ESP32 `Preferences` (EEPROM-replacement).

---

## 2. Hardware Architecture & Bill of Materials (BOM)

| Component | Specification / Notes | Qty |
| :--- | :--- | :--- |
| **Microcontroller** | ESP32-C3 Super Mini (Single-core RISC-V @ 160MHz, 4MB Flash) | 1 |
| **Display** | 2.4" SPI TFT LCD (ST7789 controller, 320x240 Resolution, with backlight) | 1 |
| **Input: Joystick** | 2-Axis Analog Thumb Joystick (HW-504 module) | 1 |
| **Input: Buttons** | 6x6x5mm 2-pin or 4-pin Tactile Momentary Push Buttons | 3 |
| **Audio** | 5V Passive Piezo Buzzer (allows multi-frequency tone generation) | 1 |
| **Power Management** | TP4056 Lithium Battery Charger & Protection Module (USB Type-C) | 1 |
| **Battery** | 3.7V Lithium-Ion (18650 cell) or 3.7V LiPo Flat Cell (1000–2000 mAh) | 1 |
| **Power Switch** | SPDT Mini Slide Switch (ON / OFF) | 1 |
| **Prototyping** | Single-sided Dot-matrix Perfboard / Vero board + 30AWG wrapping wires | 1 |

---

## 3. Physical Layout & Ergonomics

The console uses a landscape handheld layout styled after Nintendo portable systems:

```text
+-----------------------------------------------------------------------------------+
|                                                                                   |
|      [LEFT SIDE]                   [CENTER]                     [RIGHT SIDE]      |
|                                                                                   |
|       (JOYSTICK)            +---------------------+                  (BTN B)      |
|           ^                 |                     |               [Exit to Menu]  |
|        <  O  >              |      2.4" TFT       |                               |
|           v                 |   320 x 240 SCREEN  |          (BTN P)              |
|                             |                     |      [Pause / Resume]         |
|     (Navigation &           |                     |                               |
|      Movement)              +---------------------+                  (BTN A)      |
|                                                                   [Action / Jump] |
|                                                                                   |
|  [BUZZER: Mounted Near]                                   [BATTERY: Back Cover]   |
+-----------------------------------------------------------------------------------+
```

### Physical Control Mapping
* **Left Thumb:** Controls the analog joystick for 4-way character steering and vertical menu navigation.
* **Right Thumb:** 
  * **BTN A (Bottom):** Primary action button (Jump in Mario/Flappy Bird, Fire in Space Shooter, Menu Select).
  * **BTN P (Middle):** Dedicated hardware pause toggle.
  * **BTN B (Top):** Global instant return to the Main Arcade Menu.

---

## 4. Complete Electrical Schematic & Pinout

### 4.1. GPIO Allocation Table

| Peripheral | Component Pin | ESP32-C3 Pin | Mode / State | Notes |
| :--- | :--- | :--- | :--- | :--- |
| **TFT Display** | VCC | **5V** or **3.3V** | Power | Connect to 5V (or 3.3V stable rail) |
| | GND | **GND** | Power | Common system ground |
| | SCK (CLK) | **GPIO 4** | Output (SPI Clock) | Hardware SPI bus (`FSPI`) |
| | SDI (MOSI) | **GPIO 6** | Output (SPI MOSI) | Hardware SPI data out |
| | CS | **GPIO 7** | Output | Chip select active low |
| | DC | **GPIO 5** | Output | Data / Command register switch |
| | RESET | **3.3V** | Fixed High | Tied directly to 3.3V power rail |
| | LED (Backlight)| **3.3V** | Power | Limits backlight current safely |
| | SDO (MISO) | *NC* | None | Leave unconnected |
| **Analog Joystick**| VCC | **3.3V** | Power | Connect to 3.3V for accurate ADC |
| | GND | **GND** | Power | Common ground |
| | VRx (Vertical) | **GPIO 0** | ADC1 Channel 0 | Rotated orientation mapping |
| | VRy (Horizontal)| **GPIO 1** | ADC1 Channel 1 | Rotated orientation mapping |
| | SW | *NC* | None | Left unconnected |
| **Push Buttons** | BTN A (Action) | **GPIO 3** | `INPUT_PULLUP` | Second lead connects to GND |
| | BTN B (Exit) | **GPIO 8** | `INPUT_PULLUP` | Second lead connects to GND |
| | BTN P (Pause)| **GPIO 9** | `INPUT_PULLUP` | Second lead connects to GND |
| **Buzzer** | Positive (+) | **GPIO 2** | Digital Output | Software PWM driven |
| | Negative (-) | **GND** | Power | Common ground |

---

### 4.2. Wiring Diagram

```text
                               +-------------------+
                               | 3.7V LIPO BATTERY |
                               +---------+---------+
                                    +    |    -
                                    |    |    |
                     +--------------+    |    +--------------+
                     |                   |                   |
                     v                   v                   v
                 +--------+          +--------+          +--------+
                 |  B +   |          | TP4056 |          |  B -   |
                 | MODULE |          | CHARGE |          | MODULE |
                 +---+----+          +---+----+          +---+----+
                     |                   |                   |
                   OUT+                  |                 OUT-
                     |                   |                   |
               [SPDT SWITCH]             |                   |
                     |                   |                   |
                     v                   |                   v
                ESP32-C3 [5V]            |             ESP32-C3 [GND]
                     |                   |                   |
                     +-------------------+-------------------+
                                         |
               +-------------------------+-------------------------+
               |                         |                         |
               v                         v                         v
       +---------------+         +---------------+         +---------------+
       | 2.4" TFT LCD  |         | HW-504 JOYSTK |         | PASSIVE BUZZ  |
       +---------------+         +---------------+         +---------------+
       | VCC  -> 5V    |         | VCC  -> 3.3V  |         | (+) -> GPIO 2 |
       | GND  -> GND   |         | GND  -> GND   |         | (-) -> GND    |
       | CS   -> GPIO 7|         | VRx  -> GPIO 0|         +---------------+
       | DC   -> GPIO 5|         | VRy  -> GPIO 1|                 |
       | MOSI -> GPIO 6|         | SW   -> NC    |                 v
       | SCK  -> GPIO 4|         +---------------+         +---------------+
       | RST  -> 3.3V  |                                   | PUSH BUTTONS  |
       | LED  -> 3.3V  |                                   +---------------+
       +---------------+                                   | BTN_A-> GPIO 3|
                                                           | BTN_B-> GPIO 8|
                                                           | BTN_P-> GPIO 9|
                                                           | Common-> GND  |
                                                           +---------------+
```

---

## 5. Software Architecture & Critical Fixes

### 5.1. Why `Adafruit_ST7789` Replaced `TFT_eSPI` & `ILI9341`
1. **White Screen & WDT Crash Loops:** The ESP32-C3 runs on a RISC-V core. `TFT_eSPI` often defaults to standard ESP32 pins (GPIO 18, 19, 23) if library configurations fail to link, causing hardware bus locks and triggering system watchdog timer resets (`rst:0x8 (TG1WDT_SYS_RST)`).
2. **Half-Screen Split Static Distortion:** The 2.4" module contains an **ST7789V** controller rather than an ILI9341. When addressed with ILI9341 drivers, internal memory coordinate offsets cause the right half of the screen to show unrefreshed analog snow/static. `Adafruit_ST7789` with explicit 240x320 initialization solves this completely.

### 5.2. Crash-Safe Non-Blocking Software Audio
Standard Arduino `tone()` and raw `ledcAttach` calls on ESP32-C3 boards can collide with timer channels, crashing board peripherals or stalling SPI transfers. This project uses a lightweight manual microsecond pulse driver (`playToneDirect` and software audio queues):
```cpp
void playToneDirect(int freq, int durationMs) {
  if (freq <= 0) { delay(durationMs); return; }
  int period = 1000000 / freq;
  long cycles = ((long)freq * durationMs) / 1000;
  for (long i = 0; i < cycles; i++) {
    digitalWrite(PIN_BUZZER, HIGH);
    delayMicroseconds(period / 2);
    digitalWrite(PIN_BUZZER, LOW);
    delayMicroseconds(period / 2);
  }
}
```

### 5.3. Rotated Joystick Axis Inversion & Deadzone
Because the joystick is physically soldered turned 90 degrees on the perfboard, the raw sensor readings are inverted. A calibrated deadzone between **900** and **3100** eliminates ghost drift:
* **UP:** `analogRead(PIN_JOY_X) > 3100`
* **DOWN:** `analogRead(PIN_JOY_X) < 900`
* **LEFT:** `analogRead(PIN_JOY_Y) > 3100`
* **RIGHT:** `analogRead(PIN_JOY_Y) < 900`

---

## 6. Included Games (5-in-1 Suite)

1. **Flappy Bird Pro Edition:**
   * Full 320x240 widescreen color adaptation.
   * Bonus yellow stars (collectible for +2 points).
   * Progressive difficulty level scaling (Level 1 to 5 with pipe velocity scaling).
   * Persistent high-score saving to ESP32 Flash (`Preferences`).
2. **Space Shooter:**
   * Smooth horizontal ship maneuvering.
   * Rapid laser fire against dropping enemy alien vessels.
   * Real-time score counter and audio hit triggers.
3. **Retro Snake:**
   * Grid-based Nokia style snake mechanics.
   * Screen wrap-around navigation with dynamic apple respawns and snake lengthening.
4. **Brick Breaker (Arkanoid):**
   * Multi-colored reactive brick matrices.
   * Continuous paddle collision physics and angular ball bounce trajectory.
5. **Mario Platform Runner:**
   * Side-scrolling platform obstacle course.
   * Physics-driven gravity jumps over Goomba enemies.

---

## 7. Installation & Flashing Instructions

### Required Libraries
Install the following via the Arduino IDE Library Manager (`Ctrl + Shift + I`):
1. **Adafruit GFX Library**
2. **Adafruit ST7735 and ST7789 Library**

### Arduino IDE Board Settings
* **Board:** `ESP32C3 Dev Module`
* **Flash Mode:** `QIO` or `DIO`
* **Flash Frequency:** `80MHz`
* **CPU Frequency:** `160MHz`
* **Upload Speed:** `921600` or `115200`
* **USB CDC On Boot:** `Enabled`

---

## 8. Troubleshooting Checklist

* **Display Stays Pure White:**
  * Verify display **RESET** pin is tied to **3.3V** (do not leave floating).
  * Verify display **CS** is in **GPIO 7**, **DC** in **GPIO 5**, and **MOSI** in **GPIO 6**.
* **Board Boots Into Bootloader Loop:**
  * Check that **GPIO 8** and **GPIO 9** buttons are not shorted to Ground during boot.
* **Colors Appear Inverted (e.g., Black is White):**
  * Add `tft.invertDisplay(true);` or `tft.invertDisplay(false);` right after `tft.init()` in `setup()`.
