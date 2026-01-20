# SY6970 Power Management IC Component

This component provides a driver for the Silergy SY6970 Single Cell Li-Ion DC/DC Switching Charger.

## Hardware Connection

**ACTUAL HARDWARE (from schematic):**
```
VSYS → R47 (1K) → LED1 (Red LED) → STAT pin (pin 4 of SY6970) → Open-drain output
```

The LED1 is controlled **DIRECTLY** by the SY6970's STAT pin (open-drain output). There is **no GPIO control** for this LED - it is entirely hardware-controlled by the PMIC.

**SOFTWARE PATTERN CONTROL:** By toggling the STAT_DIS bit (REG_07 bit 6), software can create **burst patterns** to indicate specific faults while leveraging the hardware's fixed 1Hz timing.

## LED Status Codes (STAT Pin)

The SY6970 indicates charging status via the hardware-controlled STAT pin LED:

| LED Behavior | Meaning | Description |
| :--- | :--- | :--- |
| **Off** | **Charge Done / Idle** | Battery is fully charged, charging is disabled, or device is in sleep mode. |
| **Continuous 1Hz Blink** | **Charging / Generic Fault** | Battery is charging OR an unspecified fault has occurred. |
| **2 Blinks + Pause** | **WDT Fault** | Watchdog timer expired - software pattern. |
| **3 Blinks + Pause** | **OVP Fault** | Battery overvoltage protection - software pattern. |
| **6 Blinks + Pause** | **Temperature Fault** | NTC temperature fault (SOS-like pattern) - software pattern. |

### Detailed Fault Conditions (1Hz Blink)

When the LED blinks at 1Hz, one or more of these fault conditions are active (read from **REG_0C** - Fault Status Register):

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

Use `sy6970_get_faults()` and `sy6970_decode_faults()` to read and interpret fault conditions in your application.

### Software Pattern Control

By toggling STAT_DIS (REG_07 bit 6), software creates burst patterns:
- **STAT_DIS = 0** (enabled): Hardware controls LED (1Hz blink when fault exists)
- **STAT_DIS = 1** (disabled): LED turns off

Software rapidly toggles this bit to create:
- **N blinks + pause**: Enable STAT for N seconds, disable for pause duration, repeat
- **Continuous**: Keep STAT enabled for generic fault indication

**Example:**
```c
// 3 blinks (3 seconds on), then 4 second pause
sy6970_led_set_mode(SY6970_LED_BLINK, (3 << 16) | 4);
```

## API Usage

See `sy6970.h` for the full C API.

```c
#include "sy6970.h"

// Initialize
sy6970_init();

// Get Battery Voltage
uint16_t vbat = sy6970_get_battery_voltage();
 
```
