# SY6970 PMIC Component

This component manages the SY6970 Power Management IC used on the LilyGo T4-S3.

## Critical Configuration

The SY6970 has a built-in watchdog timer that will reset the power rails if not serviced or disabled. For this driver, we disable it during initialization.

### I2C Configuration
- **Address:** `0x6A`
- **SDA:** GPIO 6
- **SCL:** GPIO 7
- **Frequency:** 400kHz

### Initialization Steps
1. **Read Chip ID:** Verify communication (Reg `0x14`).
2. **Disable Watchdog:**
   - Register: `0x07`
   - Action: Clear bits 4 and 5.
3. **Disable OTG:**
   - Register: `0x03`
   - Action: Clear bit 5.
   - *Note:* Early attempts enabled OTG, but the working Arduino implementation disables it.
4. **Input Current Limit:**
   - Register: `0x00`
   - Action: Set to Max (3A) to ensure sufficient power for the AMOLED display.

## Usage
Call `sy6970_init()` at the very beginning of `app_main()` to ensure stable power before initializing the display.
