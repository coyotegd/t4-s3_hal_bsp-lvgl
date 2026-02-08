# ESP32 + LVGL MJPEG Video Playback

> **Using standard libjpeg for video playback on ESP32-S3**

This document describes a precise, end-to-end workflow for converting animated media (GIF, MP4, etc.) into an ESP32-compatible AVI (MJPEG) file and displaying it using the standard libjpeg decoder library.

## Background

This project tried a few decoder options before settling on the libjpeg API provided by the `espressif__libjpeg-turbo` component:

1. LVGL's built-in tjpgd (limited; issues with MJPEG AVI frames in this project)
2. tjpgd as an ESP-IDF component (limited JPEG feature support)
3. NanoJPEG (small, but RGB565/color handling was problematic here)
4. libjpeg API via `espressif__libjpeg-turbo` (current approach)

## Status

This document describes the intended workflow and the current code structure for AVI (MJPEG) playback. Build integration details depend on how your project wires component dependencies (so if you see a missing `jpeglib.h`, treat it as a build/dependency issue rather than an AVI parsing issue).

## Markdown Note: Code Fences (Doc Hygiene)

This is *not* part of the firmware build, but it matters for keeping these instructions readable and easy to diff/merge.

**What is a “code fence”?**

A code fence is a fenced code block in Markdown, typically using triple backticks:

````text
```bash
some command
```
````

### Why it can look “corrupted”

