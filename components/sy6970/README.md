# SY6970 Power Management IC Component

This component provides a driver for the Silergy SY6970 Single Cell Li-Ion DC/DC Switching Charger.

## Hardware Connection

**ACTUAL HARDWARE (from schematic):**
```
VSYS → R47 (1K) → LED1 (Red LED) → STAT pin (pin 4 of SY6970) → Open-drain output
```

The LED1 is controlled **DIRECTLY** by the SY6970's STAT pin (open-drain output). There is **no GPIO control** for this LED - it is entirely hardware-controlled by the PMIC.

## STAT LED Behavior

The SY6970 indicates charging status via the hardware-controlled STAT pin LED:

| LED State | Meaning | Description |
| :--- | :--- | :--- |
| **Solid ON** | **Charging** | STAT pin LOW - Battery is actively charging (precharge or fast charge). |
| **OFF** | **Charge Done / Idle** | STAT pin HIGH - Battery is fully charged, charging is disabled, or device is in sleep mode. |
| **Blinking 1Hz** | **Fault Condition** | Hardware automatic - One or more faults detected (see below). |

### Fault Detection

When the LED blinks at 1Hz, one or more fault conditions are active. **All faults blink identically at 1Hz** - you must read **REG_0C (Fault Status Register)** via I2C to determine the specific fault:

**Fault Types (REG_0C bits):**
1. **WDT_Expired** (bit 7) - Watchdog timer expired
2. **BOOST_Fault** (bit 6) - Boost converter fault
3. **CHG_Input_Fault** (bits 5:4 = 0x01) - Charge input fault
4. **CHG_Thermal_Shutdown** (bits 5:4 = 0x02) - Charge thermal shutdown
5. **CHG_Timer_Expired** (bits 5:4 = 0x03) - Charge safety timer expired
6. **BAT_OVP** (bit 3) - Battery overvoltage protection
7. **NTC Temperature Faults** (bits 2:0):
   - **NTC_Warm** (0x02) - Battery temperature warm
   - **NTC_Cool** (0x03) - Battery temperature cool
   - **NTC_Cold** (0x05) - Battery temperature cold
   - **NTC_Hot** (0x06) - Battery temperature hot

**Software API for fault reading:**
```c
// Read fault register (returns bitmask)
uint8_t faults = sy6970_get_faults();

// Get human-readable description
const char* fault_desc = sy6970_decode_faults(faults);
```

### Software Control

Software can only enable/disable the STAT pin output via `STAT_DIS` bit (REG_07[6]):
```c
// Enable STAT LED (hardware controls based on charge/fault state)
sy6970_enable_stat_led(true);

// Disable STAT LED (LED always off)
sy6970_enable_stat_led(false);
```

**Note:** The LED behavior (solid/blinking) is determined entirely by the charger IC hardware. Software cannot create custom blink patterns.

## Configuration & Defaults

The component is configured with "Practical Realistic Defaults" designed for safety and robustness on standard ESP32-S3 boards with 3.7V/4.2V LiPo batteries.

| Setting | Default Value | Description |
| :--- | :--- | :--- |
| **Input Current Limit** | **3000 mA** | Maximize draw from strong sources. Weak sources are handled by VINDPM. |
| **Min Input Voltage (VINDPM)** | **4400 mV** | (4.4V) Throttles charging if USB voltage sags below this. Safer than 4.5V for bad cables. |
| **Fast Charge Current** | **1024 mA** | (1.0A) Safe charge rate for standard 1000mAh+ batteries. |
| **Pre-Charge Current** | **128 mA** | Gentle recovery for deeply discharged batteries (<3.0V). |
| **Termination Current** | **128 mA** | Charging stops when current drops below this (Full Charge). |
| **Charge Voltage** | **4208 mV** | (4.2V) Standard max voltage for LiPo/Li-Ion. |
| **Min System Voltage** | **3500 mV** | (3.5V) Ensures VSYS stays high enough for 3.3V LDO stability even if battery is low. |
| **Boost Voltage (OTG)** | **5126 mV** | (~5.1V) Output voltage when acting as a power bank (OTG mode). |
| **HIZ Mode** | **Disabled** | High-Impedance Mode (Input disconnect). |
| **Shipping Mode** | **Disabled** | BATFET Control. |

## Advanced Features & Persistence

### NVS Persistence
This driver is integrated with Non-Volatile Storage (NVS). Settings changed via the API (or UI) are:
1. Applied immediately to the SY6970 registers.
2. Saved to NVS (`storage` partition).
3. **Restored automatically on boot** via `ui_pmic_restore_settings()`.

### Hot Reload (UI)
The "Defaults" feature in the UI performs a unique "Hot Reload":
1. Resets NVS values to the defaults table above.
2. Re-initializes the SY6970 registers.
3. **Destroys and Re-creates** the Settings UI page on the fly to reflect the new values immediately without a reboot.

### OTG (On-The-Go) / Boost Mode
The chip can reverse operation to provide 5V out on the VBUS (USB) pin from the battery.
- **Control**: `sy6970_enable_otg(bool enable)`
- **Voltage**: Configurable via `sy6970_set_boost_voltage()`.

### HIZ Mode (High Impedance)
Disconnects the VBUS input electrically while maintaining data lines (physically). The system runs purely on battery even if plugged in.
- Useful for "Disable USB Power" features.

### Shipping Mode (Hard Shutdown)
Disables the BATFET, effectively disconnecting the battery from the system.
- **Use**: Long-term storage or forcing a hard reset.
- **Wake**: Plug in USB power to reset the BATFET.

## API Usage

See `sy6970.h` for the full C API.

```c
#include "sy6970.h"

// Initialize (Applying defaults)
sy6970_init();

// Restore User Settings from NVS
// (Called automatically by UI init, or manually)
ui_pmic_restore_settings();

// Get Battery Voltage
uint16_t vbat = sy6970_get_battery_voltage_accurate(); // Pauses charging for precision

// Enable OTG
sy6970_enable_otg(true);
```

### VINDPM & Register 0x0D Details

The **Input Voltage Limit (VINDPM)** register (REG0D) has a specific behavior that differs slightly from some interpretations of the datasheet map.

*   **Formula**: `Voltage = 2.6V + (RegisterValue[6:0] * 100mV)`
*   **Absolute Minimum**: The hardware enforces a floor of approximately **3.9V** (Register Value 13 / `0x0D`).
*   **Behavior**: If you attempt to write a value that results in a voltage lower than 3.9V (e.g., trying to write calculated value 10 for 3.6V), the chip will **clamp** the setting to the minimum (13 / `0x0D`).
    *   *Example*: Writing `0x0A` (calculated as 3.6V via offset) results in a readback of `0x8D` (3.9V).
*   **Driver Implementation**: The driver correctly handles this by using the `2.6V` base offset in calculations, ensuring that requests for standard USB voltages (e.g., 4.4V, 4.5V) are mapped to correct register values (e.g., 4.4V -> Register 18 / `0x12`).
*   **Bit 7**: The driver sets Bit 7 (`0x80`) to force **Absolute VINDPM Threshold** mode.

