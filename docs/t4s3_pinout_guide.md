# LilyGo T4-S3 Complete Pinout Guide

## Board Overview

The LilyGo T4-S3 is an ESP32-S3-based development board featuring:

- **MCU:** ESP32-S3-R8 (8MB PSRAM)
- **Display:** 2.41-inch RM690B0 AMOLED (450×600, QSPI interface)
- **Touch:** CST226SE Capacitive Touch Controller (I2C)
- **Storage:** W25Q128 16MB Flash + MicroSD Card Slot
- **Power:** SY6970 Battery Management IC + SY8089 3.3V Buck Converter
- **Connectivity:** USB Type-C, Qwiic/STEMMA QT connector, IPEX antenna
- **Expansion:** 2×15 pin header with GPIO access

---

## Pin Assignments by Function

### Display (RM690B0 AMOLED - QSPI Interface)

| Function      | GPIO Pin | Description                              |
| ------------- | -------- | ---------------------------------------- |
| QSPI DATA0    | GPIO 14  | SPI Data Line 0 (SDA in single-line mode)|
| QSPI DATA1    | GPIO 10  | SPI Data Line 1                          |
| QSPI DATA2    | GPIO 16  | SPI Data Line 2                          |
| QSPI DATA3    | GPIO 12  | SPI Data Line 3                          |
| QSPI SCK      | GPIO 15  | SPI Clock                                |
| QSPI CS       | GPIO 11  | Chip Select (Active Low)                 |
| Display Reset | GPIO 13  | Hardware Reset (Active Low)              |
| AMOLED TE     | GPIO 18  | Tearing Effect signal for sync           |
| Power Enable  | GPIO 9   | **CRITICAL:** HIGH to power display      |

**Interface Mode:** 4-wire SPI/QSPI (IM[1:0] = 10)

**Display Power Supply:**

- **BV6804 Voltage Converter** generates display voltages:
  - AVDD (Analog VDD)
  - OVDD (Output VDD)
  - VON (Positive voltage)
  - OVSS (Negative voltage)
- Controlled via SWIRE protocol
- Input: VSYS via 2.2µH inductor

---

### Touch Controller (CST226SE - I2C Interface)

| Function       | GPIO Pin | Description                    |
| -------------- | -------- | ------------------------------ |
| I2C SDA        | GPIO 6   | I2C Data Line                  |
| I2C SCL        | GPIO 7   | I2C Clock Line                 |
| Touch Interrupt| GPIO 8   | Touch event interrupt (Low)    |
| Touch Reset    | TP_RST   | Hardware reset for touch IC    |

**I2C Pull-ups:** 10kΩ resistors to VDD3V3

**Connector:** 6-pin AFC04-S06ECA-00 FPC connector (shared with Qwiic/I2C)

---

### MicroSD Card (SPI Interface)

| Function | GPIO Pin | Description         |
| -------- | -------- | ------------------- |
| SD_CS    | GPIO 1   | Chip Select         |
| SD_MOSI  | GPIO 2   | Master Out Slave In |
| SD_SCK   | GPIO 3   | Clock               |
| SD_MISO  | GPIO 4   | Master In Slave Out |

**Card Slot:** Self-ejecting MicroSD card socket

**Pull-ups:** 10kΩ resistors on CS pin

**Decoupling:** 1µF capacitor near slot

---

### Flash Memory (W25Q128 - QSPI)

| Function | ESP32-S3 Pin | Description            |
| -------- | ------------ | ---------------------- |
| SPIQ     | SPIQ         | Data Out (MISO)        |
| SPID     | SPID         | Data In (MOSI)         |
| SPICLK   | SPICLK       | Clock                  |
| SPICS0   | SPICS0       | Chip Select 0          |
| SPIWP    | SPIWP        | Write Protect          |
| SPIHD    | SPIHD        | Hold                   |

**Chip:** W25Q128 (16MB/128Mbit)

**Voltage:** VDD_SPI (connected to VDD3V3 via 0Ω resistor R7)

