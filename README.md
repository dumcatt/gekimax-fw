# Gekimax IO4 & Multi-Mode Custom Firmware (v7)

High-performance custom firmware for the [Gekimax](https://github.com/Umi4Life/gekimax) rhythm game controller built on the **Raspberry Pi Pico (RP2040)**.

> [!CAUTION]
> This project was 100% vibe coded. I don't know how much of it is actually correct, but it works for my use cases.
## Features

- **WebSerial Hardware Studio (`configurator.html`)**:
  - Single-file web application with proportional CAD layout matching `gekimax.kicad_pcb` (south-facing switch LEDs `D1–D21`, bottom-mounted underglow `D22–D27`, and accurate potentiometer orientation).
  - Visually rebind all 21 keys across **Base Layer (0)** and **FN Layer (1)** in both **Keyboard Mode** (232-key NKRO) and **Gamepad Mode** (XInput 360 buttons, triggers, D-Pad, and thumbsticks).
  - Customize per-button & underglow RGB colors, lighting effects (`Reactive`, `Breathing`, `Inverted Reactive`, `Rainbow Wave`, `Solid`, `Off`), master LED brightness (`0–255`), switch debounce (`0–25 ms`), lever smoothing & X-axis inversion, and potentiometer calibration limits with real-time hardware LED preview.
  - Entered by holding **GP1 (Side Left)** on bootup.
- **Full Dual-Mode FN Layer (`fn_keymap[21]` & `fn_gamepad_map[21]`)**:
  - **Tap-Hold on `GP20` (Mid Space / Fn)**: Quick tap (`< 180 ms`) with no chord sends `GP20`'s normal Base Layer action (`Space` in Keyboard mode, `Guide` in Gamepad mode); holding `GP20` (`>= 180 ms`) or chording `GP20` with any other switch shifts the controller into **FN Layer (1)**.
  - **Bindable `FN Hold` Key (`240`)**: Any switch on the Base Layer can also be assigned as a dedicated momentary `FN Hold` modifier.
  - **Pass-Through (`▽ Base` / `254`)**: Switches set to `▽ Base` on the FN Layer transparently trigger their Base Layer assignment.
  - **On-Controller Hardware Shortcuts (`241–248`)**: Adjust lighting effects, brightness, save settings to Flash, or reboot into UF2 BOOTSEL directly from the controller without opening a browser:
    - `FN + GP0 (Left Menu)`: **LED Mode +** (Cycle Next Lighting Effect)
    - `FN + GP2 (Left Red)`: **Brightness -** (`-25`)
    - `FN + GP3 (Left Green)`: **LED Toggle** (Mute / Unmute LEDs)
    - `FN + GP4 (Left Blue)`: **Brightness +** (`+25`)
    - `FN + GP8 (Left Bottom 4)`: **Enter BOOTSEL (UF2)**
    - `FN + GP10 (Right Menu)`: **Save Active Settings to Flash**
  - **Visual FN Hint Lighting**: In Reactive LED mode, holding `FN` softly illuminates all switches that have an active FN Layer action at 25% of their assigned color.
- **Sega IO4 Emulation Mode** (for Sega ONGEKI arcade game support):
  - Pure IO4 emulation matching `lkick-io4` with Sega string descriptors.
  - Inverted side buttons for arcade optical sensor compatibility.
  - Game-controlled RGB lighting for the main 6 buttons (ONGEKI `SET_GENERAL_OUTPUT`, unaffected by KB/Gamepad LED modes or brightness).
  - **Test** (`Fn + GP0`), **Service** (`Fn + GP10`), and **Coin Chute** (`Fn + GP9`) shortcuts.
- **Full N-Key Rollover (NKRO) Keyboard Mode**:
  - 232-key bitmap report supporting all 21 buttons held down simultaneously with 0 rollover limit and 0 dropped keys at 1000Hz.
  - Customizable Base + FN Layer keybindings (including Left/Right modifiers, `FN Hold`, Hardware Shortcuts, and `None` unbind) stored in Flash memory.
- **XInput Gamepad Mode**:
  - Native Xbox 360 controller emulation at 1000Hz for PC rhythm games.
  - Fully remappable Base + FN Layer XInput button assignments per switch (`gamepad_map[21]` & `fn_gamepad_map[21]`).
  - Inverted & smoothed analog lever mapped to **Left Stick X-Axis** on a deterministic 1000Hz timebase.
- **4× Hardware Oversampled & Piecewise Dual-Slope Analog Lever (`GP26`)**:
  - Averages 4 back-to-back 12-bit ADC conversions on a deterministic 1ms tick and normalizes Left (`calib_max`) and Right (`calib_min`) independently around `calib_center` and `deadzone`.
- **0ms-Latency Eager Switch Debouncing**:
  - Configurable per-switch state machine (`0–25 ms`, default `3 ms`) that registers presses on the very first microsecond of contact and filters contact bounce during the lockout window.
- **Guided Potentiometer Calibration Mode**:
  - Interactive 3-stage LED-guided calibration on boot (**Left** = Left Wing LEDs + High ADC, **Right** = Right Wing LEDs + Low ADC, **Center** = Center LEDs) with a live 8-key bottom-bar lever position cursor.
- **Multicore-Safe Non-Volatile Flash Persistence**:
  - Safely parks Core 1 in SRAM while Core 0 writes configuration and calibration data to RP2040 QSPI Flash (`CONFIG_VERSION = 7`).

---

## Startup Button Combinations

Hold down one of these buttons when plugging in the USB cable:

| Button Pin | Action / Mode | Flash Stored? |
|---|---|:---:|
| **GP1** (Side Left) | Enter **WebSerial Configurator Mode** | - |
| **GP5** (Left Bottom 1) | Switch to **Sega IO4 Mode** (ONGEKI) | Yes |
| **GP6** (Left Bottom 2) | Switch to **Keyboard NKRO Mode** | Yes |
| **GP7** (Left Bottom 3) | Switch to **XInput Gamepad Mode** | Yes |
| **GP8** (Left Bottom 4) | Enter **BOOTSEL Mode** (UF2 flashing drive appears) | - |
| **GP20** (Fn / Mid Space) | Enter **Guided Potentiometer Calibration Mode** | Yes |

*Note: If no button is held at boot, the controller automatically starts in the last saved mode.*

---

## WebSerial Configurator Tool

1. Unplug the controller.
2. Hold down **GP1 (Side Left)** while plugging the controller into USB (all LEDs will pulse in Cyan/Blue).
3. Open [`configurator.html`](file:///c:/Users/papha/OneDrive/Documents/gekimax-fw/configurator.html) in Google Chrome, Microsoft Edge, or any Chromium browser.
4. Click **"Connect Device"** and select the Gekimax serial port.
5. Customize your Base Layer & FN Layer bindings, Hardware Shortcuts, LED colors/modes/brightness, debounce, and lever calibration visually, then click **"Save to Flash"** (`Ctrl+S`)!

---

## Pinout & Default Mapping Reference

| GPIO Pin | Gekimax Switch | LED | IO4 Mode (ONGEKI) | Keyboard Base / FN Layer | XInput Base / FN Layer |
|---|---|---|---|---|---|
| **GP0** | Left Menu (SW1) | D1 | Left Menu (`switches[1]:14`) | `Escape` / **LED Mode +** | `Back` / **LED Mode +** |
| **GP1** | Left Side (SW2) | D2 | Left Wall (Inverted) | `Left Shift` / `▽ Base` | `LB` / `▽ Base` |
| **GP2** | Left Red (SW3) | D3 | Left Red (`switches[0]:0`) | `A` / **Bright -** | `D-Pad Left` / **Bright -** |
| **GP3** | Left Green (SW4) | D4 | Left Green (`switches[0]:5`) | `S` / **LED Toggle** | `D-Pad Up` / **LED Toggle** |
| **GP4** | Left Blue (SW5) | D5 | Left Blue (`switches[0]:4`) | `D` / **Bright +** | `D-Pad Right` / **Bright +** |
| **GP5** | Left 1 (SW6) | D6 | *(Boot: IO4 Mode)* | `Z` / `F1` | `D-Pad Down` / `LS Up` |
| **GP6** | Left 2 (SW7) | D7 | *(Boot: KB Mode)* | `X` / `F2` | `LS Click` / `LS Down` |
| **GP7** | Left 3 (SW8) | D8 | *(Boot: XInput)* | `V` / `F3` | `RS Click` / `LS Left` |
| **GP8** | Left 4 (SW9) | D9 | *(Boot: BOOTSEL)* | `B` / **Enter BOOTSEL** | `B` / **Enter BOOTSEL** |
| **GP9** | Left Space (SW10) | D10 | *(Fn + GP9 = Coin)* | `C` / `Backspace` | `LT` / `▽ Base` |
| **GP10**| Right Menu (SW11)| D11 | Right Menu (`switches[0]:13`)| `Enter` / **Save Flash** | `Start` / **Save Flash** |
| **GP11**| Right Side (SW12)| D12 | Right Wall (Inverted) | `Right Shift` / `▽ Base` | `RB` / `▽ Base` |
| **GP12**| Right Red (SW13) | D13 | Right Red (`switches[0]:1`)| `L` / `Arrow Left` | `X` / `RS Left` |
| **GP13**| Right Green (SW14)| D14 | Right Green (`switches[1]:0`)| `;` / `Arrow Up` | `Y` / `RS Up` |
| **GP14**| Right Blue (SW15) | D15 | Right Blue (`switches[0]:15`)| `'` / `Arrow Right` | `A` / `RS Right` |
| **GP15**| Right 1 (SW16) | D16 | - | `N` / `F5` | `B` / `RS Down` |
| **GP16**| Right 2 (SW17) | D17 | - | `,` / `F6` | `RS Up` / `LS Click` |
| **GP17**| Right 3 (SW18) | D18 | - | `.` / `F7` | `RS Left` / `RS Click` |
| **GP18**| Right 4 (SW19) | D19 | - | `/` / `F8` | `RS Right` / `Guide` |
| **GP19**| Right Space (SW20)| D20 | - | `M` / `Arrow Down` | `RT` / `▽ Base` |
| **GP20**| Mid Space (SW21) | D21 | **Fn** (`+GP0` Test, `+GP10` Svc, `+GP9` Coin) | `Space` (Tap) / **FN Hold** | `Guide` (Tap) / **FN Hold** |
| **GP21**| 27× WS2812B LEDs | D1–D27 | Game-Controlled RGB | Configurable LED Effect | Configurable LED Effect |
| **GP26**| Potentiometer Lever | - | ONGEKI Lever (`analog[0]`) | - | **Left Stick X-Axis** (Inverted + Smoothed) |

---

## Flashing the Firmware

1. Hold **GP8** on the Gekimax while plugging it into your computer (or press **FN + GP8** in Keyboard/Gamepad mode, or click **Enter BOOTSEL (UF2)** in `configurator.html`).
2. A USB drive named `RPI-RP2` will appear.
3. Drag and drop [`build/gekimax.uf2`](file:///c:/Users/papha/OneDrive/Documents/gekimax-fw/build/gekimax.uf2) onto the `RPI-RP2` drive.
4. The Pico will flash and reboot automatically!
