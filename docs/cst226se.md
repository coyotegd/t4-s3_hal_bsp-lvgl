# Hynitron Touchscreen Controller Driver Guide

## Document Information

**Document Version:** V3.5  
**Driver Version:** V2.4  
**App Version:** V1.26  
**Release Date:** July 1, 2022  
**Author:** Steven  

**Company Information:**

- **Shanghai Headquarters:** No. 10, Lane 36, Xuelin Road, Pudong New District, Shanghai
- **Shenzhen Branch:** Building 1B, 8th Floor, Phase 2, High-Tech Industrial Park, Liuxian 1st Road, Bao'an District 67, Shenzhen
- **Website:** <http://hailichuang.cent.uoeee.com/>

---

## Version History

| Version | Date      | Author | Changes                          |
| ------- | --------- | ------ | -------------------------------- |
| V2.0    | 2020/8/13 | Steven | Initial release                  |
| V3.0    | 2021/4/13 | Steven | Added CST9XX support             |
| V3.1    | 2021/4/14 | Steven | Firmware configuration updates   |
| V3.2    | 2021/7/20 | Steven | USB debugging support            |
| V3.3    | 2021/8/4  | Steven | CST918/CST130 fixes              |
| V3.4    | 2021/9/3  | Steven | Boot firmware matching           |
| V3.5    | 2022/7/1  | Steven | CST3240 support, SD card logging |

### Version V2.0 (Initial Release)

1. Organized chip series compatibility, supporting:
   - CST1XX/CST2XX/CST3XX/CST7XX/CST8XX
   - CST1XXSE/CST2XXSE/CST3XXSE
2. APK support for firmware upgrade, version reading, factory testing, raw/diff data reading
3. ESD detection functionality
4. Gesture wake-up functionality
5. Proximity sensing functionality

### Version V3.0

1. Added CST9XX support
2. Factory open/short circuit testing
3. Mandatory DTS matching for RST/INT/resolution

### Version V3.1

1. Modified firmware configuration description

### Version V3.2

1. Modified debug functionality with USB connection debugging support
2. Added factory test function for CST644K

### Version V3.3

1. Fixed CST918/CST130 software reset failure

### Version V3.4

1. Modified boot firmware matching mechanism

### Version V3.5

1. Added CST3240 chip support
2. Added SD card log file saving functionality

---

## Table of Contents