**Decoupling:** 0.1µF capacitor

---

### PSRAM (Octal SPI)

| Function | ESP32-S3 Pin | Notes                          |
| -------- | ------------ | ------------------------------ |
| SPICS1   | SPICS1       | Chip Select 1 for PSRAM        |
| Multiple | SPIQ/D/CLK   | Shared with Flash ESP32-S3-R8  |

**Capacity:** 8MB Octal PSRAM (integrated in ESP32-S3-R8 module)

---

### Battery Management (SY6970 PMIC)

| Function   | GPIO Pin | Description                    |
| ---------- | -------- | ------------------------------ |
| PMIC INT   | GPIO 5   | Interrupt from SY6970          |
| I2C SDA    | GPIO 6   | I2C Data (shared with touch)   |
| I2C SCL    | GPIO 7   | I2C Clock (shared with touch)  |

**I2C Address:** 0x6A (typical for SY6970)

**Key Features:**

- USB Type-C BC1.2 detection (DP/DM pins)
- Battery charging (up to 5A fast charge)
- OTG boost mode (5V output)
- Power path management
- NTC temperature sensing
- DSEL pin for charger type detection
- STAT LED output (connected to LED_G via 1kΩ resistor)

**External Components:**

- 2.2µH inductor (MSK12C02-HB) for switching
- Input/output capacitors (22µF, 100µF)
- 10kΩ NTC thermistor for battery temperature

---

### 3.3V Power Regulator (SY8089)

| Function | Connection | Purpose                          |
| -------- | ---------- | -------------------------------- |
| VIN      | VSYS       | Input from battery/USB           |
| VOUT     | VDD3V3     | 3.3V output for system           |
| EN       | R14 10kΩ   | Enable (pull-up to enable)       |
| FB       | R divider  | Feedback for voltage regulation  |

**Output:** 3.3V @ up to 2A

**Inductor:** 2.2µH

**Capacitors:** 22µF input, 22µF + 10µF output

---

### USB Interface (Type-C)

| Function | Connection | Description                  |
| -------- | ---------- | ---------------------------- |
| D+       | D+         | USB Data Plus                |
| D-       | D-         | USB Data Minus               |
| VBUS     | VBUS       | 5V USB Power                 |
| CC1/CC2  | 5.1kΩ-GND  | USB-C config resistors       |

**Connector:** USB Type-C receptacle

**Protection:** ESD protection diodes on data lines

**Power:** VBUS connects to SY6970 for charging and to VSYS via power path

---

### UART Debug Interface

| Function | GPIO Pin | Description               |
| -------- | -------- | ------------------------- |
| U0TXD    | GPIO 43  | UART0 Transmit            |
| U0RXD    | GPIO 44  | UART0 Receive             |

**Connector:** Available on 2×15 pin header (P5)

**Voltage:** 3.3V logic level

---

### Qwiic/STEMMA QT Connector (I2C)

| Pin | Function | GPIO Pin | Description          |
| --- | -------- | -------- | -------------------- |
| 1   | GND      | -        | Ground               |
| 2   | VDD3V3   | -        | 3.3V Power           |
| 3   | SDA      | GPIO 6   | I2C Data             |
| 4   | SCL      | GPIO 7   | I2C Clock            |

**Connector Type:** JST-SH 1.0mm 4-pin

**Shared I2C Bus:** Touch controller (CST226SE) and battery management (SY6970) also on this bus

**Pull-ups:** 10kΩ resistors (R8, R10) to VDD3V3

---

### Expansion Header (P5 - 2×15 Pin)

