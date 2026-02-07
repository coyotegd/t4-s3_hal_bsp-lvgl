# ESP32 + LVGL MJPEG Video Playback

> **Using standard libjpeg for video playback on ESP32-S3**

This document describes a precise, end-to-end workflow for converting animated media (GIF, MP4, etc.) into an ESP32-compatible AVI (MJPEG) file and displaying it using the standard libjpeg decoder library.

### 🔄 The Evolution

This project went through several iterations to find a working solution:

1. **❌ LVGL's tjpgd** - Built-in decoder failed to handle MJPEG AVI frames reliably
2. **⚠️ Espressif's libjpeg-turbo** - Worked great but caused CMake dependency conflicts  
3. **❌ NanoJPEG** - Lightweight, dependency-free, but had color format issues
4. **✅ Standard libjpeg** - FINAL SOLUTION: Reliable, fast, and works perfectly

### ✅ Current Status: COMPLETE AND WORKING

✅ **Fully Functional:**
- AVI parsing and frame extraction
- JPEG decoding to RGB24 pixel data using standard libjpeg
- RGB24 to RGB565 conversion
- Correct color output (no channel swapping)
- Smooth animation playback in LVGL
- Automatic looping support
- PSRAM buffer management

**The system is production-ready and working reliably.**

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
3. ❌ **NanoJPEG** - Lightweight but had RGB/BGR color channel issues
4. ✅ **Standard libjpeg** - FINAL SOLUTION: Reliable and works perfectly

### The Goal

> Play animated content on ESP32 by extracting JPEG frames from an AVI container, decoding them to RGB24 using standard libjpeg, converting to RGB565, and displaying them in LVGL.

### ✅ Mission Accomplished

The system now uses **standard libjpeg** which provides:
- Complete JPEG compliance
- Reliable decoding of all MJPEG variants
- Correct RGB color output
- Excellent performance on ESP32-S3

---

## 2. High-Level Strategy

> **Convert all animated media into MJPEG AVI, where each frame is a fully-formed baseline JPEG. Extract frames, decode to RGB24 using standard libjpeg, convert to RGB565, and display as LVGL images.**

### Architecture Overview

```
AVI File (SD Card)
    ↓
Extract JPEG frame (avi_mjpg_mgr.c)
    ↓
Decode JPEG → RGB24 (standard libjpeg)
    ↓
Convert RGB24 → RGB565 (manual pixel conversion)
    ↓
Wrap RGB565 buffer in lv_img_dsc_t
    ↓
Display in LVGL (ui_avi.c)
```

This approach uses standard libjpeg for robust JPEG decoding, with manual RGB565 conversion for LVGL compatibility.

---

## 3. JPEG Decoder Requirements

### The Journey Through JPEG Decoders

Getting MJPEG video playback working on ESP32-S3 was not straightforward. Multiple decoder libraries were tried, each with its own set of problems.

### ❌ Attempt 1: LVGL's Built-in tjpgd Decoder

**Why we tried it:**
- Already integrated with LVGL
- No additional dependencies
- Seemed like the obvious first choice

**Why it failed:**
- ❌ Inconsistent handling of MJPEG frames extracted from AVI containers
- ❌ Would work for some frames, fail on others from the same file
- ❌ Limited JPEG format support (baseline only, very strict)
- ❌ Poor error reporting - hard to debug what was actually wrong
- ❌ Couldn't handle frames with missing or abbreviated headers (common in MJPEG)
- ❌ Memory management issues with larger frames

**Lesson learned:** LVGL's tjpgd is designed for static images, not video frame sequences.

---

### ⚠️ Attempt 2: Espressif's libjpeg-turbo Component

**Why we tried it:**
- Espressif officially provides it as a managed component
- Industry-standard JPEG decoder
- Excellent performance
- Full JPEG compliance

**Why it was abandoned:**
- ✅ Actually worked perfectly for JPEG decoding
- ✅ Fast and reliable
- ❌ **CMake dependency conflicts** with other ESP-IDF managed components
- ❌ Build system would fail when other components were added
- ❌ Circular dependency issues in menuconfig
- ❌ Version conflicts between different managed components expecting different libjpeg versions
- ❌ Would break when updating ESP-IDF or other components

**Example error:**
```
CMake Error: Dependency graph includes circular dependencies
  libjpeg-turbo -> component_x -> libjpeg-turbo
```

**Lesson learned:** Managed components can have hidden dependency conflicts that only appear in complex projects.

---

### ❌ Attempt 3: NanoJPEG (Single-file decoder)

**Why we tried it:**
- Lightweight, single-file implementation
- No external dependencies
- Easy to integrate (just drop nanojpeg.c into project)
- Seemed perfect for avoiding CMake conflicts

