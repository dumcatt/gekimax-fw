# Gekimax IO4 & Multi-Mode Custom Firmware

High-performance custom firmware for the **Gekimax** rhythm game controller built on the **Raspberry Pi Pico (RP2040)**.

## Features

- **WebSerial Configurator Tool (`configurator.html`)**:
  - Single-file web application to visually rebind all 21 keys, customize per-button & underglow RGB colors, tune lever smoothing & inversion, and test inputs live.
  - Entered by holding **GP1 (Side Left)** on bootup.
- **Sega IO4 Emulation Mode** (for Sega ONGEKI arcade game support):
  - Pure IO4 emulation matching `lkick-io4` with correct Sega string descriptors.
  - Inverted side buttons for arcade optical sensor compatibility.
  - Game-controlled RGB lighting for the main 6 buttons (ONGEKI `SET_GENERAL_OUTPUT`).
  - Test (`Fn + GP0`) and Service (`Fn + GP10`) switches.
- **Full N-Key Rollover (NKRO) Keyboard Mode**:
  - 232-key bitmap report supporting all 21 buttons held down simultaneously with 0 rollover limit and 0 dropped keys at 1000Hz.
  - Customizable keybindings stored in Flash memory.
- **XInput Gamepad Mode**:
  - Native Xbox 360 controller emulation for PC games.
  - Inverted & smoothed analog lever mapped to **Left Stick X-Axis**.
- **Guided Potentiometer Calibration Mode**:
  - Interactive LED-guided calibration on boot to adapt to the potentiometer's exact physical limits.
- **Non-Volatile Flash Persistence**:
  - Saves your active mode, keybinds, LED colors, and lever calibration directly to RP2040 Flash memory.
- **27-LED Reactive & Game Lighting Engine**:
  - Driven by high-speed RP2040 PIO (800kHz WS2812B/SK6812) on Core 1 for zero input latency on Core 0.

---

## Startup Button Combinations

Hold down one of these buttons when plugging in the USB cable:

| Button Pin | Action / Mode | Flash Stored? |
|---|---|:---:|
| **GP1** (Side Left) | Enter **WebSerial Configurator Mode** | - |
| **GP5** (Left Bottom 1) | Switch to **Sega IO4 Mode** (ONGEKI) | Yes |
| **GP6** (Left Bottom 2) | Switch to **Keyboard NKRO Mode** | Yes |
| **GP7** (Left Bottom 3) | Switch to **XInput Gamepad Mode** | Yes |
| **GP8** (Left Bottom 4) | Enter **BOOTSEL Mode** (flashing drive appears) | - |
| **GP20** (Fn / Mid Space) | Enter **Guided Potentiometer Calibration Mode** | Yes |

*Note: If no button is held at boot, the controller automatically starts in the last saved mode.*

---

## WebSerial Configurator Tool

1. Unplug the controller.
2. Hold down **GP1 (Side Left)** while plugging the controller into USB (all LEDs will pulse in Cyan/Blue).
3. Open [`configurator.html`](file:///c:/Users/papha/OneDrive/Documents/gekimax-io4/configurator.html) in Google Chrome or Microsoft Edge.
4. Click **"Connect Controller"** and select the Gekimax serial port.
5. Customize your keybinds, reactive LED colors, and lever settings visually, then click **"Save to Controller Flash"**!

---

## Pinout & Default Mapping Reference

| GPIO Pin | Gekimax Switch | IO4 Mode (ONGEKI) | Keyboard Mode (NKRO) | XInput Mode |
|---|---|---|---|---|
| **GP0** | Left Menu (SW1) | Left Menu (`switches[1]:14`) | `Escape` | `Back` (Select) |
| **GP1** | Left Side (SW2) | Left Wall (Inverted) | `Left Shift` | `LB` (Left Bumper) |
| **GP2** | Left Red (SW3) | Left Red (`switches[0]:0`) | `A` | `D-Pad Left` |
| **GP3** | Left Green (SW4) | Left Green (`switches[0]:5`) | `S` | `D-Pad Up` |
| **GP4** | Left Blue (SW5) | Left Blue (`switches[0]:4`) | `D` | `D-Pad Right` |
| **GP5** | Left 1 (SW6) | *(Boot: IO4 Mode)* | `Z` *(Boot: IO4 Mode)* | `D-Pad Down` *(Boot: IO4)* |
| **GP6** | Left 2 (SW7) | *(Boot: KB Mode)* | `X` *(Boot: KB Mode)* | `LS` (Thumb L) *(Boot: KB)* |
| **GP7** | Left 3 (SW8) | *(Boot: XInput)* | `V` *(Boot: XInput)* | `RS` (Thumb R) *(Boot: XInput)*|
| **GP8** | Left 4 (SW9) | *(Boot: BOOTSEL)* | `B` *(Boot: BOOTSEL)* | `B` *(Boot: BOOTSEL)* |
| **GP9** | Left Space (SW10) | - | `C` | `LT` (Left Trigger) |
| **GP10**| Right Menu (SW11)| Right Menu (`switches[0]:13`)| `Enter` | `Start` |
| **GP11**| Right Side (SW12)| Right Wall (Inverted) | `Right Shift` | `RB` (Right Bumper) |
| **GP12**| Right Red (SW13) | Right Red (`switches[0]:1`)| `L` | `X` |
| **GP13**| Right Green (SW14)| Right Green (`switches[1]:0`)| `;` (Semicolon) | `Y` |
| **GP14**| Right Blue (SW15) | Right Blue (`switches[0]:15`)| `'` (Single Quote) | `A` |
| **GP15**| Right 1 (SW16) | - | `N` | `B` |
| **GP16**| Right 2 (SW17) | - | `,` (Comma) | Right Stick Up |
| **GP17**| Right 3 (SW18) | - | `.` (Period) | Right Stick Left |
| **GP18**| Right 4 (SW19) | - | `/` (Slash) | Right Stick Right |
| **GP19**| Right Space (SW20)| - | `M` | `RT` (Right Trigger) |
| **GP20**| Mid Space (SW21) | **Fn**<br>• Fn + GP0 = **Test**<br>• Fn + GP10 = **Service** | `Space` | `Guide` (Xbox Home) |
| **GP21**| 27x WS2812 LEDs | Game-Controlled RGB | Reactive LED Lighting | Reactive LED Lighting |
| **GP26**| Potentiometer Lever | ONGEKI Lever (`analog[0]`) | - | **Left Stick X-Axis** (Inverted + Smoothed) |

---

## Flashing the Firmware

1. Hold **GP8** on the Gekimax while plugging it into your computer (or hold the Pico's white BOOTSEL button).
2. A USB drive named `RPI-RP2` will appear.
3. Drag and drop [`build/gekimax.uf2`](file:///c:/Users/papha/OneDrive/Documents/gekimax-io4/build/gekimax.uf2) onto the `RPI-RP2` drive.
4. The Pico will flash and reboot automatically!