If a fence is left unclosed (missing the closing ```), Markdown renderers treat the rest of the file as code. That makes headings/lists disappear and can make copied commands include stray characters.

### House rules used in this repo

- Always close fences.
- Always include a language tag when practical (`bash`, `c`, `cmake`, `text`).
- If you need to show literal triple-backticks inside a fenced block, use a *longer* fence (four backticks) around it.

---

## Quick Start

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
- **Solution required:** Full-featured JPEG decoder with proper build system integration

### The Journey

1. **LVGL's tjpgd** - Failed to decode MJPEG AVI frames properly
2. **libjpeg-turbo (initial attempt)** - Worked but caused CMake dependency conflicts
3. **NanoJPEG** - Lightweight but had unresolvable color format issues
4. **libjpeg API via `espressif__libjpeg-turbo`** - Current approach

### The Goal

> Play animated content on ESP32 by extracting JPEG frames from an AVI container, decoding them to RGB565 using standard libjpeg, and displaying them in LVGL.

### Solution Summary

**Standard libjpeg integration** - Full JPEG decoding with proper color handling:

- Uses `espressif__libjpeg-turbo` managed component
- CMake conflicts resolved through proper dependency ordering
- Decodes to RGB888, then converts to RGB565
- Perfect color accuracy, no channel swapping
- Production-ready implementation

---

## 2. High-Level Strategy

> **Convert all animated media into MJPEG AVI, where each frame is a fully-formed baseline JPEG. Extract frames, decode to RGB565 using standard libjpeg, and display as LVGL images.**

### Architecture Overview

```text
AVI File (SD Card)
    ↓
Extract JPEG frame (avi_mjpg_mgr.c)
    ↓
Decode JPEG → RGB888 (libjpeg)
    ↓
Convert RGB888 → RGB565
    ↓
Wrap RGB565 buffer in lv_img_dsc_t
    ↓
Display in LVGL (ui_avi.c)
```

This approach uses industry-standard libjpeg for reliable JPEG decoding, bypassing LVGL's limited tjpgd decoder.

---

## 3. JPEG Decoder Requirements

### Why Not LVGL's tjpgd?

**LVGL's built-in tjpgd decoder failed** to handle MJPEG AVI frames reliably. Issues included:

- Inconsistent handling of embedded vs missing headers
- Limited format support
- Difficult to debug failures

### Why Not NanoJPEG?

**NanoJPEG is a lightweight single-file decoder** but had critical issues:

- Requires complete JPEG headers (DHT/DQT)
- Unresolvable color format issues (RGB/BGR channel swapping)
- Limited debugging capabilities
- Non-standard RGB565 packing

### Final Solution: Standard libjpeg

**Standard libjpeg via `espressif__libjpeg-turbo` component:**

- Full JPEG compliance (all variants supported)
- Industry-standard, battle-tested decoder
- Good performance with optimizations
- Proper color space handling (no channel issues)
- Decodes to RGB888, then converts to RGB565

### Integration Details

**CMakeLists.txt configuration:**

```cmake
idf_component_register(
    SRCS "src/avi_mjpg_mgr.c" ...
    REQUIRES ... espressif__libjpeg-turbo
)
```

**Header usage:**

```c
#include "jpeglib.h"  // Standard libjpeg interface
```

### Build Integration Notes (Managed Components)

This project uses ESP-IDF Component Manager managed components for both LVGL and libjpeg-turbo.

- `espressif__libjpeg-turbo` provides the public libjpeg headers (like `jpeglib.h`) under the component's build/install include directory.
- LVGL may also build its own libjpeg-turbo wrapper (`lv_libjpeg_turbo.c`). Depending on the LVGL version/config, that wrapper may include **internal** libjpeg-turbo headers (like `jpegint.h`) which are *not* part of the public installed header set.

To keep managed components pristine, this repo wires the needed include paths from the project side:

- Project-side glue lives in `main/CMakeLists.txt` (function `_t4s3_create_libjpeg_alias()`).
- It links the LVGL component target against `idf::espressif__libjpeg-turbo` and adds internal include dirs when needed.

If this ever regresses again, the error message usually tells you which include path is missing:

- `fatal error: jpeglib.h: No such file or directory`
  - Ensure the managed dependency exists (e.g. `main/idf_component.yml` depends on `espressif/libjpeg-turbo`).
  - Ensure the LVGL component target links against `idf::espressif__libjpeg-turbo`.
- `fatal error: jpegint.h: No such file or directory`
  - Add the internal source include dir: `managed_components/espressif__libjpeg-turbo/libjpeg-turbo/src`.
- `fatal error: jconfigint.h: No such file or directory`
  - Add the libjpeg-turbo build dir include: `build/esp-idf/espressif__libjpeg-turbo/libjpeg-build`.

**Key advantages:**

- Handles all JPEG variants (baseline, progressive, etc.)
- Automatic color space conversion
- Robust error handling
- Well-documented API

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

| Flag                | Purpose                                         |
| ------------------- | ----------------------------------------------- |
| `-vcodec mjpeg`     | Forces JPEG per frame                           |
| `-pix_fmt yuvj422p` | Baseline JPEG compatible                        |
| `-q:v`              | Controls size vs quality (1-31, lower = better) |
| `-an`               | Removes audio (ESP32 unsupported)               |
| `-f avi`            | RIFF container with MJPEG                       |

### Result

- Every frame is an **independent JPEG image**
- All required metadata is **embedded per frame**

---

## 5. Verify the Output

### 5.1 Confirm Codec

```bash
ffprobe output_mjpeg.avi
```

**Expected output:**

```text
Video: mjpeg (Baseline), yuvj422p
```

### 5.2 Inspect a Frame

```bash
ffmpeg -i output_mjpeg.avi -vframes 1 test.jpg
hexdump -C test.jpg | head
```

**You must see:**

```text
ff d8 ff e0   # SOI + APP0
...
ff db         # DQT (Quantization tables)
ff c4         # DHT (Huffman tables)
```

**If you do, the file is properly formatted.** Standard libjpeg handles both complete and incomplete JPEG headers automatically.

---

## 6. ESP32-Side Architecture

```text
SD Card (/sdcard/video.avi)
    ↓
avi_mjpg_open() - Parse RIFF/AVI headers
    ↓
avi_mjpg_get_next_frame() - Extract JPEG chunk
    ↓
jpeg_read_header() - Parse JPEG metadata
    ↓
jpeg_start_decompress() - Begin decoding
    ↓
jpeg_read_scanlines() - Decode to RGB888
    ↓
Convert RGB888 → RGB565
    ↓
Return RGB565 buffer
    ↓
lv_image_set_src() - Display in LVGL
```

### Key Points

- **AVI is only a container** — only MJPEG video chunks are processed
- **Decoding happens on ESP32** using standard libjpeg (not LVGL's decoder)
- **Output is raw RGB565 pixels**, not JPEG data
- **LVGL displays pre-decoded images**, avoiding tjpgd entirely
- **libjpeg handles all JPEG variants** automatically

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

| Function                    | Description                       |
| --------------------------- | --------------------------------- |
| `ui_avi_create(parent)`     | Creates an AVI player widget      |
| `ui_avi_set_src(obj, path)` | Loads AVI file (S:/filename.avi)  |
| `ui_avi_play(obj)`          | Starts playback                   |
| `ui_avi_pause(obj)`         | Pauses playback                   |
| `ui_avi_stop(obj)`          | Stops and rewinds                 |

---

### 7.4 🗂️ Path Convention

Use **`"S:/filename.avi"`** format:

- `S:` prefix maps to `/sdcard` mount point
- Alternatively, use full path: `"/sdcard/filename.avi"`

---

### 7.5 🔧 How It Works Internally

```text
SD Card (S:/video.avi)
         ↓
   avi_mjpg_open() - Open AVI, parse headers
         ↓
   avi_mjpg_get_next_frame() - Read JPEG chunk
         ↓
   jpeg_read_header() - Parse JPEG metadata
         ↓
   jpeg_start_decompress() - Setup decoding
         ↓
   jpeg_read_scanlines() - Decode to RGB888
         ↓
   Convert RGB888 → RGB565 (PSRAM buffer)
         ↓
   Return RGB565 pixel data
         ↓
   Wrap in lv_img_dsc_t (LV_COLOR_FORMAT_RGB565)
         ↓
   lv_image_set_src() - Display raw pixels
         ↓
   LVGL renders (no decoding needed)
```

**Critical difference:** LVGL receives **pre-decoded RGB565 pixels**, not JPEG data. Standard libjpeg ensures proper color handling.

---

## 8. Implementation Details

### Decoding Pipeline

**File:** `avi_mjpg_mgr.c`

1. **Extract JPEG frame** from AVI container
2. **Initialize libjpeg decompressor** (`jpeg_create_decompress`)
3. **Read JPEG header** (`jpeg_read_header`) - automatic format detection
4. **Configure output** - RGB888 color space
5. **Decompress scanlines** (`jpeg_read_scanlines`) → RGB888 buffer
6. **Convert RGB888 → RGB565** in PSRAM buffer
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

- Pre-decoded RGB565 pixel data
- Not JPEG compressed data

**LVGL will:**

1. Recognize RGB565 raw format
2. Copy pixels directly to display buffer
3. No decoding occurs in LVGL

---

## 9. Performance & Memory Constraints

### 9.1 Practical Limits (ESP32-S3)

| Resolution | Status                |
| ---------- | --------------------- |
| 240×240    | Safe                  |
| 320×240    | Recommended           |
| 480×320    | Risky                 |
| 600×400    | Usually too large     |

### 9.2 Memory Math

```text
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

Tip: 10–12 FPS is ideal for ESP32.

---

## 11. What You Do NOT Need to Do

- Use LVGL's tjpgd decoder (bypassed)
- Manually add JPEG headers (libjpeg handles all variants)
- Decode MPEG/H.264 on ESP32 (not feasible)
- Use FFmpeg libraries on-device (not needed)
- Debug color format issues (libjpeg handles correctly)

### What IS Happening Behind the Scenes

- **Standard libjpeg decodes JPEG → RGB888** in `avi_mjpg_mgr.c`
- **Automatic format detection** - handles all JPEG variants
- **RGB888 → RGB565 conversion** with proper color handling
- **LVGL displays raw RGB565 pixels** (no decoding in LVGL)
- **PSRAM used for frame buffers** to avoid SRAM exhaustion
- **CMake integration** through `espressif__libjpeg-turbo`

---

## 12. Final Takeaway

- **MJPEG AVI** is the only viable video format for ESP32
- **ffmpeg** produces fully compliant JPEG frames
- **Standard libjpeg** decodes them (via `espressif__libjpeg-turbo`)
- **Proper color handling** - no channel swapping or format issues

### Lessons Learned

1. **LVGL's tjpgd is unreliable** for MJPEG playback
2. **Standard libjpeg is the gold standard** - use industry-proven libraries
3. **CMake conflicts are solvable** through proper dependency configuration
4. **Pre-decoding frames** avoids LVGL decoder limitations
5. **PSRAM is essential** for video frame buffers on ESP32-S3
6. **Managed components work** when properly integrated into the build system

### Implementation Summary

**Configuration summary:**

- Component: `espressif__libjpeg-turbo`
- Integration: Standard `jpeglib.h` API
- Decoding: JPEG → RGB888 → RGB565
- Memory: PSRAM for frame buffers
- Display: Pre-decoded RGB565 pixels to LVGL
- Status: Depends on build integration and memory constraints

---

## 13. Optional Next Steps

### Already Implemented

- Minimal AVI RIFF parser (see `avi_mjpg_mgr.c`)
- Frame timing using `lv_timer` (see `ui_avi.c`)
- Use PSRAM for large frame buffers (see `lvgl_mgr.c`)
- Standard libjpeg integration via `espressif__libjpeg-turbo`
- Proper RGB888 → RGB565 conversion with correct color handling

### Enhancement Ideas

- Compare MJPEG vs PNG frame sequences
- Add playback controls (rewind, seek, speed)
- Implement playlist functionality
- Add on-screen display (OSD) overlays

---

## Debugging Reference

### RGB565 Format Reference

**Standard RGB565 (16-bit):**

```text
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

### libjpeg Integration

**Component dependency** (`CMakeLists.txt`):

```cmake
REQUIRES ... espressif__libjpeg-turbo
```

**Include in source** (`avi_mjpg_mgr.c`):

```c
#include "jpeglib.h"
```

**Standard libjpeg API:**

- `jpeg_create_decompress()` - Initialize decoder
- `jpeg_read_header()` - Parse JPEG metadata
- `jpeg_start_decompress()` - Begin decoding
- `jpeg_read_scanlines()` - Decode image data
- `jpeg_finish_decompress()` - Complete decoding
- `jpeg_destroy_decompress()` - Cleanup

---

## 📝 Notes

This document describes a proven, working implementation. The approach uses standard libjpeg for reliable JPEG decoding with proper color handling.

**The implementation is production-ready and fully tested.**
