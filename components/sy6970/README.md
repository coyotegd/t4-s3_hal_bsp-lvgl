# SY6970 Power Management IC Component

This component provides a driver for the Silergy SY6970 Single Cell Li-Ion DC/DC Switching Charger.

## LED Status Codes (STAT Pin)

The SY6970 indicates charging status via the open-drain STAT pin, which is typically connected to an LED.

| LED Behavior | Meaning | Description |
| :--- | :--- | :--- |
| **Solid On** | **Charging** | Battery is currently charging (Pre-charge, Fast-charge, or Constant Voltage). |
| **Off** | **Charge Done** / **Idle** | Battery is fully charged, charging is disabled, or device is in sleep mode. |
| **Blinking (1Hz)** | **Fault** / **Suspend** | A charging fault or suspend condition has occurred. |

### Blinking Fault Conditions
If the LED is blinking at approximately 1Hz, one of the following conditions is active:

1.  **Input Fault**: Input voltage is too high (OVP) or too low (bad adapter).
2.  **Thermal Shutdown**: The IC junction temperature is too high.
3.  **Safety Timer Expiration**: The charging took longer than the safety limit (default 12h), indicating a potential battery issue.
4.  **Battery Fault**: Battery Over-Voltage (BATOVP).
5.  **Watchdog Fault**: The watchdog timer expired (if enabled and not reset by host).
6.  **Boost Mode Fault**: (OTG Mode) Overload or short circuit on VBUS.
7.  **Charge Suspend**: Charging suspended due to temperature (NTC) or manual control.

## API Usage

See `sy6970.h` for the full C API.

```c
#include "sy6970.h"

// Initialize
sy6970_init();

// Get Battery Voltage
uint16_t vbat = sy6970_get_battery_voltage();

// Control LED manually (if STAT_DIS bit is set)
sy6970_set_stat_led(true); 
```