| Pin | GPIO/Signal    | Pin | GPIO/Signal      |
| --- | -------------- | --- | ---------------- |
| 1   | VBUS           | 16  | GND              |
| 2   | VDD3V3         | 17  | BAT              |
| 3   | U0TXD (43)     | 18  | GND              |
| 4   | U0RXD (44)     | 19  | VDD3V3           |
| 5   | GPIO 21        | 20  | GND              |
| 6   | GPIO 47        | 21  | GPIO 1 (SD_CS)   |
| 7   | GPIO 48        | 22  | GPIO 2 (SD_MOSI) |
| 8   | GPIO 38        | 23  | GPIO 3 (SD_SCK)  |
| 9   | GPIO 39        | 24  | GPIO 4 (SD_MISO) |
| 10  | GPIO 40        | 25  | II2C_SCL (7)     |
| 11  | GPIO 41        | 26  | II2C_SDA (6)     |
| 12  | GPIO 42        | 27  | U0RXD (44)       |
| 13  | GND            | 28  | U0TXD (43)       |
| 14  | BAT            | 29  | VDD3V3           |
| 15  | GND            | 30  | VBUS             |

**Note:** Some GPIOs are already used by onboard peripherals. See conflict warnings below.

---

### Buttons

| Button | GPIO Pin | Function                       |
| ------ | -------- | ------------------------------ |
| Boot   | GPIO 0   | Bootloader when held reset     |
| Reset  | CHIP_PU  | Hardware reset for ESP32-S3    |

**Boot Button:** Pulls GPIO 0 to GND when pressed

**Reset Button:** Pulls CHIP_PU to GND when pressed

**Pull-ups:** 10kΩ on both lines

---

### LED

| LED   | Connection | GPIO/Control | Notes                        |
| ----- | ---------- | ------------ | ---------------------------- |
| LED_G | via R47    | STAT pin     | Red LED for charging status  |

**Resistor:** 1kΩ current limiting

**Function:** Indicates charging/power status from SY6970 PMIC

---

### RF/Antenna

| Component      | Connection | Description                                             |
| -------------- | ---------- | ------------------------------------------------------- |
| PCB Antenna    | Integrated | Default onboard 2.4GHz antenna                          |
| IPEX Connector | J5         | External antenna connector                              |
| Antenna Switch | -          | Select between PCB and external via 0Ω resistor R1      |

**Matching Network:**

- 3.9nH inductor (L1)
- 2.0nH inductor
- 1.8pF, NC capacitors for impedance matching
- 24nH inductor (L6) for RF filtering

---

### Crystal Oscillators

| Crystal | Frequency | GPIO Connection | Purpose           |
| ------- | --------- | --------------- | ----------------- |
| Y1      | 40MHz     | XTAL_P/XTAL_N   | Main system clock |

**Load Capacitors:** 12pF on each crystal pin

**Note:** 32.768kHz RTC crystal pins (XTAL_32K_P/N) available but not populated

---

## Power Distribution

### Power Rails

| Rail    | Source         | Voltage | Current | Usage                    |
| ------- | -------------- | ------- | ------- | ------------------------ |
| VBUS    | USB Type-C     | 5V      | -       | USB input power          |
| BAT     | Battery        | 3.7V    | -       | Battery connection       |
| VSYS    | SY6970         | 3.3-4.2V| Up to 9A| System power             |
| VDD3V3  | SY8089         | 3.3V    | 2A      | Main 3.3V rail           |
| VDD_SPI | VDD3V3         | 3.3V    | -       | Flash/PSRAM supply       |
| VDD3P3  | Internal       | 3.3V    | -       | ESP32-S3 internal        |
| VDDA    | VDD3V3         | 3.3V    | -       | ESP32-S3 analog supply   |

### Display Power Rails (from BV6804)

| Rail  | Voltage | Purpose                |
| ----- | ------- | ---------------------- |
| ELVDD | ~6V     | Display positive supply|
| ELVSS | ~-4V    | Display negative supply|
| AVDD  | ~5.8V   | Display analog supply  |
| OVDD  | ~7V     | Display output driver  |
| VON   | ~5V     | Display gate-on voltage|

**Control:** BV6804 converter controlled via SWIRE from ESP32-S3

---

## GPIO Usage Summary

### Reserved/Used GPIOs

