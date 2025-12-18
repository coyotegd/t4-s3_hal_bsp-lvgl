# RM690B0 DVR - LilyGo T4-S3 Display Driver

This project implements a working ESP-IDF driver for the **LilyGo T4-S3** (2.41" AMOLED) development board. 

## The "Struggle" & The Solution

Getting this display to work was a significant challenge due to unique hardware design choices and undocumented protocols. This project serves as a reference for anyone facing similar issues.

### 1. The "No DCX" Mystery
Most SPI/8080 displays use a **D/C (Data/Command)** pin to distinguish between register writes and pixel data. The RM690B0 on this board **does not have a D/C pin**.
- **Initial Failure:** Standard SPI and QSPI drivers failed because they rely on the D/C pin or standard SPI command structures.
- **Discovery:** By analyzing the Arduino `LilyGo-AMOLED-Series` library, we discovered a custom QSPI wrapper protocol is required.
- **The Protocol:**
    - **Commands:** Sent using SPI Opcode `0x02` (Page Program). The actual command byte is shifted into the **Address Phase** (24-bit).
    - **Pixel Data:** Sent using SPI Opcode `0x32` (Quad Page Program). The Address is **0x003C00** (See "The Addressing Anomaly" below).

### 2. The Power Enable (GPIO 9)
Even with the correct protocol, the display remained black.
- **Discovery:** A deep dive into the schematic (`T4-S3-240719.pdf`) and the `LilyGo_AMOLED.h` header file revealed a critical pin: **GPIO 9**.
- **Function:** GPIO 9 controls the Enable pin of the PMIC/Display power rail.
- **Fix:** Pulling GPIO 9 HIGH during initialization immediately brought the screen to life.

### 3. PMIC Watchdog (SY6970)
The board uses an SY6970 PMIC which has a default watchdog enabled.
- **Issue:** The board would reset or power cycle if the watchdog wasn't fed or disabled.
- **Fix:** The driver initializes the SY6970 over I2C (`0x6A`) and explicitly disables the watchdog in Register `0x07`.

## Hardware Pin Map

| Signal | GPIO | Notes |
| :--- | :--- | :--- |
| **CS** | 11 | Chip Select |
| **SCK** | 15 | Clock |
| **D0** | 14 | Data 0 |
| **D1** | 10 | Data 1 |
| **D2** | 16 | Data 2 |
| **D3** | 12 | Data 3 |
| **RST** | 13 | Reset |
| **TE** | 18 | Tearing Effect |
| **PMIC_EN** | 9 | **CRITICAL:** Power Enable |
| **I2C_SDA** | 6 | PMIC/Touch I2C |
| **I2C_SCL** | 7 | PMIC/Touch I2C |

## Build & Flash

```bash
idf.py build
idf.py -p /dev/ttyACM0 flash monitor
```

### 4. The Addressing Anomaly (0x2C00 vs 0x3C00)
During the integration of the BSP, we encountered a persistent "Black Screen" issue despite correct initialization.
- **Standard Behavior:** The RM690B0 datasheet specifies `0x2C` (RAMWR) as the command to write memory. In a standard SPI implementation, you would send command `0x2C` and then stream data.
- **The Anomaly:** In this specific QSPI implementation (Opcode `0x32`), the controller **ignores** data sent to address `0x002C00`.
- **The Fix:** Analysis of the working Arduino library revealed that while the `RAMWR` command (0x2C) is sent first to prepare the display, the actual pixel data stream **MUST** be directed to address **`0x003C00`**.
- **Why?** This is likely an internal memory map offset or a specific configuration of the QSPI interface on this module. Changing the address from `0x002C00` to `0x003C00` immediately fixed the display.