1. [Objective](#1-objective)
2. [Supported Chip Types](#2-supported-chip-types)
3. [File Structure](#3-file-structure)
4. [Compilation Configuration](#4-compilation-configuration)
5. [Driver Function Configuration](#5-driver-function-configuration)
6. [ADB Debug Nodes](#6-adb-debug-nodes)
7. [APK Debugging](#7-apk-debugging)
8. [Gesture Wake-up Function](#8-gesture-wake-up-function)
9. [Proximity Sensing Function](#9-proximity-sensing-function)
10. [Driver Loading Process](#10-driver-loading-process)
11. [Register Description](#11-register-description)

---

## 1. Objective

This document introduces the Hynitron touch chip driver framework architecture, driver configuration and debugging procedures for FAE colleagues and solution companies. It covers:

- Main driver functions
- Driver configuration
- Document structure
- Porting steps

---

## 2. Supported Chip Types

### Compatible Chip Series

| Series       | Supported Models                                  |\n| ------------ | ------------------------------------------------- |\n| **CST1XX**   | CST126, CST128, CST130, CST140, CST14055, CST148 |\n| **CST1XXSE** | CST128SE                                          |\n| **CST2XX**   | CST226, CST237, CST240                            |\n| **CST2XXSE** | CST226SE                                          |\n| **CST3XX**   | CST326, CST328, CST340, CST348                    |\n| **CST7XX**   | CST716, CST726, CST736                            |\n| **CST8XX**   | CST816, CST826, CST836U                           |\n| **CST9XX**   | CST912, CST918                                    |\n| **CST6XX**   | CST6928S                                          |\n| **CST644K**  | CST644K                                           |\n\n### Supported Platforms

- Android platforms: MTK, Qualcomm, Allwinner, Rockchip, Spreadtrum

### Features

- ADB and APK debugging support
- Gesture wake-up
- ESD detection
- Proximity sensing

---

## 3. File Structure

Driver files are stored in the `hynitron` folder, implementing driver mounting, touch point reporting, sleep/wake, gesture wake-up, firmware upgrade, and APK/ADB debugging interfaces.

| File Name                     | Required | Function                                                               |
| ----------------------------- | -------- | ---------------------------------------------------------------------- |
| **Makefile**                  | Yes      | Makefile configuration                                                 |
| **Kconfig**                   | Yes      | Kernel configuration                                                   |
| **xxx_core.c**                | Yes      | Main driver file - mounting, touch data reporting, sleep/wake          |
| **xxx_core.h**                | Yes      | Main header file - info, structure types (must configure per project)  |
| **hynitron_config.h**         | Yes      | Feature module enable/disable configuration                            |
| **hynitron_common.h**         | Yes      | Chip types, register definitions, external declarations, print macros  |
| **hynitron_esd_check.c**      | Yes      | ESD detection functionality                                            |
| **hynitron_gesture.c**        | Optional | Gesture wake-up functionality                                          |
| **hynitron_i2c.c**            | Yes      | I2C communication functions                                            |
| **hynitron_proximity.c**      | Optional | Proximity sensing functionality                                        |
| **hynitron_tool_debug.c**     | Optional | Android sys/proc nodes for ADB and APK debugging (strongly recommended)|
| **hynitron_update_firmware.c* | Yes      | Firmware update functionality                                          |
| **hynitron_update_firmware.h* | Yes      | Firmware update header                                                 |
| **/firmware**                 | Yes      | Firmware files for upgrades                                            |
| **/docs**                     | Optional | DTS configuration documentation                                        |

---

## 4. Compilation Configuration

### 4.1 Modify Compilation Files

1. Copy the `hynitron` folder to: `kernel/drivers/input/touchscreen/`

2. **Modify Kconfig** in the touchscreen directory, add at the end:

   ```text
   source "drivers/input/touchscreen/hynitron/Kconfig"
   ```

3. **Modify Makefile** in the touchscreen directory, add at the end:

   ```makefile
   obj-$(CONFIG_TOUCHSCREEN_HYNITRON_TS) += hynitron/
   ```

   Or:

   ```makefile
   obj-y += hynitron/
   ```

### 4.2 Compilation Commands

1. Open menuconfig and select `TOUCHSCREEN_HYNITRON_TS`
2. Build bootimage:

   ```bash
   make bootimage -j32
   ```

Default compilation includes driver files without additional modifications.

---

## 5. Driver Function Configuration

### 5.1 DTS Configuration

**Example:**

```dts
i2c@f9927000 {
    hynitron@1a {
        compatible = "hynitron,hyn_ts";
        reg = <0x1a>;
        hynitron,reset-gpio = <&gpio 12 0x01>;
        hynitron,irq-gpio = <&gpio 13 0x02>;
        hynitron,max-touch-number = <5>;
        hynitron,display-coords = <1080 1920>;
        hynitron,have-key;
        hynitron,key-number = <3>;
        hynitron,key-code = <139 172 158>;
        hynitron,key-y-coord = <2000 2000 2000>;
        hynitron,key-x-coord = <200 600 800>;
    };
};
```

**DTS Must Include:**

1. **I2C Address** (`reg`) - Default 0x1A (can be changed for special cases)
2. **Compatible String** - Must match driver internal definition
3. **Interrupt GPIO** (`hynitron,irq-gpio`)
4. **Reset GPIO** (`hynitron,reset-gpio`)
5. **Max Touch Points** (`hynitron,max-touch-number`)
6. **Display Resolution** (`hynitron,display-coords`)
7. **Key Information** (if keys are present, must be configured)

**Notes:**

- All parameters except key information are mandatory, otherwise DTS parsing will fail
- If DTS cannot be modified, manually modify `hyn_parse_dt()` function with direct assignments
- DTS needs platform-specific adjustments (see `/docs/hynitron-ts.txt`)
- Mutual capacitance chips use 7-bit I2C address (default 0x1A or 0x5A), with 16-bit register addresses

### 5.2 Configure Function Modules and Project Info

#### 5.2.1 Configure hynitron_core.h (Project Information)

**Project Configuration - Select IC type and project ID:**

| Parameter           | Description                                              | Example |
| ------------------- | -------------------------------------------------------- | ------- |
| **CHIP_TYPE**       | Chip type (REQUIRED - incorrect type causes failure)     | CST340  |
| **TRIGGER_RISING**  | Rising edge trigger (optional, default falling edge)     | 0x00    |
| **X_DISPLAY**       | Default X resolution                                     | 720     |
| **Y_DISPLAY**       | Default Y resolution                                     | 1280    |
| **X_REVERT**        | Reverse X coordinate direction                           | 0       |
| **Y_REVERT**        | Reverse Y coordinate direction                           | 0       |
| **XY_EXCHANGE**     | Swap X and Y                                             | 0       |
| **MAX_KEYS**        | Number of keys                                           | 3       |
| **MAX_POINTS**      | Max touch points (self-capacitance needs 2)              | 5       |

**Other Configuration Macros:**

| Macro                                 | Function                                                      | Default |
| ------------------------------------- | ------------------------------------------------------------- | ------- |
| `HYN_RESET_SOFTWARE`                  | Software watchdog reset (enable if no reset pin)              | Disable |
| `HYN_UPDATE_FIRMWARE_POWERON_ENABLE`  | Power-off reset upgrade                                       | Disable |
| `HYN_UPDATE_FIRMWARE_ENABLE`          | Firmware upgrade function                                     | Disable |
| `HYN_UPDATE_FIRMWARE_FORCE`           | Force upgrade (no project ID/version check - use carefully)   | Disable |
| `HYN_IIC_TRANSFER_LIMIT`              | I2C byte length limit (some MTK platforms require this)       | Disable |

#### 5.2.2 Configure hynitron_config.h (Feature Modules)

**Feature Module Configuration:**

| Macro                         | Function                                                   | Default |
| ----------------------------- | ---------------------------------------------------------- | ------- |
| `HYN_DEBUG_EN`                | Debug log printing (disable in user builds)                | Enable  |
| `HYN_MT_PROTOCOL_B_EN`        | Linux multi-touch protocol (enable=B, disable=A)           | Enable  |
| `HYN_REPORT_PRESSURE_EN`      | Report pressure value in multi-touch A/B                   | Enable  |
| `HYN_GESTURE_EN`              | Gesture wake-up function                                   | Disable |
| `HYN_PSENSOR_EN`              | Proximity sensing                                          | Disable |
| `HYN_ESDCHECK_EN`             | ESD protection (checks every 1s, resets on error)          | Enable  |
| `HYN_AUTO_FACTORY_TEST_EN`    | Boot factory test verification (TP consistency check)      | Disable |
| `HYN_EN_AUTO_UPDATE`          | Automatic firmware upgrade                                 | Disable |
| `HYN_SYS_AUTO_SEARCH_FIRMWARE`| Automatic firmware search and upgrade                      | Disable |
| `ANDROID_TOOL_SUPPORT`        | Android proc node generation for APK debugging             | Enable  |
| `HYN_SYSFS_NODE_EN`           | Android sys node generation for ADB debugging              | Enable  |

**Firmware Upgrade Support by Chip Series:**

| Macro                            | Chip Series                                    | Default |
| -------------------------------- | ---------------------------------------------- | ------- |
| `HYN_EN_AUTO_UPDATE_CST0xxSE`    | CST016SE/CST026SE/CST036SE                     | Disable |
| `HYN_EN_AUTO_UPDATE_CST0xx`      | CST016/CST026/CST036                           | Disable |
| `HYN_EN_AUTO_UPDATE_CST1xx`      | CST126/CST130/CST140/CST14055/CST148           | Disable |
| `HYN_EN_AUTO_UPDATE_CST1xxSE`    | CST128SE/CST18858SE/CST18868SE                 | Disable |
| `HYN_EN_AUTO_UPDATE_CST2xx`      | CST226/CST237/CST240                           | Disable |
| `HYN_EN_AUTO_UPDATE_CST2xxSE`    | CST226SE                                       | Disable |
| `HYN_EN_AUTO_UPDATE_CST3xx`      | CST326/CST328/CST340/CST348                    | Disable |
| `HYN_EN_AUTO_UPDATE_CST3xxSE`    | CST328SE                                       | Disable |
| `HYN_EN_AUTO_UPDATE_CST78xx`     | CST716/CST726/CST736/CST816/CST826/CST836U     | Disable |
| `HYN_EN_AUTO_UPDATE_CST6xx`      | CST6928S                                       | Disable |
| `HYN_EN_AUTO_UPDATE_CST9xx`      | CST912/CST918                                  | Disable |
| `HYN_EN_AUTO_UPDATE_CST644K`     | CST644K                                        | Disable |

**Note on Bootloader Mode Entry:**

If unable to enter bootloader mode during upgrade, verify reset method:

1. Power-off reset
2. Reset pin reset
3. Watchdog reset

Bootloader mode timing window: Send command within 5ms~20ms after chip reset.

#### 5.2.3 Configure Firmware Information (NEW PROJECTS MUST MODIFY)

**Configure IC Type:**

```c
#define HYN_CHIP_TYPE_CONFIG CST340  // in hynitron_core.h
```

**Configure Firmware (hynitron_update_firmware.c):**

Must modify the following:

1. Replace or add corresponding chip's `.h` file
2. Modify `hynitron_fw_grp[]` array with firmware name, project ID, module ID, chip type
3. For projects with multiple TP vendors, add corresponding header files identified by project ID and module ID

### 5.3 Firmware Version Information

**Mutual Capacitance Chip Series - Firmware Version Reading Function:** `cst3xx_firmware_info()`

**Steps:**

1. Enter debug info mode: Write 0xD1 = 0x01
2. Read register 0xD1FC checkcode to determine if chip is blank
3. If firmware exists, continue reading chip type/project ID/version info/checksum
4. Exit debug info mode, return to normal mode: Write 0xD1 = 0x09

### 5.4 Touch Function Debugging

**Mutual Capacitance Chip Series - Touch Function Debugging:**

1. Touch input device/workqueue registration, supports A/B protocol: `hyn_input_dev_int()`
2. Interrupt service function registration, supports rising/falling edge: `hyn_irq_init()` / `hyn_eint_interrupt_handler()`
3. When touch interrupt occurs, interrupt service responds and enters report function: `cst3xx_touch_report()`
4. Report function parses touch protocol

**Touch Information Parsing Example:**

- I2C Address: 0x1A (Read mode)

### 5.5 Firmware Upgrade Function Debugging

**Firmware Upgrade Process:**

1. Read chip firmware version information at driver probe
2. Compare with version in driver array
3. If versions differ, initiate upgrade process
4. Enter bootloader mode
5. Erase flash and program new firmware
6. Verify checksum
7. Reset chip to apply new firmware

### 5.6 Configure Factory Testing (Optional)

Factory testing can verify touch panel consistency during manufacturing.

#### 5.6.1 Install Test APK

Install the Hynitron factory test APK on the Android device.

#### 5.6.2 Modify SELinux Permissions

Ensure proper permissions for proc nodes:

```bash
chmod 666 /proc/hynitron_debug
```

#### 5.6.3 Configure APK Factory Test Parameters

Configure test parameters in the APK according to panel specifications:

- Raw data limits
- Diff data limits
- Open/short circuit thresholds

---

## 6. ADB Debug Nodes

**Available Debug Nodes:**

| Node Path                                     | Function                | Access |
| --------------------------------------------- | ----------------------- | ------ |
| `/proc/hynitron_debug`                        | Main debug interface    | R/W    |
| `/sys/class/input/inputX/device/fw_version`   | Firmware version        | R      |
| `/sys/class/input/inputX/device/chip_info`    | Chip information        | R      |
| `/sys/class/input/inputX/device/fw_update`    | Firmware update trigger | W      |
| `/sys/class/input/inputX/device/factory_test` | Factory test trigger    | R/W    |
| `/sys/class/input/inputX/device/rawdata`      | Raw data reading        | R      |
| `/sys/class/input/inputX/device/diffdata`     | Diff data reading       | R      |

**Common ADB Commands:**

```bash
# Read firmware version
cat /sys/class/input/input0/device/fw_version

# Read chip info
cat /sys/class/input/input0/device/chip_info

# Trigger firmware update
echo 1 > /sys/class/input/input0/device/fw_update

# Run factory test
cat /sys/class/input/input0/device/factory_test

# Read raw data
cat /sys/class/input/input0/device/rawdata

# Read diff data
cat /sys/class/input/input0/device/diffdata
```

---

## 7. APK Debugging

The Hynitron APK provides a graphical interface for debugging and testing:

### 7.1 Data Saving

- Raw data can be saved to SD card for analysis
- Log files automatically saved to `/sdcard/hynitron_log/`
- Supports CSV export for external analysis tools

### 7.2 Rawdata/Diff Mutual Capacitance Interface

The APK displays a grid showing:

- **Raw Data:** Base capacitance values for each sensing point
- **Diff Data:** Difference between current and baseline values
- Color-coded display for easy identification of anomalies
- Real-time refresh during touch

**Typical Values:**

- Good touch: Diff data 100-1000 units above baseline
- No touch: Raw data stable, diff data near 0
- Defects: Abnormal values in specific cells

### 7.3 APK Firmware Update

**Update Process:**

1. Copy firmware binary (.bin) to device storage
2. Open Hynitron APK
3. Navigate to "Firmware Update" section
4. Select firmware file
5. Click "Update" button
6. Wait for completion (device may reboot)

**Safety Features:**

- Firmware version verification
- Project ID matching
- Checksum validation
- Automatic rollback on failure

---

## 8. Gesture Wake-up Function

### 8.1 Gesture Initialization

**Enable gesture support in configuration:**

```c
#define HYN_GESTURE_EN 1  // in hynitron_config.h
```

**Supported Gestures:**

- Double tap
- Slide up/down/left/right
- Draw letters: C, E, M, O, S, V, W, Z
- Custom gestures (chip-dependent)

**Initialization Function:**

```c
int hyn_gesture_init(struct input_dev *input_dev);
```

### 8.2 Gesture Reporting

**Gesture Detection Process:**

1. System enters suspend mode
2. Touch controller enters low-power gesture mode
3. User performs gesture on screen
4. Controller detects gesture pattern
5. Wakes system and reports gesture event
6. Application can handle gesture-specific actions

**Gesture Event Codes:**

| Gesture     | Event Code | Value |
| ----------- | ---------- | ----- |
| Double Tap  | KEY_WAKEUP | 0x100 |
| Swipe Up    | KEY_UP     | 0x67  |
| Swipe Down  | KEY_DOWN   | 0x6C  |
| Swipe Left  | KEY_LEFT   | 0x69  |
| Swipe Right | KEY_RIGHT  | 0x6A  |
| Letter C    | KEY_C      | 0x2E  |
| Letter E    | KEY_E      | 0x12  |
| Letter M    | KEY_M      | 0x32  |
| Letter O    | KEY_O      | 0x18  |
| Letter W    | KEY_W      | 0x11  |

---

## 9. Proximity Sensing Function

### 9.1 Proximity Sensing Initialization

**Enable proximity sensing:**

```c
#define HYN_PSENSOR_EN 1  // in hynitron_config.h
```

**Hardware Requirements:**

- Touch panel must support proximity detection
- Firmware must include proximity algorithm
- System must have proximity sensor framework

**Initialization:**

```c
int hyn_proximity_init(struct hyn_ts_data *ts_data);
```

### 9.2 Proximity Sensing Reporting

**Detection Process:**

1. Touch controller monitors sensing channels
2. Detects large objects (hand, face) approaching screen
3. Reports proximity state change
4. System can disable touch during calls to prevent false touches

**Proximity States:**

- **NEAR (0):** Object detected near screen (< 5cm typical)
- **FAR (1):** No object detected

**Use Cases:**

- **Phone Calls:** Disable screen when phone is near face
- **Pocket Mode:** Prevent pocket touches
- **Power Saving:** Reduce scanning rate when object nearby

**Reporting Function:**

```c
void hyn_proximity_report(struct hyn_ts_data *ts_data, int state);
```

---

## 10. Driver Loading Process

### 10.1 Driver Entry Function

```c
module_init(hyn_driver_init);
module_exit(hyn_driver_exit);
```

**Registration:**

- Platform driver registration
- I2C driver registration
- Character device registration (for debug nodes)

### 10.2 Load I2C Driver

**I2C Driver Structure:**

```c
static struct i2c_driver hyn_ts_driver = {
    .driver = {
        .name = HYN_DRIVER_NAME,
        .owner = THIS_MODULE,
        .of_match_table = of_match_ptr(hyn_dt_match),
    },
    .probe = hyn_ts_probe,
    .remove = hyn_ts_remove,
    .id_table = hyn_ts_id,
};
```

### 10.3 Execute Probe Function

**Probe Sequence:**

1. Parse DTS configuration
2. Allocate driver data structures
3. Request GPIO resources (reset, interrupt)
4. Initialize I2C communication
5. Power on and reset touch controller
6. Read chip ID and firmware version
7. Initialize input device
8. Register interrupt handler
9. Create debug nodes
10. Start ESD check timer (if enabled)
11. Perform firmware upgrade (if needed)

### 10.4 Touch Information Reporting

**Interrupt Handler Flow:**

```text
Interrupt Triggered
    ↓
Read Touch Data from I2C
    ↓
Parse Touch Protocol
    ↓
Extract Touch Points (X, Y, Pressure, ID)
    ↓
Report via Input Subsystem
    ↓
(Protocol A: Direct report)
(Protocol B: Slot-based report)
    ↓
input_sync()
```

**Multi-Touch Protocol B (Recommended):**

```c
for (i = 0; i < max_points; i++) {
    input_mt_slot(input_dev, i);
    if (point_valid[i]) {
        input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, true);
        input_report_abs(input_dev, ABS_MT_POSITION_X, x[i]);
        input_report_abs(input_dev, ABS_MT_POSITION_Y, y[i]);
        input_report_abs(input_dev, ABS_MT_PRESSURE, pressure[i]);
    } else {
        input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, false);
    }
}
input_sync(input_dev);
```

---

## 11. Register Description

### 11.1 Mutual Capacitance Product Registers

#### 11.1.1 Touch Information Registers (ENUM_MODE_NORMAL Mode)

| Address | Name | Description | Access |
| ------- | ---- | ----------- | ------ |
| 0x0000 | STATUS | Touch status byte | R |
| 0x0001 | POINT_NUM | Number of touch points | R |
| 0x0002 | TOUCH_DATA | Touch coordinate data | R |

**Touch Data Format (10 bytes per point):**

```text
Byte 0: Point ID and event flag
Byte 1-2: X coordinate (16-bit, little endian)
Byte 3-4: Y coordinate (16-bit, little endian)
Byte 5-6: Pressure value
Byte 7-8: Area value
Byte 9: Reserved
```

**Example Read Sequence:**

```c
// Read touch status
i2c_read(0x0000, &status, 1);

// Read number of points
i2c_read(0x0001, &point_num, 1);

// Read touch data
i2c_read(0x0002, touch_buffer, point_num * 10);
```

#### 11.1.2 Version Information Registers (ENUM_MODE_DEBUG_INFO Mode)

To read version information:

1. Write 0x01 to 0xD1 (enter debug info mode)
2. Read version registers
3. Write 0x09 to 0xD1 (return to normal mode)

| Address | Name | Description | Size |
| ------- | ---- | ----------- | ---- |
| 0xD1FC | CHECKCODE | Firmware check code | 2 bytes |
| 0xD1FE | BOOT_STATE | Boot state | 1 byte |
| 0xD204 | CHIP_TYPE | Chip type identifier | 4 bytes |
| 0xD208 | PROJECT_ID | Project ID | 4 bytes |
| 0xD20C | FW_VERSION | Firmware version | 4 bytes |
| 0xD210 | CHECKSUM | Firmware checksum | 2 bytes |

#### 11.1.3 Mode Command Registers

| Address | Command | Function |
| ------- | ------- | -------- |
| 0xD1 | 0x01 | Enter Debug Info Mode |
| 0xD1 | 0x02 | Enter Factory Test Mode |
| 0xD1 | 0x03 | Enter Gesture Mode |
| 0xD1 | 0x09 | Return to Normal Mode |
| 0xD1 | 0x0A | Enter Sleep Mode |
| 0xD1 | 0x0B | Software Reset |

### 11.2 Self-Capacitance Product Registers

#### 11.2.1 Working Mode Switch Commands

| Command | Function |
| ------- | -------- |
| Write 0xAA to 0xED | Enter Factory Test Mode |
| Write 0xCC to 0xED | Exit Factory Test Mode |
| Write 0xD1 to 0xEE | Enter Sleep Mode |
| Write 0xD2 to 0xEE | Exit Sleep Mode |

#### 11.2.2 NORMAL Register Description

| Address | Name | Description | Access |
| ------- | ---- | ----------- | ------ |
| 0x00 | GESTURE_ID | Gesture ID (gesture mode) | R |
| 0x01 | FINGER_NUM | Number of fingers | R |
| 0x02 | XPOS_H | X position high byte | R |
| 0x03 | XPOS_L | X position low byte | R |
| 0x04 | YPOS_H | Y position high byte | R |
| 0x05 | YPOS_L | Y position low byte | R |
| 0xA7 | CHIP_ID | Chip ID | R |
| 0xA8 | FW_VERSION | Firmware version | R |
| 0xA9 | VENDOR_ID | Vendor ID | R |

---

## Summary

This driver guide provides comprehensive information for integrating Hynitron touchscreen controllers into Android systems. Key points:

- **Flexible Architecture:** Supports multiple chip series with common driver framework
- **Feature-Rich:** Gesture wake-up, proximity sensing, ESD protection, firmware updates
- **Easy Configuration:** DTS-based configuration with header file options
- **Debugging Tools:** APK and ADB interfaces for factory testing and field debugging
- **Production Ready:** Factory test support for quality control

For additional support, contact Hynitron FAE team or refer to chip-specific datasheets.
