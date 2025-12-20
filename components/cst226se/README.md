# CST226SE Touch Controller Driver

ESP-IDF driver for the CST226SE capacitive touch controller, ported from the LilyGo AMOLED Series C++ library.

## Overview

The CST226SE is a multi-touch capacitive touch controller that communicates over I2C. Unlike the more common CST816S, it uses a **packed 12-bit coordinate format** and requires a special initialization sequence.

## Key Features

- ✅ Multi-touch support (up to 5 points)
- ✅ 12-bit coordinate precision
- ✅ Four rotation modes with automatic coordinate transformation
- ✅ Home button detection
- ✅ Shared I2C bus compatible (PMIC + Touch)

## Critical Implementation Details

### 1. Packed 12-bit Coordinate Format

**DO NOT** parse coordinates as standard 16-bit values. The CST226SE uses a special packed format:

```c
// Correct parsing (from 28-byte buffer):
uint16_t x = (buffer[1] << 4) | ((buffer[3] >> 4) & 0x0F);
uint16_t y = (buffer[2] << 4) | (buffer[3] & 0x0F);
```

**Why this matters:** Using standard 16-bit parsing (like CST816S) results in wildly incorrect, "slanted" coordinates that no amount of calibration can fix.

### 2. Special Initialization Sequence

The chip requires entering "Command Mode" to configure properly:

```c
// 1. Enter Command Mode
i2c_write({0xD1, 0x01});
delay(10ms);

// 2. Read Resolution (optional but recommended)
i2c_write_read({0xD1, 0xF8}, buffer, 4);
uint16_t res_x = (buffer[1] << 8) | buffer[0];  // Typically 450
uint16_t res_y = (buffer[3] << 8) | buffer[2];  // Typically 600

// 3. Exit Command Mode
i2c_write({0xD1, 0x09});
```

### 3. Coordinate Transformation for Rotation

The touch chip reports coordinates in its **native orientation** (landscape, 450×600). To match the display rotation, apply transformations:

| Rotation | Display Size | SwapXY | MirrorX | MirrorY |
|----------|--------------|--------|---------|---------|
| 0 (Portrait) | 600×450 | ✓ | ✗ | ✓ |
| 1 (Landscape) | 450×600 | ✗ | ✗ | ✗ |
| 2 (Portrait Inv.) | 600×450 | ✓ | ✓ | ✗ |
| 3 (Landscape Inv.) | 450×600 | ✗ | ✓ | ✓ |

**Implementation:**
```c
// After reading raw coordinates:
if (swap_xy) {
    int16_t tmp = x;
    x = y;
    y = tmp;
}
if (mirror_x) x = max_x - x;
if (mirror_y) y = max_y - y;
```

### 4. Protocol Validation

The chip includes validation bytes to detect invalid reads:

```c
// From 28-byte read buffer:
if (buffer[6] != 0xAB) return ERROR;  // Validation marker
if (buffer[0] == 0xAB) return ERROR;  // Invalid state
if (buffer[5] == 0x80) return ERROR;  // Checksum error
```

### 5. Home Button Detection

Some variants support a capacitive home button:

```c
if (buffer[0] == 0x83 && buffer[1] == 0x17 && buffer[5] == 0x80) {
    // Home button pressed
}
```

## I2C Configuration

- **Address:** `0x5A` (7-bit)
- **Speed:** 400 kHz (Fast Mode)
- **Shared Bus:** Yes (with SY6970 PMIC at 0x6A)

## GPIO Pins (T4-S3)

| Pin | GPIO | Function |
|-----|------|----------|
| RST | 8 | Touch Reset (Active Low) |
| INT | 17 | Interrupt (optional, falling edge) |
| SDA | 6 | I2C Data |
| SCL | 7 | I2C Clock |

## API Usage

### Initialization

```c
#include "cst226se.h"

// Initialize (shares I2C bus with PMIC)
sy6970_init();        // Init PMIC first
cst226se_init();      // Then touch
cst226se_set_rotation(CST226SE_ROTATION_0);
```

### Reading Touch Data

```c
cst226se_data_t touch;

if (cst226se_read(&touch)) {
    if (touch.pressed) {
        printf("Touch at: %d, %d\n", touch.x, touch.y);
    } else {
        printf("Touch released\n");
    }
}
```

### Rotation Control

```c
// Cycle through rotations
cst226se_set_rotation(CST226SE_ROTATION_90);  // 0, 90, 180, 270
```

## Common Pitfalls

### ❌ Wrong: Standard CST816S Parsing
```c
// This produces "slanted" coordinates:
uint16_t x = (buffer[1] << 8) | buffer[2];  // WRONG!
uint16_t y = (buffer[3] << 8) | buffer[4];  // WRONG!
```

### ❌ Wrong: Skipping Command Mode Init
```c
// Chip may not report data correctly:
cst226se_init();  // Missing command mode sequence
```

### ❌ Wrong: Ignoring Rotation Transforms
```c
// Coordinates won't match display orientation:
display_set_rotation(1);
// Need: cst226se_set_rotation(1);
```

## Troubleshooting

| Issue | Cause | Solution |
|-------|-------|----------|
| Coordinates are "slanted" | Using 16-bit parsing | Use packed 12-bit format |
| Touch inverted on X/Y | Wrong mirror flags | Check rotation transform table |
| No touch detected | Shared I2C bus conflict | Init PMIC before touch |
| Coordinates don't match display | Missing rotation sync | Call `cst226se_set_rotation()` |
| Random coordinates | Invalid data read | Check validation bytes (buffer[6] == 0xAB) |

## References

- **LilyGo Library:** [TouchClassCST226.cpp](https://github.com/Xinyuan-LilyGO/LilyGo-AMOLED-Series)
- **Datasheet:** Limited public documentation available
- **I2C Address:** 0x5A (7-bit addressing)

## License

Ported from LilyGo-AMOLED-Series (MIT License)
