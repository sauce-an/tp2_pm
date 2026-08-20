# Smart AC/DC Power Measurement & Telemetry System

A bare-metal embedded firmware application developed on the **STM32L476RET6TR** microcontroller (ARM Cortex-M4 @ 80 MHz, 512 KB Flash, 128 KB SRAM, LQFP-64) for precision AC/DC power monitoring, real-time graphical display management, local SD-card logging, and dual-mode USB/Wi-Fi telemetry streaming.

---

## System Overview

```
                         +-------------------------------------------+
                         |      STM32L476RET6TR (ARM Cortex-M4)      |
                         |                                           |
[AC / DC Inputs] ------->| ADC1/2/3 (DMA & Polling Engines)          |
[Current MUX/Relay] <----| PC0 / RANGE_SELECT (Auto-Ranging)         |
[Pushbuttons] ----------->| GPIO (PA5-PA7, PB0-PB1) Debounce FSM      |
[ST7920 128x64 LCD] <----| SPI3 + DMA2 + Timer 1 PWM Backlight       |
[MicroSD Card] <---------| SPI2 (FatFS Data Logger)                  |
[Hardware RTC] <--------->| LSE 32.768kHz + Supercap Backup Check     |
[USB Host / PC] <-------->| USART3 (Direct UART Stream)               |
[Seeed Wi-Fi Module] <---| USART3 (Hardware Armor / 0V Clamp)        |
[Status LED] <-----------| PC4 (100ms Measurement Indicator)         |
                         +-------------------------------------------+
```

---

## Key Features

### 1. High-Speed Measurement Engines
- **AC Voltage & Current RMS:** Continuous 40ms high-speed sample accumulation with variance calculation to determine true RMS values.
- **Phase & Frequency Detection:** Dual-channel synchronous zero-crossing detection with hysteresis filtering to prevent false triggers from signal noise.
- **Power Analytics:** Real-time computation of Real Power (W), Reactive Power (VAR), Apparent Power (VA), Power Factor (PF), Voltage Peak-to-Peak (V_pp), and Current Peak-to-Peak (I_pp).
- **Hardware Auto-Ranging with Hysteresis:** Dynamic current range switching between High (> 1.1A) and Low (< 1.0A) hardware channels via `RANGE_SELECT_Pin` to eliminate relay chatter.
- **DC Channel Monitoring:** Dedicated ADC sampling for DC Voltage, Current, and Power computation.

### 2. Graphical Display & Menu FSM
- **ST7920 128x64 LCD Driver:** Framebuffer rendering via hardware SPI3 and DMA transmit completion callbacks.
- **Finite State Machine (FSM):** Hierarchical navigation covering Main Menu, DC/AC live inspection sub-menus, Settings, and a customizable 4-variable Live Summary Table.
- **PWM Backlight Driver:** Hardware Timer 1 (`TIM1_CH2`) generating variable-duty PWM for smooth, flicker-free brightness control.

### 3. Drift-Free Timed Execution & Hardware Protection
- **Drift-Free 1Hz Loop:** The system timing loop accumulates interval ticks (`lastCalcTime += 1000`) rather than polling absolute ticks post-transmission, ensuring transmission latency does not cause RTC synchronization drift.
- **Phantom Voltage Hardware Armor:** When USB is disconnected, USART3 lines (`PB10` and `PB11`) are clamped to ground (0V) or analog mode to eliminate a 1.1V back-feeding voltage from the Wi-Fi module that causes power rail browning and LCD tearing.
- **Pulsed Communication:** USART peripherals are activated only during the active transmission window (~300ms) and silenced during idle states.

### 4. Storage & Persistence
- **Emulated Non-Volatile Memory (NV):** Retains custom transformer step-down ratios, default telemetry routing modes, and table layout preferences directly in internal Flash memory without requiring an external EEPROM.
- **Hardware RTC & Supercapacitor Backup:** Backup register verification (`RTC_BKP_DR1 == 0x32F2`) preserves time registers across primary power losses.
- **FatFS SD-Card Logging:** On-demand CSV measurement recording over SPI2 triggered via dedicated hardware toggle (`PC8`).

---

## Hardware Pin Mapping

