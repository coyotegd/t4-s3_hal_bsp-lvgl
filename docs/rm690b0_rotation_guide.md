# RM690B0 Rotation & Configuration Guide

## Overview
This document details the configuration required to achieve artifact-free rotation on the RM690B0 AMOLED display driver (QSPI interface). 

During development, we encountered significant graphical artifacts when rotating the display to Landscape modes (90° and 270°). These included:
1.  **Skew/Italics Effect**: The image appeared slanted or wrapped incorrectly.
2.  **Redraw Lines**: Single-pixel lines appearing on button release or partial redraws.

## The Solution: "Symmetric Even Resolution"
The root cause was identified as an alignment issue within the display controller's GRAM access when the `MV` (Row/Column Exchange) bit is set in the `MADCTL` register.

To prevent these artifacts, the Landscape modes **must** use:
1.  **Even Resolutions**: Both width and height must be even numbers.
2.  **Even Offsets**: The X and Y offsets must be even numbers to ensure proper byte alignment during QSPI transfers.

## Final Configuration
Below are the validated settings for all four rotation angles.

### Portrait Modes (0° & 180°)
*These modes are less sensitive to alignment and work with the native odd-width resolution.*

| Parameter | Value | Notes |
| :--- | :--- | :--- |
| **Resolution** | **459 x 594** | Native visible area |
| **Offset X** | **14** | Centers the image horizontally |
| **Offset Y** | **4** | Centers the image vertically |
| **MADCTL (0°)** | `0x00` | Default |
| **MADCTL (180°)** | `MX | MY` | Inverted |

### Landscape Modes (90° & 270°)
*These modes require strict even-number alignment.*

| Parameter | Value | Notes |
| :--- | :--- | :--- |
| **Resolution** | **600 x 460** | **Must be even**. (Native is 594x459, adjusted to nearest even) |
| **Offset X** | **20** | **Must be even**. |
| **Offset Y** | **12** | **Must be even**. (Fixed redraw artifacts) |
| **MADCTL (90°)** | `MV | MY` | Row/Col Exchange + Y-Mirror |
| **MADCTL (270°)** | `MV | MX` | Row/Col Exchange + X-Mirror |

## Code Implementation
*Reference: `components/rm690b0/rm690b0.c`*

```c
void rm690b0_set_rotation(rm690b0_rotation_t rot) {
    // CRITICAL: Use even numbers for resolution and offsets
    switch (rot) {
        case RM690B0_ROTATION_0:
        case RM690B0_ROTATION_180:
            current_width = 450;
            current_height = 600;
            offset_x = 14;
            offset_y = 4;
            break;

        case RM690B0_ROTATION_90:
        case RM690B0_ROTATION_270:
            current_width = 600;
            current_height = 450;
            offset_x = 20;
            offset_y = 12; 
            break;
    }

}
```
## Troubleshooting History
*   **Attempt 1 (Standard Swap)**: Swapping X/Y to 594x459 caused severe skewing.
*   **Attempt 2 (Resolution Tuning)**: Reducing resolution helped but didn't eliminate artifacts until we enforced symmetry.
*   **Attempt 3 (Offset Tuning)**: Using `offset_y = 11` left a single pixel line artifact during partial redraws. Changing to `offset_y = 12` (even) resolved this.
