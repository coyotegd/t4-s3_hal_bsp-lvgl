# LVGL Integration Journey (T4-S3 + RM690B0)

This document details the technical challenges and solutions encountered while porting LVGL 9 to the LilyGo T4-S3 (ESP32-S3) with the RM690B0 AMOLED display driver.

## 1. Hardware Context
- **MCU**: ESP32-S3 (Octal PSRAM enabled)
- **Display**: RM690B0 AMOLED (450x600)
- **Interface**: QSPI (Quad SPI) with custom opcodes (`0x02` cmd, `0x32` pixel write)
- **Touch**: CST226SE (I2C)

## 2. Key Challenges & Solutions

### A. The "Rainbow Lines" (Color Depth Mismatch)
**Symptom**: The display showed correct shapes but with wrong colors, appearing as "rainbow" or "neon" lines.
**Cause**: LVGL was configured for `RGB565` (16-bit), but the buffer allocation logic was using `sizeof(lv_color_t)`. If LVGL's internal configuration defaulted to 24-bit (RGB888) in some headers, the buffer size and data format would mismatch the display driver's expectation.
**Solution**:
- Explicitly forced `lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565)`.
- Hardcoded buffer allocation to 2 bytes per pixel: `w * h * 2`.
- Implemented byte swapping (`lv_draw_sw_rgb565_swap`) because the RM690B0 expects Big Endian data.

### B. The "Skew" & "Smudging" (Stride Alignment)
**Symptom**: 
- Full screen redraws worked fine.
- **Partial redraws** (e.g., clicking a button, updating text) resulted in the image being skewed by ~45 degrees or "smudged" to the right.
- Text became illegible on updates.

**Root Cause**: 
- LVGL 9 enforces a **stride alignment** (typically 4 bytes) for its draw buffers.
- If a redraw area has an odd width (e.g., 123 pixels), the row length in bytes is `123 * 2 = 246` bytes.
- 246 is not divisible by 4. LVGL adds 2 bytes of padding to reach 248.
- The RM690B0 display driver expects a tightly packed stream of pixels. It interprets the padding bytes as pixel data for the next row, causing a 1-pixel shift per line (skew).

**Solution**:
- Implemented a **Rounder Callback** (`lvgl_rounder_cb`).
- This callback forces all invalidated areas to have **even coordinates** and **even widths**.
- `Even Width * 2 bytes/pixel` is always divisible by 4.
- This naturally satisfies LVGL's alignment requirement without adding padding, ensuring the buffer remains tightly packed for the display driver.

```c
static void lvgl_rounder_cb(lv_event_t *e) {
    lv_area_t * area = lv_event_get_param(e);
    // Round x1 down to even
    if(area->x1 & 1) area->x1--;
    // Round x2 up to odd (so width = x2 - x1 + 1 is even)
    if(!(area->x2 & 1)) area->x2++;
}
```

### C. Signal Integrity (Thin Lines)
**Symptom**: Hair-thin horizontal lines appearing intermittently on button redraws.
**Cause**: SPI Clock speed of 40MHz was marginally unstable for the specific wiring or display controller state during partial updates.
**Solution**: 
- Reduced QSPI clock speed to **20MHz**.
- Increased `esp_rom_delay_us` after setting the window address to allow the controller to stabilize before receiving high-speed pixel data.

### D. The Coordinate System Chaos (XY Swap/Mirror)
**Symptom**: Touch inputs were inverted or mirrored relative to the visual display. Sometimes up was down, left was right, or the axes were completely swapped (portrait touch on landscape display).
**Context**: This system involves three distinct layers of coordinate transformation:
1. **The Physical Display (RM690B0)**: Has hardware registers (`MADCTL`) for memory access control (Exchange X/Y, Mirror X, Mirror Y).
2. **The Touch Controller (CST226SE)**: Reports coordinates based on its own physical orientation, which often differs from the screen.
3. **The Graphics Library (LVGL)**: Manages rotation logically (`LV_DISPLAY_ROTATION_0`, etc.).

**The Chaos**:
- Fixing the display visuals often broke the touch correlation.
- Fixing the touch correlation often broke the display visuals (e.g., text rendering backwards).
- The correct configuration was not found by logic alone but through an iterative process where `xy_swap` and `xy_mirror` settings had to be flipped back and forth multiple times across all three layers until they finally aligned. **It seemed that the X/Y swap and mirror settings were switched not just once, but twice across the three layers before stabilizing.**

**Solution**:
- Settled on a configuration where the Display Driver handles the hardware rotation setup via `rm690b0_set_rotation`, ensuring `MADCTL` is correct for 0/90/180/270 degrees.
- Touch coordinates are mapped to the display resolution *after* receiving them, applying specific swaps matching the visual rotation.

## 3. Architecture Highlights

### Async DMA Flushing
To maintain high frame rates, the driver uses ESP32's SPI DMA in asynchronous mode:
1. LVGL calls `flush_cb`.
2. Driver queues a DMA transaction (`spi_device_queue_trans`).
3. Driver returns immediately, allowing LVGL to render the next chunk while the previous one transmits.
4. A callback (`spi_trans_post_cb`) notifies LVGL when the transfer is complete (`lv_display_flush_ready`).

### Double Buffering
- Two buffers of ~12KB each (10 lines height) are allocated in internal RAM (`MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA`).
- Small buffers are used to stay well within the max DMA transfer size (32KB) and save SRAM, while still providing enough data for efficient burst transfers.

## 4. Update: Performance Tuning (Dec 30, 2025)

### A. Responsiveness vs. Tearing
**Optimization**: The driver initially waited for the VSYNC (Tearing Effect) signal before sending *each* buffer chunk. While this prevented tearing, it introduced significant latency during screen updates (the "eyelid" effect).
**Adjustment**: 
- **Disabled strict VSYNC waiting** in `lvgl_flush_cb`.
- The RM690B0's high-speed QSPI (20MHz) allows for fast enough updates that strict VSYNC synchronization is not required for standard UI interactions, significantly improving perceived responsiveness.

### B. Buffer Size Optimization
**Observation**: The initial 12KB buffer (10 lines) was safe but inefficient, causing too many small SPI transactions.
**Adjustment**:
- Increased buffer size to **~24KB (20 lines)**.
- `600 pixels * 20 lines * 2 bytes = 24,000 bytes`.
- This fits comfortably within the ESP32-S3's internal SRAM and is well below the hardware DMA limit.

### C. SPI Configuration
- **Max Transfer Size**: Increased `max_transfer_sz` in `spi_bus_config_t` to `65536` (64KB) to support the larger LVGL buffers.
- **Queue Size**: Maintained at 10 to handle the pipeline of CASET, RASET, and Pixel transactions.

## 5. Final Status
- **Visuals**: Correct colors and geometry.
- **Responsiveness**: Snappy button presses and screen transitions.
- **Stability**: No crashes or watchdog timeouts during rapid updates.