| Pin | Peripheral / Mode | Signal / Target | Description |
| :--- | :--- | :--- | :--- |
| **PC0** | `ADC1_IN1` | AC Current (High Range) | Primary high-current AC input channel |
| **PC1** | `ADC1_IN2` | AC Current (Low Range) | Amplified low-current AC input channel |
| **PC3** | `ADC3_IN4` | AC Voltage Input | Scaled AC voltage input channel |
| **PA1** | `ADC2_IN6` | DC Measurements | Multiplexed DC voltage/current sensor input |
| **PC7** | `GPIO_Output` | `RANGE_SELECT` | Switches external AC current shunt / relay path |
| **PC4** | `GPIO_Output` | Measurement LED | Diagnostic heartbeat indicator (pulses on measurement) |
| **PB10** | `USART3_TX` / `GPIO_Out` | Telemetry TX | Microcontroller TX to PC / Wi-Fi module |
| **PB11** | `USART3_RX` / `Analog` | Telemetry RX | Microcontroller RX from PC / Wi-Fi module |
| **PB2** | `EXTI_IT_RISING_FALLING` | USB VBUS Detect | Hardware interrupt line for USB cable connection sensing |
| **PD2** | `GPIO_Output` | `LCD_SELECT` (CS) | Chip Select line for ST7920 LCD |
| **PC4** | `GPIO_Output` | `LCD_RESET` | Hardware Reset line for ST7920 LCD |
| **PC10** | `SPI3_SCK` | LCD Clock | Serial clock for ST7920 graphic pipeline |
| **PC12** | `SPI3_MOSI` | LCD Data | Serial data transmission via DMA |
| **PA9** | `TIM1_CH2` | LCD Backlight PWM | Timer 1 PWM output for brightness adjustments |
| **PB12** | `GPIO_Output` | `SD_CARD_SELECT` | Chip Select for SPI MicroSD Card |
| **PB13-15** | `SPI2` | SD Card Bus | SPI2 interface for FatFS data logging |
| **PA5** | `GPIO_Input_PU` | Brightness Down | Active-low user pushbutton |
| **PA6** | `GPIO_Input_PU` | Brightness Up | Active-low user pushbutton |
| **PA7** | `GPIO_Input_PU` | Menu Down / Scroll | Active-low user pushbutton |
| **PB0** | `GPIO_Input_PU` | Menu Select / Enter | Active-low user pushbutton |
| **PB1** | `GPIO_Input_PU` | Menu Up / Back | Active-low user pushbutton |
| **PC8** | `GPIO_Input` | SD Log Toggle | Hardware toggle switch for data logging to SD card |

> **Note:** `PC4` appears twice in the table above (Measurement LED and `LCD_RESET`). Confirm whether this is intentional pin sharing or a documentation error before others rely on this table.

---

## Telemetry Protocol Specification

Telemetry data is transmitted once per second at **115200 Baud, 8N1**.

```
[Timestamp],[15 Comma-Separated Values]|SPEC_V:[500-Byte Hex]|SPEC_I:[500-Byte Hex]\r\n
```

### 1. Measurement Payload Format
```
HH:MM:SS,DC_V,DC_I,DC_W,AC_V,AC_I,FREQ,PHASE,REAL_W,REAC_VAR,APP_VA,PF,V_PP,I_PP,THD_V,THD_I
```
- **Under-Limit Indicator:** Unconnected or noise-floor signals are emitted as `-999.00` or `-1.00`.

### 2. Dense Hex Frequency Spectra
To prevent stack overflows and minimize transmission blocking time, 500 frequency bins (0 Hz to 500 Hz, 1 Hz non-interpolated resolution) for both Voltage and Current are transmitted as contiguous 2-character hexadecimal byte streams:
- `|SPEC_V:` — header followed by 1000 hex characters (500 bins x 2 hex digits).
- `|SPEC_I:` — header followed by 1000 hex characters (500 bins x 2 hex digits).

### 3. Remote RTC Synchronization
To set the internal hardware Real-Time Clock remotely, transmit the ASCII character `T` followed by a 14-byte date-time packet and a null terminator `\0`:

```
T<DD><MM><YYYY><HH><MM><SS>\0
```
- **Example:** `T20082026120000\0` synchronizes the device to 20/08/2026 12:00:00.

---

## Hardware Architecture & Schematics

