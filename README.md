# LilyGo T4-S3 ESP-IDF Starter with BSP-LVGL & HAL

This project is a complete (except Bluetooth implementation), working starter template for the **LilyGo T4-S3** (2.41" AMOLED) development board using **ESP-IDF** and **LVGL**.

It features a robust **Hardware Abstraction Layer (HAL)** that handles the complex low-level drivers for the display, touch screen, and power management IC (PMIC), allowing you to focus on building your application.

## 📸 Gallery

![UI Home Screen](docs/images/ui_home.png)
![UI Home Screen](docs/images/ui_media.png)
![UI Home Screen](docs/images/ui_pm_set.png)
## ✨ Features

*   **Display:** RM690B0 Driver (AMOLED 450x600 via QSPI/SPI-like protocol).
*   **Touch:** CST226SE Driver (Capacitive Touch).
*   **Power:** SY6970 PMIC Driver (Battery charging, voltage monitoring, power path).
*   **HAL Manager:** A unified facade (`hal_mgr`) that simplifies hardware usage.
*   **LVGL Integration:** Pre-configured LVGL 9 display and touch drivers.
*   **Battery Logic:** Smart detection for "No Battery" vs "Charging" states.
*   **Development Tools:** ESP-IDF helper script (`tools/idfsh.sh`) for interactive build/flash/monitor workflows.

## 🚀 Getting Started

### Prerequisites
*   VS Code with Espressif IDF Extension.
*   ESP-IDF v5.x.

### Build & Run
1.  **Open** this folder in VS Code.
2.  **Build** the project: Click the `Build` button in the status bar or run `idf.py build`.
3.  **Flash** to device: Click `Flash` or run `idf.py -p /dev/ttyACM0 flash` (check your port name).
4.  **Monitor** output: Click `Monitor` or run `idf.py monitor`.

## 🛠 Hardware Abstraction Layer (HAL)

The `hal_mgr` acts as a facade. Instead of interacting with the `rm690b0` or `sy6970` drivers directly, your application uses `hal_mgr`.

```c
// main.c example
#include "hal_mgr.h"

void app_main(void) {
    // 1. Initialize all hardware (Screen, Touch, Power)
    hal_mgr_init();

    // 2. Register callbacks for events (Charge status, USB insert, etc.)
    hal_mgr_register_charge_callback(my_charge_handler, NULL);

    // 3. Your app continues...
}
```

## 🌐 WiFi & Auto-Timezone

The project includes a robust WiFi Manager (`wifi_mgr`) that handles:
*   **Scanning & Connection:** Scans for available networks and manages connection state.
*   **SNTP Time Sync:** Automatically fetches UTC time from `pool.ntp.org` upon connection.
*   **Auto-Timezone Detection:**
    *   After obtaining an IP, the device queries `http://ip-api.com` to determine your location.
    *   It parses the specific time zone offset from the JSON response.
    *   System `TZ` environment variable is automatically updated to match your local time (e.g., PST/PDT).
    *   The UI displays "http d/t requested . . ." until the valid local time is resolved.

### 🕒 Timekeeping Summary
*   **Second-by-second:** Handled by the ESP32's internal software counter (System Time).
*   **Accuracy check:** Updated from the internet via SNTP every **60 minutes**.
*   **Power Loss:** Since there is no dedicated coin-cell RTC, if power is lost completely, time resets until WiFi reconnects.

## 🔌 USB Type-C & Power Delivery Configuration

The PM Settings page includes advanced USB Type-C configuration options for managing power delivery and USB protocol versions.

### USB Type-C Voltage Selector

Three voltage modes are available for Type-C power delivery:

*   **Auto-PD (Negotiated)** - *Default and Recommended*
    *   Allows the device to automatically negotiate the optimal voltage with the connected USB-C power adapter
    *   USB Power Delivery (USB-PD) protocol handles this transparently
    *   The charger and device communicate to select the best voltage/current combination
    *   Most compatible with modern USB-C adapters
    
*   **5V Override**
    *   Forces operation at standard USB 5V (USB 2.0/3.0 default)
    *   Use when connected to older USB ports or basic adapters
    *   Safest option for unknown power sources
    
*   **9V Override**
    *   Requests 9V operation from the power adapter via USB-PD
    *   Enables faster charging if the adapter supports 9V output
    *   Only effective with USB-PD compatible adapters (most modern USB-C chargers)

**Note:** USB Power Delivery is automatic by default. The override options are provided for specific use cases or troubleshooting. Setting an override tells the system your preference, but the actual voltage delivered depends on what the power adapter can provide.

### USB Version Selector

Selects the USB data transfer protocol standard:

*   **USB 2.0** - Up to 480 Mbps (backward compatible, lower power)
*   **USB 3.0** - Up to 5 Gbps (faster data transfer)
*   **USB 4** - Up to 40 Gbps (latest standard, maximum performance)

**Important:** USB version and USB-PD voltage are independent:
*   USB 4 can work with any voltage (5V, 9V, 12V, 15V, or 20V)
*   They operate over the same USB Type-C connector
*   USB version controls **data speed**, USB-PD controls **charging voltage/current**
*   You can select "USB 4 + PD 9V" for maximum data and charging performance

### Current Implementation Status

**🔴 Hardware Implementation Required:**

The USB Type-C voltage and version selectors are currently **UI-only settings**. They:
*   Display in the PM Settings interface
*   Save your preferences to non-volatile storage (NVS)
*   Log your selections to the ESP console

**However, actual voltage negotiation requires:**
1.  **Hardware Support:** A USB-PD controller chip (many modern Type-C adapters have this, but the ESP32-S3 doesn't natively control it)
2.  **Driver Implementation:** Code in the `sy6970` driver to communicate with USB-PD negotiation hardware
3.  **Protocol Logic:** Implementation of the USB-PD protocol state machine

**Current Behavior:**
*   The SY6970 PMIC accepts whatever voltage the power adapter provides (5V-20V)
*   It will charge the battery safely regardless of input voltage
*   The UI settings prepare the framework for future USB-PD control implementation

These settings are included now so you can:
*   Document your power adapter capabilities
*   Prepare for future hardware integration
*   Understand the relationship between USB-PD and USB data standards

## 🎥 AVI Video Playback

The system includes a video player for `.avi` files stored on the SD card.

*   **Frame Rate:** Video playback is optimized for **~15 FPS**. Increasing the frame rate beyond this provides no visual benefit on this screen/interface and only consumes extra resources.
*   **Codec:** The player uses an **older MPEG codec**, not the latest standards (like H.264). Please ensure video files are encoded using compatible legacy MPEG formats.

## � Over-The-Air (OTA) Updates

The system supports wireless firmware updates via the **System OTA** menu.

*   **Logic:**
    *   Checks a remote GitHub Release URL for the latest `firmware.bin`.
    *   Parses the **Version** string (e.g., `v1.2.0`) from the new binary header.
    *   **Anti-Downgrade:** Only updates if the remote version is strictly *higher* than the current running version.
    *   **Up-To-Date:** If version is same or older, displays "System is Up To Date".
*   **Partitioning:** Uses an A/B partition scheme (`ota_0`, `ota_1`) with an `otadata` manager to switch safe slots automatically.
*   **Safety:** Automatically verifies image header before writing and reboots upon success.

## �📂 Project Structure

## 🧩 Hardware Details & Pin Map

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
| **PMIC_EN** | 9 | **CRITICAL:** Power Enable (Must be HIGH) |
| **I2C_SDA** | 6 | PMIC/Touch I2C |
| **I2C_SCL** | 7 | PMIC/Touch I2C |

## 💡 The Technical "Struggle" (Solved)

This project solves several tricky hardware behaviors of the T4-S3:

1.  **Missing D/C Pin (RM690B0):** The display uses a custom QSPI wrapper protocol instead of standard SPI/8080.
2.  **GPIO 9 Power Enable:** The display and PMIC power rail is controlled by GPIO 9. It must be pulled HIGH or the screen stays black.
3.  **PMIC Watchdog:** The SY6970 watchdog is disabled on boot to prevent random resets.
4.  **No Battery Detection:** Uses a voltage volatility algorithm to detect if the device is running solely on USB (voltage fluctuates) vs Battery (voltage stable).

## 📚 Documentation
*   [LVGL Integration Journey](docs/LVGL_JOURNEY.md)
*   [RM690B0 Rotation Guide](docs/rm690b0_rotation_guide.md)

## 🔧 Troubleshooting: Hardcoded Paths in Cloned Repos

If cloning this or similar ESP‑IDF projects fails to build or flash due to paths/ports, it’s usually because workspace settings include user‑specific absolute paths.

**Symptoms**
- Build refers to another user’s `esp-idf` install.
- Flash/monitor tries a non‑existent serial port (e.g., `/dev/ttyACM0`).
- Language server (`clangd`) errors about missing toolchain binaries.

**Quick Fix**
- Move aside their workspace settings: `mv .vscode .vscode.bak`
- Reconfigure the project:

```bash
idf.py fullclean
idf.py set-target esp32s3
idf.py reconfigure
idf.py build
```

- Select a valid port when flashing:

```bash
idf.py -p /dev/ttyUSB0 flash monitor
```

**Recommended Repo Practices**
- Avoid committing user‑specific `.vscode/settings.json` entries:
    - `idf.espIdfPath`, `idf.toolsPath`, `idf.port`, `clangd.path`
- Prefer workspace‑relative paths:
    - `clangd.arguments: --compile-commands-dir=${workspaceFolder}/build`
- Track `sdkconfig.defaults`, ignore `sdkconfig` to let each machine generate its own.

**First‑Time Setup**
- Ensure ESP‑IDF tools are installed via the VS Code extension or by sourcing your local IDF (`. $IDF_PATH/export.sh`).
- Initialize submodules:

```bash
git submodule update --init --recursive
```

---

### 🎯 Updated Portable Settings (February 2026)

**This repository now uses fully portable workspace settings!** All user-specific paths have been removed from `.vscode/settings.json`.

**What's included (workspace-portable):**
```json
{
  "idf.buildPath": "${workspaceFolder}/build",
  "clangd.arguments": ["--compile-commands-dir=${workspaceFolder}/build"],
  "idf.customExtraVars": { "IDF_TARGET": "esp32s3" }
}
```

**What's auto-configured by ESP-IDF extension:**
- ESP-IDF installation path
- Toolchain paths  
- Python environment
- Serial port (select when flashing)
- Clangd path

**After cloning:**
1. Open in VS Code
2. Reload window (`Ctrl+Shift+P` → "Reload Window")
3. Extension auto-configures for your system
4. Build and flash!
