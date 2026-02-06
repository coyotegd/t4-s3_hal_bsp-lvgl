# ESP32 + LVGL MJPEG Video Playback

> **Using NanoJPEG for video playback on ESP32-S3**

This document describes a precise, end-to-end workflow for converting animated media (GIF, MP4, etc.) into an ESP32-compatible AVI (MJPEG) file and displaying it using the NanoJPEG decoder library.

### 🔄 The Evolution

This project went through several iterations to find a working solution:

1. **❌ LVGL's tjpgd** - Built-in decoder failed to handle MJPEG AVI frames reliably
2. **⚠️ Espressif's libjpeg-turbo** - Worked great but caused CMake dependency conflicts
3. **🚧 NanoJPEG** - Current attempt: lightweight, dependency-free, **BUT HAS COLOR FORMAT ISSUES**

### 🐛 Current Status: PARTIAL VICTORY

✅ **What's Working:**
- AVI parsing and frame extraction
- JPEG decoding to pixel data
- Animation playback in LVGL

❌ **What's Broken:**
- **Color channels are swapped** - displays green instead of red
- Likely RGB vs BGR byte order issue
- Or RGB565 format mismatch

**Key insight:** We're 90% there - frames decode and animate, but we need to fix the color format/byte ordering before claiming victory.

---

## ⚡ Quick Start

