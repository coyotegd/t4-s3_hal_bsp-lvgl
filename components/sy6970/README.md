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

## API Usage

See `sy6970.h` for the full C API.

```c
#include "sy6970.h"

// Initialize
sy6970_init();

// Get Battery Voltage
uint16_t vbat = sy6970_get_battery_voltage();
 
```