**What worked:**
- ✅ Successfully decoded JPEG frames
- ✅ Animation playback worked
- ✅ No dependency conflicts
- ✅ Small memory footprint

**Why it failed:**
- ❌ **Color channels were swapped** - red appeared as green, blue appeared incorrectly
- ❌ RGB vs BGR byte order issues that couldn't be resolved
- ❌ RGB565 packing format didn't match what the display expected
- ❌ Tried multiple solutions:
  - Swapping R and B channels in conversion code
  - Byte swapping the RGB565 output
  - Changing LVGL color format settings
  - Adjusting display driver color order
- ❌ Hours spent debugging color format mismatches
- ❌ Required complete JPEG headers (DHT/DQT tables), necessitating header injection code

**Attempted fixes that didn't work:**
```c
// Try 1: Swap R and B channels
rgb565 = ((b & 0xF8) << 8) | ((g & 0xFC) << 3) | (r >> 3);  // Still wrong

// Try 2: Byte swap
rgb565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
*out++ = __builtin_bswap16(rgb565);  // Still wrong

// Try 3: Various RGB565 packing attempts
// None produced correct colors on the RM690B0 display
```

**Lesson learned:** Color format compatibility between decoder, color space conversion, and display driver is critical and can be surprisingly difficult to get right.

---

### ✅ Final Solution: Standard libjpeg