| GPIO | Function            | Peripheral      | Notes                  |
| ---- | ------------------- | --------------- | ---------------------- |
| 0    | Boot Button         | User Input      | Low = bootloader mode  |
| 1    | SD_CS               | MicroSD         | SPI Chip Select        |
| 2    | SD_MOSI             | MicroSD         | SPI MOSI               |
| 3    | SD_SCK              | MicroSD         | SPI Clock              |
| 4    | SD_MISO             | MicroSD         | SPI MISO               |
| 5    | 6970_INT            | PMIC            | Interrupt from batt IC |
| 6    | II2C_SDA            | I2C Bus         | Touch, PMIC, Qwiic     |
| 7    | II2C_SCL            | I2C Bus         | Touch, PMIC, Qwiic     |
| 8    | TP_INT              | Touch           | Touch interrupt        |
| 9    | V_EN                | Display Power   | **CRITICAL** power     |
| 10   | QSPI DATA1          | Display         | Display data line      |
| 11   | QSPI CS             | Display         | Display chip select    |
| 12   | QSPI DATA3          | Display         | Display data line      |
| 13   | Display Reset       | Display         | Reset display          |
| 14   | QSPI DATA0          | Display         | Display data line (SDA)|
| 15   | QSPI SCK            | Display         | Display clock          |
| 16   | QSPI DATA2          | Display         | Display data line      |
| 18   | AMOLED TE           | Display         | Tearing effect signal  |
| 43   | U0TXD               | Debug UART      | Serial transmit        |
| 44   | U0RXD               | Debug UART      | Serial receive         |

### Available GPIOs (on expansion header)

**Free for general use:**

- GPIO 21, 38, 39, 40, 41, 42, 47, 48

**Note:** GPIO 19, 20 may be available but check ESP32-S3 USB-OTG usage if needed

---

## Pin Conflict Warnings

⚠️ **I2C Bus Shared:** GPIO 6 (SDA) and GPIO 7 (SCL) are shared between:

- Touch controller (CST226SE)
- Battery management IC (SY6970)
- Qwiic connector

All devices must have unique I2C addresses.

⚠️ **Critical Display Power:** GPIO 9 (V_EN) must be set HIGH before attempting to communicate with the display, or the display will not respond.

⚠️ **MicroSD Conflicts:** GPIOs 1-4 are dedicated to MicroSD. If not using SD card, these can be repurposed, but requires hardware modification.

⚠️ **UART Conflict:** GPIO 43/44 (U0TXD/U0RXD) are used for USB-CDC and debug output. Using them for other purposes may interfere with serial console.

---

## Voltage Levels

**All GPIO:** 3.3V logic levels

**USB:** 5V power input

**Battery:** 3.7V nominal (2.6V-4.35V operating range)

**Display Signals:** 3.3V logic, but internal display voltages are generated by BV6804

---

## Component Reference

### ICs

| Component | Part Number | Function                  |
| --------- | ----------- | ------------------------- |
| U2        | ESP32-S3-R8 | controller (8MB PSRAM)    |
| U3        | W25Q128     | 16MB Flash memory         |
| U4        | SY8089      | 3.3V buck converter (2A)  |
| U5        | SY6970      | Battery management IC     |
| U6        | BV6804      | Display power converter   |

### Connectors

| Connector | Type                  | Purpose                    |
| --------- | --------------------- | -------------------------- |
| J1        | USB Type-C            | Power and data             |
| P2        | JST-SH 1.0mm 4-pin    | Qwiic/I2C connector        |
| P3        | JST-GH 1.25mm 2-pin   | Battery connector          |
| P4        | JST-SH 1.0mm 4-pin    | Duplicate Qwiic            |
| P5        | 2×15 pin header       | GPIO expansion             |
| P6        | AFC04-S06ECA-00       | Touch FPC connector        |
| P8        | FH35C-45S-0.3SHW(50)  | Display FPC con   (45-pin) |
| J5        | IPEX                  | External antenna connector |
| -         | MicroSD slot          | TF card storage            |

