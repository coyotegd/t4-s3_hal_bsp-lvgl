## HAL Manager as Application Facade

The `hal_mgr` (Hardware Abstraction Layer Manager) acts as a facade, exposing only the subset of driver functionality needed by the application. Instead of calling the display, touch, or PMIC drivers directly, `main.c` and other app code use the HAL interface. This keeps the application code simple and portable, and allows the underlying drivers to be swapped or extended without changing the app logic.

For example, the app in `main.c` only needs to:

- Initialize all hardware
- Set display brightness
- Clear the display
- Poll for touch events

All of these are provided by the HAL manager:

```c
// main.c
#include "hal_mgr.h"

void app_main(void) {
    hal_init();
    hal_set_brightness(128);
    hal_clear_full_display(0x0000); // Black
    while (1) {
        if (hal_handle_touch()) {
            // Use hal_touch_data_t for coordinates/press state
        }
    }
}
```

The HAL manager internally calls the appropriate driver APIs (e.g., `rm690b0_set_brightness`, `cst226se_read`, `sy6970_set_stat_led`) but only exposes what the application actually uses. This design keeps the app logic clean and focused on high-level tasks, not hardware details.
---

## Hardware Abstraction Layer (HAL) Manager (`hal_mgr.h`)

The `hal_mgr` provides a unified interface for display, touch, and power management. It abstracts the underlying drivers, so application code can use a single API regardless of hardware details.

**Key HAL API Functions:**

- `void hal_init(void);`  // Initialize all hardware (display, touch, PMIC)
- `void hal_set_rotation(uint8_t rot);`  // Set display/touch rotation
- `void hal_redraw_screen(void);`
- `void hal_set_brightness(uint8_t brightness);`
- `uint16_t hal_get_display_width(void);`
- `uint16_t hal_get_display_height(void);`
- `void hal_draw_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);`
- `void hal_clear_full_display(uint16_t color);`
- `void hal_cycle_rotation(void);`
- `bool hal_handle_touch(void);`  // Poll and update touch state
- `esp_err_t hal_set_stat_led(bool on);`  // Control PMIC STAT LED

**Touch Data Structure:**
```c
typedef struct {
    uint16_t x;
    uint16_t y;
    bool pressed;
} hal_touch_data_t;
```

---

## Usage Example (main.c)

```c
#include "hal_mgr.h"

void app_main(void) {
    hal_init();
    hal_set_brightness(128);
    hal_clear_full_display(0x0000); // Black
    while (1) {
        if (hal_handle_touch()) {
            // Use hal_touch_data_t for coordinates/press state
        }
    }
}
```

# LilyGo T4-S3 Driver Suite (RM690B0, CST226SE, SY6970) with HAL Manager

This project implements a working ESP-IDF driver suite for the **LilyGo T4-S3** (2.41" AMOLED) development board, including:
- **RM690B0** display driver
- **CST226SE** touch controller driver
- **SY6970** PMIC/charger driver
- **hal_mgr**: Hardware Abstraction Layer manager for unified access

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


---

## Driver APIs

### RM690B0 Display Driver API (`rm690b0.h`)

- `void rm690b0_init(void);`
- `void rm690b0_send_cmd(uint8_t cmd, const uint8_t *data, size_t len);`
- `void rm690b0_send_pixels(const uint8_t *data, size_t len);`
- `void rm690b0_set_window(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);`
- `void rm690b0_set_rotation(rm690b0_rotation_t rot);`
- `uint16_t rm690b0_get_width(void);`
- `uint16_t rm690b0_get_height(void);`
- `void rm690b0_read_id(uint8_t *id);`
- `void rm690b0_set_brightness(uint8_t level);`
- `void rm690b0_sleep_mode(bool sleep);`
- `void rm690b0_display_power(bool on);`
- `void rm690b0_invert_colors(bool invert);`
- `void rm690b0_enable_te(bool enable);`
- `void rm690b0_clear_full_display(uint16_t color);`

### CST226SE Touch Driver API (`cst226se.h`)

- `void cst226se_init(void);`
- `void cst226se_set_i2c_bus(i2c_master_bus_handle_t bus);`
- `void cst226se_set_rotation(cst226se_rotation_t rotation);`
- `void cst226se_set_swap_xy(bool swap);`
- `void cst226se_set_mirror_xy(bool mirror_x, bool mirror_y);`
- `void cst226se_set_max_coordinates(uint16_t x, uint16_t y);`
- `bool cst226se_get_resolution(int16_t *x, int16_t *y);`
- `bool cst226se_read(cst226se_data_t *data);`
- `void cst226se_reset(void);`

### SY6970 PMIC/Charger Driver API (`sy6970.h`)

- `void sy6970_init(void);`
- `i2c_master_bus_handle_t sy6970_get_bus_handle(void);`
- `esp_err_t sy6970_set_input_current_limit(uint16_t current_ma);`
- `esp_err_t sy6970_enable_otg(bool enable);`
- `esp_err_t sy6970_enable_charging(bool enable);`
- `esp_err_t sy6970_set_charge_current(uint16_t current_ma);`
- `esp_err_t sy6970_set_charge_voltage(uint16_t voltage_mv);`
- `esp_err_t sy6970_set_min_system_voltage(uint16_t voltage_mv);`
- `esp_err_t sy6970_enable_hiz_mode(bool enable);`
- `esp_err_t sy6970_disable_batfet(bool disable);`
- `esp_err_t sy6970_reset_watchdog(void);`
- `uint16_t sy6970_get_vbus_voltage(void);`
- `uint16_t sy6970_get_battery_voltage(void);`
- `uint16_t sy6970_get_system_voltage(void);`
- `uint16_t sy6970_get_charge_current(void);`
- `uint8_t sy6970_get_ntc_percentage(void);`
- `sy6970_bus_status_t sy6970_get_bus_status(void);`
- `sy6970_charge_status_t sy6970_get_charge_status(void);`
- `bool sy6970_is_power_good(void);`
- `bool sy6970_is_vbus_connected(void);`
- `esp_err_t sy6970_set_stat_led(bool on);`

---

## Hardware Pin Map

| Signal | GPIO | Notes |
| :--- | :--- | :--- |
| **CS** | 11 | Chip Select |
| **SCK**| 15 | Clock |
| **D0** | 14 | Data 0 |
| **D1** | 10 | Data 1 |
| **D2** | 16 | Data 2 |
| **D3** | 12 | Data 3 |
| **RST**| 13 | Reset |
| **TE** | 18 | Tearing Effect |
| **PMIC_EN** | 9 | **CRITICAL:** Power Enable |
| **I2C_SDA** | 6 | PMIC/Touch I2C |
| **I2C_SCL** | 7 | PMIC/Touch I2C |

1.  Open the project folder in VS Code.
    *   *Note:* The extension automatically handles the environment variables for you. You do **not** need to run `export.sh` manually when using these buttons.

### Option B: Command Line (CLI)
If you prefer the terminal, you **must** load the environment variables first.

1.  **Set up the Environment (Crucial Step):**
    ```bash

### "idf.py: command not found"
If you see this error, it means you haven't run the export script in your current terminal session. See step 1 above.

### VS Code "Red Squiggles" (Undefined Identifiers)
If VS Code shows errors like `CONFIG_LOG_MAXIMUM_LEVEL is undefined` but the project builds fine:
1.  Ensure you delete the first build and rebuild at least once (`idf.py build`).
2.  The `sdkconfig.h` file is generated in `build/config/`.
3.  Check `.vscode/settings.json` and ensure `compile-commands-dir` uses `${workspaceFolder}/build` instead of a hardcoded absolute path from another machine.