**Why we went back to try it:**
- After NanoJPEG's color issues, decided to revisit libjpeg
- This time, use standard libjpeg (not Espressif's managed version)
- Build it directly into the project to avoid managed component conflicts

**How it's different from attempt #2:**
- Not using ESP-IDF's managed component system
- Included as a regular component in the project
- Full control over build configuration
- No dependency on other managed components

**Why it finally worked:**
- ✅ Industry-standard decoder with complete JPEG support
- ✅ Outputs RGB24 format (3 bytes per pixel) which is straightforward to convert
- ✅ Manual RGB24 → RGB565 conversion gives precise control
- ✅ **Colors are correct** - proper RGB ordering
- ✅ Handles all JPEG variants found in MJPEG files
- ✅ Excellent performance on ESP32-S3
- ✅ Well-documented, stable, reliable
- ✅ No CMake conflicts (when built as regular component)

**The RGB24 to RGB565 conversion that finally works:**
```c
// libjpeg outputs RGB24 (r, g, b as separate bytes)
uint8_t r = rgb24[0];
uint8_t g = rgb24[1];
uint8_t b = rgb24[2];

// Convert to RGB565 (standard format)
rgb565[i] = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
// Result: Correct colors on display!
```

**Lesson learned:** Sometimes the standard, proven solution is standard and proven for a reason. Don't be afraid to revisit earlier approaches with a different integration strategy.

---

### Summary of the Journey

| Decoder | Pros | Fatal Flaw | Result |
|---------|------|------------|--------|
| **tjpgd** | Built-in, no deps | Unreliable MJPEG frame decoding | ❌ Failed |
| **libjpeg-turbo (managed)** | Fast, reliable decoding | CMake dependency conflicts | ⚠️ Abandoned |
| **NanoJPEG** | Lightweight, no deps | Color channel swap issues | ❌ Failed |
| **libjpeg (standard)** | Proven, reliable | None (when integrated correctly) | ✅ **Success** |

**Time investment:** Weeks of troubleshooting before finding the working solution.

**Key insight:** The winning combination was standard libjpeg + manual RGB565 conversion + proper build integration.

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
jpeg_mem_src() - Setup libjpeg memory source
    ↓
jpeg_read_header() - Read JPEG headers
    ↓
jpeg_start_decompress() - Start decompression
    ↓
jpeg_read_scanlines() - Decode to RGB24
    ↓
Convert RGB24 → RGB565 - Manual pixel conversion
    ↓
Return RGB565 buffer
    ↓
lv_image_set_src() - Display in LVGL
```

### Key Points

- **AVI is only a container** — only MJPEG video chunks are processed
- **Decoding happens on ESP32** using standard libjpeg (not LVGL's decoder)
- **Output is RGB565 pixels**, converted from RGB24
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
2. **Setup libjpeg** with memory source (`jpeg_mem_src`)
3. **Read JPEG headers** (`jpeg_read_header`)
4. **Start decompression** (`jpeg_start_decompress`)
5. **Decode to RGB24** (`jpeg_read_scanlines`)
6. **Convert RGB24 to RGB565** - Manual pixel conversion in PSRAM
7. **Return RGB565 buffer** to caller

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
❌ Fight with libjpeg-turbo CMake conflicts (avoided by using standard libjpeg)  
❌ Fix color channel swaps (working correctly now)  
❌ Manually add JPEG headers (libjpeg handles all variants)  
❌ Decode MPEG/H.264 on ESP32 (impossible)  
❌ Use FFmpeg libraries on-device (not needed)  

### What IS Happening Behind the Scenes

✅ **Standard libjpeg decodes JPEG → RGB24** in `avi_mjpg_mgr.c`  
✅ **Manual RGB24 → RGB565 conversion** for LVGL compatibility  
✅ **LVGL displays raw RGB565 pixels** (no decoding in LVGL)  
✅ **PSRAM used for frame buffers** to avoid SRAM exhaustion

---

## 12. Final Takeaway

✅ **MJPEG AVI** is the only viable video format for ESP32  
✅ **ffmpeg** produces fully compliant JPEG frames  
✅ **Standard libjpeg** finally works (after tjpgd failed, turbo_jpeg had conflicts, NanoJPEG had color issues)  
✅ **Manual RGB24→RGB565 conversion** ensures correct colors  
✅ **System is complete and production-ready** (after significant troubleshooting)

### Lessons Learned (The Hard Way)

1. **LVGL's tjpgd is unreliable for MJPEG playback**
   - Works for static images, fails for video frame sequences
   - Poor error reporting made debugging difficult

2. **Espressif's managed components can cause CMake conflicts**
   - Even official components can conflict in complex projects
   - Dependency hell is real in ESP-IDF ecosystem

3. **NanoJPEG had unfixable color channel issues**
   - RGB/BGR problems that couldn't be resolved
   - Spent significant time on color format debugging
   - Single-file simplicity doesn't guarantee compatibility

4. **Standard libjpeg is the reliable solution**
   - Industry standard for a reason
   - Build as regular component (not managed) to avoid conflicts
   - RGB24 output format is easier to work with than direct RGB565

5. **Pre-decoding frames avoids LVGL decoder limitations**
   - LVGL receives raw RGB565 pixels
   - No dependency on LVGL's JPEG decoding capabilities

6. **PSRAM is essential for video playback**
   - Video frame buffers are too large for internal SRAM
   - ESP32-S3 with PSRAM is minimum requirement

7. **Manual color conversion gives precise control**
   - Direct control over RGB565 packing format
   - Can verify output format matches display expectations
   - Easier to debug than library-internal conversions

8. **Integration strategy matters as much as library choice**
   - Same library can work or fail depending on how it's integrated
   - libjpeg-turbo (managed component) = conflicts
   - libjpeg (regular component) = works perfectly

### Reality Check

**This was not easy.** The working solution came after:
- ❌ 3 failed decoder attempts
- ⚠️ Weeks of debugging and troubleshooting
- 🔧 Multiple color format debugging sessions
- 📚 Deep dives into RGB565 format, JPEG internals, and CMake build systems
- 🧪 Countless test builds and iterations

**But it was worth it.** The final implementation is solid, reliable, and production-ready.

### ✅ Implementation Complete

**Current RGB565 conversion** (in `avi_mjpg_mgr.c`):
```c
// Convert RGB24 to RGB565
uint16_t *rgb565 = (uint16_t *)handle->rgb_buffer;
uint8_t *rgb24 = temp_rgb24;
for (int i = 0; i < handle->width * handle->height; i++) {
    uint8_t r = rgb24[0];
    uint8_t g = rgb24[1];
    uint8_t b = rgb24[2];
    rgb565[i] = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
    rgb24 += 3;
}
```

**RGB565 Format:**
```
Bit:  15 14 13 12 11 | 10 09 08 07 06 05 | 04 03 02 01 00
      R  R  R  R  R  | G  G  G  G  G  G  | B  B  B  B  B
```

This produces standard RGB565 format that works correctly with LVGL and the RM690B0 display.

---

## 13. Optional Next Steps

### ✅ Already Implemented

- ✅ Minimal AVI RIFF parser (see `avi_mjpg_mgr.c`)
- ✅ Frame timing using `lv_timer` (see `ui_avi.c`)
- ✅ Use PSRAM for large frame buffers
- ✅ Standard libjpeg integration
- ✅ RGB24 to RGB565 conversion
- ✅ Automatic looping support
- ✅ Correct color output
### 🚀 Enhancement Ideas

- Compare MJPEG vs PNG frame sequences
- Add playback controls (rewind, seek, speed controls)
- Implement playlist functionality
- Add on-screen display (OSD) overlays
- Variable speed playback
- Frame-by-frame stepping

---

## 🔬 Reference Information

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

### Display Driver

Display controller: **RM690B0**

The implementation uses standard RGB565 format which is correctly configured for this display controller.

---

## 📝 Notes

This document provides a complete, working solution for MJPEG video playback on ESP32-S3.

**The system is production-ready and has been thoroughly tested with the included sample AVI files.**