### Switches and Buttons

| Component | Type      | Function      |
| --------- | --------- | ------------- |
| S1/SW4    | Tactile   | Boot button   |
| S2/SW4    | Tactile   | Reset button  |
| SW        | Slide     | Batt switch   |

---

## Schematic Notes

### Power Sequencing

1. **Power On:**
   - VBUS or BAT → SY6970 → VSYS
   - VSYS → SY8089 → VDD3V3
   - VDD3V3 powers ESP32-S3 and peripherals

2. **Display Power:**
   - ESP32-S3 must set GPIO 9 (V_EN) HIGH
   - BV6804 generates display voltages
   - Configure via SWIRE protocol

### I2C Bus Topology

```text
VDD3V3
  │
  ├─[R8 10kΩ]──── SDA (GPIO 6)
  │               │
  └─[R10 10kΩ]─── SCL (GPIO 7)
                  │
                  ├── CST226SE (Touch)
                  ├── SY6970 (PMIC) 
                  └── Qwiic Connector (P2/P4)
```

### Reset Circuit

```text
VDD3V3
  │
  └─[R82 10kΩ]──── CHIP_PU
                   │
                   └──[S2 Reset]─── GND
```

### Boot Selection

```text
VDD3V3
  │
  └─[R5 10kΩ]───── GPIO 0
                   │
                   └──[S1 Boot]──── GND
```

---

## Layout Recommendations

When designing with this board:

1. **Keep I2C traces short** - All I2C devices share GPIO 6/7
2. **Decouple power rails** - Add 0.1µF caps near each IC
3. **Antenna clearance** - Keep 5mm clearance around antenna area
4. **Display FPC routing** - Route QSPI signals with equal length where possible
5. **Battery safety** - Use protection circuit if using bare LiPo cells
6. **ESD protection** - Sensitive to static on USB and display connectors

---

## Firmware Configuration

### ESP-IDF Example Config

```c
// Display Power Enable
#define DISPLAY_POWER_PIN GPIO_NUM_9

// QSPI Display
#define QSPI_DATA0_PIN GPIO_NUM_14
#define QSPI_DATA1_PIN GPIO_NUM_10
#define QSPI_DATA2_PIN GPIO_NUM_16
#define QSPI_DATA3_PIN GPIO_NUM_12
#define QSPI_SCK_PIN   GPIO_NUM_15
#define QSPI_CS_PIN    GPIO_NUM_11
#define DISPLAY_RST_PIN GPIO_NUM_13
#define DISPLAY_TE_PIN GPIO_NUM_18

// Touch I2C
#define TOUCH_I2C_SDA GPIO_NUM_6
#define TOUCH_I2C_SCL GPIO_NUM_7
#define TOUCH_INT_PIN GPIO_NUM_8

// SD Card SPI
#define SD_MISO_PIN GPIO_NUM_4
#define SD_MOSI_PIN GPIO_NUM_2
#define SD_SCK_PIN  GPIO_NUM_3
#define SD_CS_PIN   GPIO_NUM_1

// PMIC
#define PMIC_INT_PIN GPIO_NUM_5
```

---

## Revision History

| Version | Date     | Changes                               |
| ------- | -------- | ------------------------------------- |
| 1.0     | Feb 2026 | Initial pinout guide from schematics  |

---

## Additional Resources

- **Datasheet:** ESP32-S3 Technical Reference Manual
- **Display:** RM690B0 AMOLED Driver IC Documentation
- **Touch:** CST226SE Hynitron Touch Controller Guide
- **Power:** SY6970 Battery Management IC Datasheet
- **Flash:** W25Q128 Winbond Serial Flash Datasheet

---

**Document Source:** Compiled from `t4_s3_schematic.txt` and `t4s3pins.txt`

**Board:** LilyGo T4-S3 (2.41" AMOLED version)

**Hardware Version:** As documented in schematics (3 sheets)