Hardware design files, PCB schematics, and simulation models are organized in the [`Hardware/`](file:///Hardware/) directory:

- **PCB Schematics ([`Hardware/Schematics/`](file:///Hardware/Schematics/)):**
  - [`ENGG3800_PCB_V2_schemetic.pdf`](file:///Hardware/Schematics/ENGG3800_PCB_V2_schemetic.pdf) — Production Revision V2 PCB Schematic with integrated AC auto-ranging, DC sensing, ST7920 display, and telemetry isolation circuits.
  - [`ENGG3800_PCB_V1_schemetic.pdf`](file:///Hardware/Schematics/ENGG3800_PCB_V1_schemetic.pdf) — Revision V1 PCB Schematic.
  - [`ENGG3800 schemetic for seminar.pdf`](file:///Hardware/Schematics/ENGG3800%20schemetic%20for%20seminar.pdf) — Presentation and seminar hardware schematic.
- **System Specifications ([`Hardware/Specifications/`](file:///Hardware/Specifications/)):**
  - [`power_meter_spec_V2.pdf`](file:///Hardware/Specifications/power_meter_spec_V2.pdf) — Complete hardware specifications, measurement tolerances, and voltage/current input limits.
  - [`power_meter_spec.pdf`](file:///Hardware/Specifications/power_meter_spec.pdf) — Initial specification document.
  - [`ENGG3800_2026s1_demo1_criteria_sheet.pdf`](file:///Hardware/Specifications/ENGG3800_2026s1_demo1_criteria_sheet.pdf) — Project evaluation and demonstration criteria.
- **Analog Circuit Simulation ([`Hardware/Simulation/`](file:///Hardware/Simulation/)):**
  - [`AC Signal Measuring/`](file:///Hardware/Simulation/AC%20Signal%20Measuring/) — LTspice simulation models (`.asc`, `.raw`) for front-end AC voltage and current conditioning circuits.
- **UI State Machine Diagram ([`Hardware/Diagrams/`](file:///Hardware/Diagrams/)):**
  - [`TP2-seminar LCD FSM.drawio.png`](file:///Hardware/Diagrams/TP2-seminar%20LCD%20FSM.drawio.png) — Visual Finite State Machine diagram for LCD navigation.

---

## Firmware Architecture

```
Core/
├── Inc/
│   ├── main.h             # Hardware pin definitions & peripheral handles
│   ├── AC_meas.h          # AC RMS, auto-ranging, and power calculation headers
│   ├── DC_meas.h          # DC measurement sampling headers
│   ├── menu.h             # FSM state declarations & display structures
│   ├── lcd_new.h          # ST7920 graphic driver & low-level drawing primitives
│   ├── comm.h             # Telemetry serial formatting and command parser
│   ├── connect.h          # USB/Wi-Fi connection state management
│   ├── brightness.h       # PWM timer backlight API
│   ├── saveNV.h           # Emulated Non-Volatile Flash memory API
│   ├── sd_comm.h          # FatFS SD-card stream logger
│   └── wifi.h             # Wi-Fi module communication & configuration
└── Src/
    ├── main.c             # System clock init, main 1Hz loop & peripheral setups
    ├── AC_meas.c          # AC sampling loops, zero-crossing, and power math
    ├── DC_meas.c          # DC ADC filtering and scaling routines
    ├── menu.c             # Menu navigation FSM, debouncing, and UI renderers
    ├── lcd_new.c          # ST7920 display driver & graphics routines
    ├── comm.c             # Dense Hex streaming and RTC sync receiver
    ├── connect.c          # EXTI callbacks, pin clamping, and safe UART restoring
    ├── brightness.c       # PWM backlight brightness control implementation
    ├── saveNV.c           # Flash memory read/write persistence routines
    ├── sd_comm.c          # FatFS SD-card stream logger implementation
    └── wifi.c             # Wi-Fi module telemetry interface

Hardware/
├── Schematics/            # PCB Revision V1, V2, and seminar schematics (PDF)
├── Specifications/        # Power meter specifications & demonstration criteria
├── Simulation/            # LTspice front-end circuit simulation models
└── Diagrams/              # LCD Finite State Machine flowcharts
```

---

## Build & Flash Instructions

### Prerequisites
- **Target MCU:** **STM32L476RET6TR** (ARM Cortex-M4 @ 80 MHz, 512 KB Flash, 128 KB SRAM, LQFP-64)
- **IDE / Toolchain:** [STM32CubeIDE](https://www.st.com/en/development-tools/stm32cubeide.html) (Version 1.14.0 or newer)
- **Compiler:** `arm-none-eabi-gcc` (C99 standard enabled)
- **Hardware Debugger:** ST-Link V2 / V3 with SWD interface (SWDIO, SWCLK, GND)

### Building the Project

1. Clone this repository:
   ```bash
   git clone https://github.com/sauce-an/Power_meter_tp2.git
   ```

2. Open STM32CubeIDE and choose **File > Open Projects from File System...**, selecting the repository root.
3. Build the binary using **Project > Build Project** (`Ctrl + B`).

### Flashing to Target

1. Connect the ST-Link programmer to the target SWD header (ensure PA13/PA14 and Ground are connected).
2. Click **Run > Debug** (`F11`) to flash the `.elf` binary to the **STM32L476RET6TR** internal Flash.
3. Resume execution (`F8`) to begin runtime operation.

---

## Author & Lead Firmware Engineer

- **An (Andrew) Jian:** Lead Firmware Architecture, ST7920 Graphical LCD Driver, Menu FSM Engine, Telemetry & Detection Logic, PWM Brightness Driver, Internal Flash NV Storage, Hardware RTC Synchronization, DC Measurement Driver, AC Driver Co-development.