**If you just want to play AVI files in THIS PROJECT, jump to [Section 7: Using the Built-In AVI Player](#7-using-the-built-in-avi-player-this-project)**

---

## Table of Contents

1. [Problem Statement](#1-problem-statement)
2. [High-Level Strategy](#2-high-level-strategy)
3. [JPEG Decoder Requirements](#3-jpeg-decoder-requirements)
4. [FFmpeg Conversion](#4-ffmpeg-conversion)
5. [Verify the Output](#5-verify-the-output)
6. [ESP32-Side Architecture](#6-esp32-side-architecture)
7. [Using the Built-In AVI Player (THIS PROJECT)](#7-using-the-built-in-avi-player-this-project)
8. [Implementation Details](#8-implementation-details)
9. [Performance & Memory Constraints](#9-performance--memory-constraints)
10. [Frame Rate Control](#10-frame-rate-control)
11. [What You Do NOT Need to Do](#11-what-you-do-not-need-to-do)
12. [Final Takeaway](#12-final-takeaway)
13. [Optional Next Steps](#13-optional-next-steps)

---

## 1. Problem Statement

### The Challenge

- **ESP32-class MCUs** cannot decode MPEG / H.264 / inter-frame video codecs
- **AVI files** may contain many codecs — most are unusable on ESP32
- **LVGL's tjpgd decoder** has limited functionality and failed to decode MJPEG frames reliably
- **Espressif's libjpeg-turbo** worked initially but caused CMake build conflicts with other components

### The Journey

1. ❌ **LVGL's tjpgd** - Failed to decode MJPEG AVI frames properly
2. ⚠️ **Espressif's libjpeg-turbo** - Worked but caused CMake dependency conflicts
3. 🚧 **NanoJPEG** - Current WIP: Decodes and animates, but colors are wrong (green instead of red)

### The Goal

> Play animated content on ESP32 by extracting JPEG frames from an AVI container, decoding them to RGB565 using NanoJPEG, and displaying them in LVGL.

### 🐛 Current Blocker

**Color format mismatch** - Animation displays but colors are incorrect:
- Red appears as green (color channel swap)
- Possible causes:
  - RGB vs BGR byte ordering
  - RGB565 packing format mismatch
  - Color component bit shift issues

---

## 2. High-Level Strategy

> **Convert all animated media into MJPEG AVI, where each frame is a fully-formed baseline JPEG. Extract frames, decode to RGB565 using NanoJPEG, and display as LVGL images.**

### Architecture Overview

```
AVI File (SD Card)
    ↓
Extract JPEG frame (avi_mjpg_mgr.c)
    ↓
Inject missing DHT/DQT tables if needed
    ↓
Decode JPEG → RGB565 (nanojpeg.c)
    ↓
Wrap RGB565 buffer in lv_img_dsc_t
    ↓
Display in LVGL (ui_avi.c)
```

This approach works around LVGL's limited JPEG support by handling decoding externally.

---

## 3. JPEG Decoder Requirements

### Why Not LVGL's tjpgd?

**LVGL's built-in tjpgd decoder failed** to handle MJPEG AVI frames reliably. Issues included:
- Inconsistent handling of embedded vs missing headers
- Limited format support
- Difficult to debug failures

### Why Not Espressif's libjpeg-turbo?

**Espressif's libjpeg-turbo component worked well** but caused problems:
- ✅ Excellent performance
- ✅ Full JPEG compliance
- ❌ CMake dependency conflicts with other managed components
- ❌ Build system integration issues

### ✅ Current Solution: NanoJPEG (Work In Progress)

**NanoJPEG is a lightweight, single-file JPEG decoder** that:
- ✅ No external dependencies
- ✅ Simple integration (just `nanojpeg.c`)
- ✅ Decodes to pixel data successfully
- ✅ Small memory footprint
- ⚠️ Requires complete JPEG headers (DHT/DQT)
- 🐛 **Currently has color format issues** (RGB/BGR byte order problem)

### 🐛 Known Issue: Color Channel Swap

**Symptom:** Animation plays but colors are wrong - red appears as green

**Likely causes:**
1. NanoJPEG outputs RGB but LVGL expects BGR (or vice versa)
2. RGB565 packing order mismatch:
   - RGB565: `RRRR RGGG GGGB BBBB`
   - BGR565: `BBBB BGGG GGGR RRRR`
3. Byte endianness issue in RGB565 conversion

**Investigation needed:**
- Check NanoJPEG's RGB565 output format
- Verify LVGL's expected RGB565 format
- May need to swap R/B channels after decode
- Or use different color format (RGB888 → RGB565 conversion)

### NanoJPEG Requirements

Each frame must contain:
- **SOI** (0xFFD8) - Start of Image
- **DQT** - Quantization tables
- **DHT** - Huffman tables  
- **SOF0** - Frame header (baseline only)
- **SOS** - Scan header
- **EOI** (0xFFD9) - End of Image

**Note:** The `avi_mjpg_mgr.c` component automatically injects missing DHT/DQT tables for MJPEG files that lack them.

---

## 4. FFmpeg Conversion

### 4.1 Convert GIF / MP4 / WebM → MJPEG AVI

```bash
ffmpeg -i input.gif \
  -vf scale=320:-1:flags=lanczos \
  -an \
  -vcodec mjpeg \
  -pix_fmt yuvj422p \
  -q:v 8 \
  -f avi \
  output_mjpeg.avi
```

### 4.2 Why These Flags Matter

| Flag | Purpose |
|------|---------|
| `-vcodec mjpeg` | Forces JPEG per frame |
| `-pix_fmt yuvj422p` | Baseline JPEG compatible |
| `-q:v` | Controls size vs quality (1-31, lower = better) |
| `-an` | Removes audio (ESP32 unsupported) |
| `-f avi` | RIFF container with MJPEG |

### ✅ Result

- Every frame is an **independent JPEG image**
- All required metadata is **embedded per frame**

---

## 5. Verify the Output

### 5.1 Confirm Codec

```bash
ffprobe output_mjpeg.avi
```

**Expected output:**
```
Video: mjpeg (Baseline), yuvj422p
```

### 5.2 Inspect a Frame

```bash
ffmpeg -i output_mjpeg.avi -vframes 1 test.jpg
hexdump -C test.jpg | head
```

**You must see:**
```
ff d8 ff e0   # SOI + APP0
...
ff db         # DQT (Quantization tables)
ff c4         # DHT (Huffman tables)
```

**If you do, the file is properly formatted for NanoJPEG.** If DHT/DQT are missing, `avi_mjpg_mgr.c` will inject them automatically.

---

## 6. ESP32-Side Architecture

```
SD Card (/sdcard/video.avi)
    ↓
avi_mjpg_open() - Parse RIFF/AVI headers
    ↓
avi_mjpg_get_next_frame() - Extract JPEG chunk
    ↓
Check for DHT/DQT headers
    ↓
Inject standard tables if missing
    ↓
njDecode() - Decode JPEG → RGB565 pixels
    ↓
Return RGB565 buffer
    ↓
lv_image_set_src() - Display in LVGL
```

### Key Points

- **AVI is only a container** — only MJPEG video chunks are processed
- **Decoding happens on ESP32** using NanoJPEG (not LVGL's decoder)
- **Output is raw RGB565 pixels**, not JPEG data
- **LVGL displays pre-decoded images**, avoiding tjpgd entirely

---

## 7. Using the Built-In AVI Player (THIS PROJECT)

### 🎬 What's Included

This project includes a **complete AVI MJPEG player implementation**:

- **Components:** `ui_avi.c`, `avi_mjpg_mgr.c`
- **SD Card:** Auto-mounted at `/sdcard` during HAL init
- **API:** Simple LVGL widget-based interface

---

### 7.1 📁 Copy AVI Files to SD Card

1. Place your MJPEG AVI files in the `4_sd_card/` directory
2. Copy them to your physical SD card

**Sample files included:**
- `fallingcube.avi`
- `spacetime.avi`
- `circletriangle.avi`
- `pulpfictiondance.avi`

---

### 7.2 💻 Code Example

Add this to your `main/main.c`:

```c
#include "ui_avi.h"  // Add to includes

void app_main(void) {
    // ... after bsp_init() and lv_ui_init() ...
    
    lvgl_mgr_lock();
    
    // Create AVI player widget
    lv_obj_t * avi_player = ui_avi_create(lv_screen_active());
    lv_obj_center(avi_player);  // Center on screen
    
    // Load file from SD card (S: prefix = /sdcard)
    ui_avi_set_src(avi_player, "S:/fallingcube.avi");
    
    // Start playback
    ui_avi_play(avi_player);
    
    lvgl_mgr_unlock();
    
    // ... rest of code ...
}
```

---

### 7.3 📚 API Functions

| Function | Description |
|----------|-------------|
| `ui_avi_create(parent)` | Creates an AVI player widget |
| `ui_avi_set_src(obj, path)` | Loads AVI file ("S:/filename.avi") |
| `ui_avi_play(obj)` | Starts playback |
| `ui_avi_pause(obj)` | Pauses playback |
| `ui_avi_stop(obj)` | Stops and rewinds |

---

### 7.4 🗂️ Path Convention

Use **`"S:/filename.avi"`** format:
- `S:` prefix maps to `/sdcard` mount point
- Alternatively, use full path: `"/sdcard/filename.avi"`

---

### 7.5 🔧 How It Works Internally

```
SD Card (S:/video.avi)
         ↓
   avi_mjpg_open() - Open AVI, parse headers
         ↓
   avi_mjpg_get_next_frame() - Read JPEG chunk
         ↓
   Check/inject DHT/DQT tables
         ↓
   njDecode() - Decode to RGB565 buffer (PSRAM)
         ↓
   Return RGB565 pixel data
         ↓
   Wrap in lv_img_dsc_t (LV_COLOR_FORMAT_RGB565)
         ↓
   lv_image_set_src() - Display raw pixels
         ↓
   LVGL renders (no decoding needed)
```

**Critical difference:** LVGL receives **pre-decoded RGB565 pixels**, not JPEG data.

---

## 8. Implementation Details

### Decoding Pipeline

**File:** `avi_mjpg_mgr.c`

1. **Extract JPEG frame** from AVI container
2. **Validate JPEG headers** (SOI, DQT, DHT, SOS, EOI)
3. **Inject missing tables** if needed (standard DHT/DQT)
4. **Decode using NanoJPEG** → RGB565 buffer in PSRAM
5. **Return RGB565 buffer** to caller

### RGB565 Descriptor Format

**File:** `ui_avi.c`

Each decoded frame is wrapped as:

```c
lv_img_dsc_t {
    .header.magic = LV_IMAGE_HEADER_MAGIC
    .header.cf = LV_COLOR_FORMAT_RGB565  // RAW pixels
    .header.w = width
    .header.h = height
    .data = RGB565 buffer pointer  // NOT JPEG data
    .data_size = width * height * 2  // bytes
}
```

**LVGL receives:**
- ✅ Pre-decoded RGB565 pixel data
- ❌ NOT JPEG compressed data

**LVGL will:**
1. Recognize RGB565 raw format
2. Copy pixels directly to display buffer
3. No decoding occurs in LVGL

---

## 9. Performance & Memory Constraints

### 9.1 Practical Limits (ESP32-S3)

| Resolution | Status |
|------------|--------|
| 240×240 | ✅ Safe |
| 320×240 | ✅ Recommended |
| 480×320 | ⚠️ Risky |
| 600×400 | ❌ Usually too large |

### 9.2 Memory Math

```
RGB565 buffer = width × height × 2 bytes
```

**JPEG buffer + RGB output + LVGL heap** must all coexist in memory.

---

## 10. Frame Rate Control

Lower frame rates dramatically improve stability:

```bash
ffmpeg -i input.gif \
  -vf "fps=10,scale=320:-1" \
  -vcodec mjpeg \
  -pix_fmt yuvj422p \
  -q:v 10 \
  output_mjpeg.avi
```

**💡 Tip:** 10–12 FPS is ideal for ESP32.

---

## 11. What You Do NOT Need to Do

❌ Use LVGL's tjpgd decoder (bypassed)  
❌ Fight with libjpeg-turbo CMake conflicts (avoided)  
❌ Manually add JPEG headers (automatic injection)  
❌ Decode MPEG/H.264 on ESP32 (impossible)  
❌ Use FFmpeg libraries on-device (not needed)  

### What IS Happening Behind the Scenes

✅ **NanoJPEG decodes JPEG → RGB565** in `avi_mjpg_mgr.c`  
✅ **Automatic DHT/DQT injection** for incomplete MJPEG frames  
✅ **LVGL displays raw RGB565 pixels** (no decoding in LVGL)  
✅ **PSRAM used for frame buffers** to avoid SRAM exhaustion

---

## 12. Final Takeaway

✅ **MJPEG AVI** is the only viable video format for ESP32  
✅ **ffmpeg** produces fully compliant JPEG frames  
🚧 **NanoJPEG** decodes them (tjpgd failed, turbo_jpeg had conflicts)  
🐛 **Color format issue remains** - animation works but colors are swapped  
⏳ **Almost there** - 90% working, need to fix RGB/BGR byte ordering

### Lessons Learned

1. **LVGL's tjpgd is unreliable** for MJPEG playback
2. **Espressif's managed components** can cause CMake conflicts
3. **Single-file libraries** (like NanoJPEG) are easier to integrate
4. **Pre-decoding frames** avoids LVGL decoder limitations
5. **PSRAM is essential** for video frame buffers on ESP32-S3
6. **Color format debugging is critical** - RGB vs BGR, byte ordering matters

### 🔧 TODO: Fix Color Channels

**Current NanoJPEG RGB565 packing** (line 394 in `nanojpeg.c`):
```c
unsigned short p = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
// Format: RRRRRGGGGGGBBBBB (RGB565)
```

**Possible solutions:**

1. **Swap to BGR565:**
   ```c
   unsigned short p = ((b & 0xF8) << 8) | ((g & 0xFC) << 3) | (r >> 3);
   ```

2. **Byte swap (endianness):**
   ```c
   unsigned short p = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
   *out++ = (p >> 8) | (p << 8);  // Swap bytes
   ```

3. **Check LVGL color format setting:**
   - Verify `LV_COLOR_16_SWAP` is set correctly
   - Check if display driver expects BGR instead of RGB

4. **Debug with pure colors:**
   - Create test pattern: pure red (0xF800), green (0x07E0), blue (0x001F)
   - See what colors actually display

**Investigation steps:**
1. Check `rm690b0` display driver's expected format
2. Try BGR565 packing in nanojpeg.c
3. Test with single color frames to verify channel mapping

---

## 13. Optional Next Steps

### ✅ Already Implemented

- ✅ Minimal AVI RIFF parser (see `avi_mjpg_mgr.c`)
- ✅ Frame timing using `lv_timer` (see `ui_avi.c`)
- ✅ Use PSRAM for large frame buffers (see `lvgl_mgr.c`)

### � URGENT: Fix Color Format Issue

**Problem:** Red displays as green (color channel swap)

**Debug approach:**
```c
// In avi_mjpg_mgr.c or ui_avi.c, test with pure colors:
uint16_t test_pixels[320*240];

// Pure red (RGB565: 0xF800)
for (int i = 0; i < 320*240; i++) test_pixels[i] = 0xF800;
lv_image_set_src(obj, test_pixels);  // Should show RED

// Pure green (RGB565: 0x07E0)
for (int i = 0; i < 320*240; i++) test_pixels[i] = 0x07E0;
lv_image_set_src(obj, test_pixels);  // Should show GREEN

// Pure blue (RGB565: 0x001F)
for (int i = 0; i < 320*240; i++) test_pixels[i] = 0x001F;
lv_image_set_src(obj, test_pixels);  // Should show BLUE
```

If colors still wrong → try BGR565 values (swap R/B):
- Red: `0x001F` (blue bits)
- Blue: `0xF800` (red bits)

**Likely fixes:**

1. **Change nanojpeg.c line 394 to BGR565:**
   ```c
   // OLD: RGB565
   unsigned short p = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
   
   // NEW: BGR565
   unsigned short p = ((b & 0xF8) << 8) | ((g & 0xFC) << 3) | (r >> 3);
   ```

2. **Or swap bytes in output:**
   ```c
   unsigned short p = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
   *out++ = __builtin_bswap16(p);  // Byte swap
   ```

3. **Check LVGL config** in `lv_conf.h`:
   ```c
   #define LV_COLOR_16_SWAP 0  // Try changing to 1
   ```

### 🚀 Enhancement Ideas (After Color Fix)

- Compare MJPEG vs PNG frame sequences
- Add playback controls (rewind, seek, speed)
- Implement playlist functionality
- Add on-screen display (OSD) overlays

---

## 🔬 Debugging Reference

### RGB565 Format Reference

**Standard RGB565 (16-bit):**
```
Bit:  15 14 13 12 11 | 10 09 08 07 06 05 | 04 03 02 01 00
      R  R  R  R  R  | G  G  G  G  G  G  | B  B  B  B  B
```

**Pure colors in RGB565:**
- Red:   `0xF800` (binary: `11111 000000 00000`)
- Green: `0x07E0` (binary: `00000 111111 00000`)
- Blue:  `0x001F` (binary: `00000 000000 11111`)
- White: `0xFFFF`
- Black: `0x0000`

**BGR565 (same structure, swapped channels):**
- Red:   `0x001F` (in blue position)
- Green: `0x07E0` (same)
- Blue:  `0xF800` (in red position)

### Display Driver Check

Your display controller: **RM690B0**

Check in `rm690b0.c` for:
- Color format settings (RGB vs BGR)
- Byte order configuration
- MADCTL register settings (0x36) - controls RGB/BGR order

---

## 📝 Notes

This document intentionally avoids shortcuts. If you follow it verbatim, your MJPEG playback will work.

**The approach is conservative and hardware-aware by design.**
