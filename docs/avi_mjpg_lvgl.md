# ESP32 + LVGL MJPEG Video Playback Guide

> **Production-ready AVI MJPEG playback using libjpeg-turbo on ESP32-S3**

This guide shows how to play MJPEG video files on ESP32 using LVGL. The system extracts JPEG frames from AVI containers and displays them using a locally-built libjpeg-turbo decoder.

---

## Quick Start

**Just want to play videos? Jump to [Using the Built-In AVI Player](#using-the-built-in-avi-player)**

---

## Table of Contents

1. [Current Implementation](#current-implementation)
2. [The Journey: What Didn't Work](#the-journey-what-didnt-work)
3. [How to Convert Videos](#how-to-convert-videos)
4. [Using the Built-In AVI Player](#using-the-built-in-avi-player)
5. [Architecture Overview](#architecture-overview)
6. [Technical Details](#technical-details)
7. [Performance Guidelines](#performance-guidelines)

---

## Current Implementation

### ✅ What's Working Now

**Decoder**: Local build of **libjpeg-turbo 3.0.4** using standard libjpeg API

**Status**: **PRODUCTION READY** - Reliable MJPEG playback at ~15 FPS

**Key Features**:
- ✅ Full JPEG compliance (handles all MJPEG formats)
- ✅ Efficient decoding (hardware-optimized where possible)
- ✅ Clean RGB24 → RGB565 conversion
- ✅ Proper PSRAM usage for frame buffers
- ✅ No CMake version conflicts (works with CMake 3.31+)
- ✅ No color format issues

**Architecture**:
```
AVI File (SD Card)
    ↓
RIFF/AVI Parser (avi_mjpg_mgr.c)
    ↓
Extract JPEG frame
    ↓
libjpeg-turbo decode → RGB24 pixels
    ↓
Convert RGB24 → RGB565
    ↓
Display in LVGL (ui_avi.c)
```

**Why Local Build?**

The managed `espressif/libjpeg-turbo` component broke with CMake 3.31+ updates. A local component override builds libjpeg-turbo from source, providing the same working standard libjpeg API without managed component dependency issues.

See `components/lv_ui/src/README.md` for the complete journey and technical details.

---

## The Journey: What Didn't Work

Before reaching the current solution, several JPEG decoders were attempted:

### 1. ❌ LVGL's tjpgd (TinyJPEG Decoder)

**Issue**: Failed to decode MJPEG AVI frames reliably
- Inconsistent handling of embedded vs missing headers
- Limited format support
- Difficult to debug failures

### 2. ⚠️ Managed espressif/libjpeg-turbo Component

**Status**: Worked initially, then broke with CMake updates

**What happened**:
- CMake 3.31+ stopped supporting `cmake_minimum_required(VERSION 2.8.12)`
- Managed component's CMakeLists.txt used old CMake version
- Component manager overwrites edits on reconfigure
- Build system broke completely

### 3. ❌ NanoJPEG (Lightweight Single-File Decoder)

**Issue**: Color format problems and limited JPEG support
- Decoded frames but colors were swapped (red appeared as green)
- Required complete JPEG headers (DHT/DQT)
- Less robust than full libjpeg implementation
- Abandoned in favor of fixing libjpeg-turbo build

### 4. ✅ Local libjpeg-turbo Build (Current Solution)

**Result**: SUCCESS - Back to the working implementation

Built libjpeg-turbo from source as a local component:
- Uses `ExternalProject_Add` to download and build v3.0.4
- Standard libjpeg API (same as before, no code changes)
- Works with modern CMake versions
- No dependency on managed components
- Full control over build configuration

---

## How to Convert Videos

### Requirements

- **FFmpeg** installed on your development machine
- Input video (GIF, MP4, WebM, etc.)
- Target: ESP32-S3 with AMOLED display

### Conversion Command

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

### Flag Explanations

| Flag | Purpose |
|------|---------|
| `-vf scale=320:-1` | Resize width to 320px, maintain aspect ratio |
| `-an` | Remove audio (ESP32 can't play audio from AVI) |
| `-vcodec mjpeg` | Use Motion JPEG codec (each frame = JPEG) |
| `-pix_fmt yuvj422p` | Baseline JPEG compatible pixel format |
| `-q:v 8` | Quality setting (1-31, lower = better, 8 = good balance) |
| `-f avi` | Output format: AVI container |

### Frame Rate Recommendations

For better performance, limit frame rate during conversion:

```bash
ffmpeg -i input.gif \
  -vf "fps=10,scale=320:-1:flags=lanczos" \
  -vcodec mjpeg \
  -pix_fmt yuvj422p \
  -q:v 10 \
  -an \
  -f avi \
  output_mjpeg.avi
```

**Recommended**: 10-12 FPS for ESP32 (smooth playback, manageable file size)

### Verify the Output

Check that conversion worked correctly:

```bash
ffprobe output_mjpeg.avi
```

**Expected output**:
```
Video: mjpeg (Baseline), yuvj422p
```

---

## Using the Built-In AVI Player

### 📁 Step 1: Copy AVI Files to SD Card

1. Convert your video using the instructions above
2. Place the `.avi` file in the `4_sd_card/` directory
3. Copy to your physical SD card

**Sample files included**:
- `fallingcube.avi`
- `spacetime.avi`
- `circletriangle.avi`
- `pulpfictiondance.avi`

### 💻 Step 2: Add Code to Your Application

```c
#include "ui_avi.h"  // Add to includes

void app_main(void) {
    // ... after bsp_init() and lv_ui_init() ...
    
    lvgl_mgr_lock();
    
    // Create AVI player widget
    lv_obj_t * avi_player = ui_avi_create(lv_screen_active());
    lv_obj_center(avi_player);  // Center on screen
    
    // Load file from SD card (S: prefix maps to /sdcard)
    ui_avi_set_src(avi_player, "S:/fallingcube.avi");
    
    // Start playback
    ui_avi_play(avi_player);
    
    lvgl_mgr_unlock();
    
    // ... rest of your code ...
}
```

### 📚 API Functions

| Function | Description |
|----------|-------------|
| `ui_avi_create(parent)` | Creates an AVI player widget |
| `ui_avi_set_src(obj, path)` | Loads AVI file from path |
| `ui_avi_play(obj)` | Starts playback |
| `ui_avi_pause(obj)` | Pauses playback |
| `ui_avi_stop(obj)` | Stops and rewinds to beginning |

### 🗂️ Path Convention

Use **`"S:/filename.avi"`** format:
- `S:` prefix maps to `/sdcard` mount point
- Alternatively: `"/sdcard/filename.avi"`

---

## Architecture Overview

### System Design

```
┌─────────────────────────────────────────────────────────┐
│                      Application                        │
│                       (ui_avi.c)                        │
└────────────────────┬────────────────────────────────────┘
                     │ ui_avi_set_src()
                     │ ui_avi_play()
                     ↓
┌─────────────────────────────────────────────────────────┐
│                  AVI/MJPEG Manager                      │
│                  (avi_mjpg_mgr.c)                       │
│  ┌─────────────────────────────────────────────────┐   │
│  │  1. Parse RIFF/AVI headers                      │   │
│  │  2. Find 'movi' chunk with video frames         │   │
│  │  3. Extract JPEG frame data                     │   │
│  └─────────────────────────────────────────────────┘   │
└────────────────────┬────────────────────────────────────┘
                     │ JPEG data
                     ↓
┌─────────────────────────────────────────────────────────┐
│               libjpeg-turbo Decoder                     │
│          (local build, components/espressif__*)         │
│  ┌─────────────────────────────────────────────────┐   │
│  │  1. jpeg_mem_src() - Read from memory          │   │
│  │  2. jpeg_read_header() - Parse JPEG headers    │   │
│  │  3. jpeg_start_decompress() - Begin decode     │   │
│  │  4. jpeg_read_scanlines() - Get RGB24 pixels   │   │
│  └─────────────────────────────────────────────────┘   │
└────────────────────┬────────────────────────────────────┘
                     │ RGB24 pixels
                     ↓
┌─────────────────────────────────────────────────────────┐
│               RGB24 → RGB565 Conversion                 │
│  ┌─────────────────────────────────────────────────┐   │
│  │  rgb565[i] = ((r & 0xF8) << 8) |               │   │
│  │              ((g & 0xFC) << 3) |               │   │
│  │              (b >> 3);                         │   │
│  └─────────────────────────────────────────────────┘   │
└────────────────────┬────────────────────────────────────┘
                     │ RGB565 buffer
                     ↓
┌─────────────────────────────────────────────────────────┐
│                    LVGL Display                         │
│  ┌─────────────────────────────────────────────────┐   │
│  │  lv_img_dsc_t {                                │   │
│  │    .header.cf = LV_COLOR_FORMAT_RGB565         │   │
│  │    .data = rgb565_buffer                       │   │
│  │  }                                             │   │
│  │  lv_image_set_src(obj, &img_dsc)              │   │
│  └─────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────┘
```

### Key Components

1. **ui_avi.c** (User Interface Layer)
   - LVGL widget for video playback
   - Timer-based frame delivery
   - Auto-loop on EOF
   - Cache management

2. **avi_mjpg_mgr.c** (Decoder Manager)
   - RIFF/AVI container parser
   - JPEG frame extraction
   - libjpeg-turbo integration
   - RGB24 → RGB565 conversion
   - PSRAM buffer management

3. **libjpeg-turbo** (Local Component)
   - Built from source (v3.0.4)
   - Standard libjpeg API only
   - ESP32 toolchain integration
   - No managed component dependencies

---

## Technical Details

### Memory Management

**PSRAM Allocation** (for large buffers):

```c
// Frame buffer for JPEG data
ctx->frame_buffer = heap_caps_malloc(32 * 1024, 
                                     MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

// RGB565 output buffer
ctx->rgb_buffer = heap_caps_malloc(width * height * 2, 
                                   MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
```

**Dynamic Resizing**:
- Frame buffer grows automatically for larger frames
- RGB565 buffer sized based on video dimensions

### JPEG Decoding Flow

```c
// 1. Set memory source
jpeg_mem_src(&cinfo, jpeg_data, data_size);

// 2. Read JPEG header
jpeg_read_header(&cinfo, TRUE);

// 3. Configure output
cinfo.out_color_space = JCS_RGB;  // RGB24 output

// 4. Start decompression
jpeg_start_decompress(&cinfo);

// 5. Read scanlines
while (cinfo.output_scanline < cinfo.output_height) {
    jpeg_read_scanlines(&cinfo, row_pointer, 1);
}

// 6. Finish
jpeg_finish_decompress(&cinfo);
```

### RGB565 Color Format

**Standard RGB565 packing** (16-bit):
```
Bit:  15 14 13 12 11 | 10 09 08 07 06 05 | 04 03 02 01 00
      R  R  R  R  R  | G  G  G  G  G  G  | B  B  B  B  B
```

**Conversion from RGB24**:
```c
uint16_t rgb565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
```

### LVGL Integration

**Pre-decoded Pixels Approach**:

LVGL receives RGB565 pixels, NOT JPEG data:

```c
lv_img_dsc_t img_dsc = {
    .header.magic = LV_IMAGE_HEADER_MAGIC,
    .header.cf = LV_COLOR_FORMAT_RGB565,  // Raw pixels
    .header.w = width,
    .header.h = height,
    .data = rgb565_buffer,  // Pointer to decoded pixels
    .data_size = width * height * 2
};

lv_image_set_src(obj, &img_dsc);
```

**Why LVGL decoders are disabled**:

```ini
# sdkconfig.defaults
CONFIG_LV_USE_TJPGD=n
CONFIG_LV_USE_LIBJPEG_TURBO=n
```

JPEG decoding happens externally in `avi_mjpg_mgr.c`, so LVGL doesn't need its own decoders.

---

## Performance Guidelines

### Resolution Recommendations (ESP32-S3)

| Resolution | Status | Notes |
|------------|--------|-------|
| 240×240 | ✅ Safe | Smooth playback |
| 320×240 | ✅ Recommended | Good balance |
| 480×320 | ⚠️ Risky | May be sluggish |
| 600×400 | ❌ Too large | Memory issues |

### Memory Calculation

```
RGB565 buffer = width × height × 2 bytes

Examples:
  320×240 = 153,600 bytes (~150 KB)
  480×320 = 307,200 bytes (~300 KB)
```

**Note**: JPEG buffer + RGB565 buffer + LVGL heap must fit in available memory.

### Frame Rate Guidelines

| FPS | Experience | Use Case |
|-----|------------|----------|
| 10-12 | Smooth | Recommended for ESP32 |
| 15 | Good | Maximum practical |
| 20+ | Choppy | Decode can't keep up |

**Best Practice**: Convert videos at 10-12 FPS for optimal performance.

### Quality vs Size Trade-off

FFmpeg quality parameter (`-q:v`):

| Value | Quality | File Size | Notes |
|-------|---------|-----------|-------|
| 2 | Excellent | Very large | Overkill for ESP32 display |
| 8 | Good | Balanced | **Recommended** |
| 15 | Fair | Small | Visible artifacts |
| 20+ | Poor | Very small | Not recommended |

---

## Troubleshooting

### Video Won't Play

**Check file format**:
```bash
ffprobe your_video.avi
```

Should see: `Video: mjpeg (Baseline)`

**Check SD card mounting**:
- File should be accessible at `/sdcard/filename.avi`
- Use `S:/` prefix in code

### Colors Look Wrong

If colors appear incorrect, check:

1. Display driver color format (should be RGB565)
2. Byte order setting in display driver
3. `LV_COLOR_16_SWAP` in lv_conf.h

**Note**: Current implementation uses standard RGB565 format and works correctly with RM690B0 display.

### Choppy Playback

**Solutions**:
- Reduce resolution (use 320×240 or smaller)
- Lower frame rate (10-12 FPS)
- Increase quality parameter (`-q:v 10` or higher)
- Check SD card read speed

### Memory Errors

**Symptoms**: Playback fails or crashes

**Solutions**:
- Reduce video resolution
- Check PSRAM is enabled in sdkconfig
- Verify buffer allocations succeed

---

## What You Do NOT Need to Do

❌ Configure LVGL's tjpgd decoder (bypassed)  
❌ Configure LVGL's libjpeg-turbo (bypassed)  
❌ Manually add JPEG headers (libjpeg handles it)  
❌ Decode MPEG/H.264 on ESP32 (impossible)  
❌ Fight with managed component versions (local build)

### What IS Happening Behind the Scenes

✅ **Local libjpeg-turbo build** from source (v3.0.4)  
✅ **Standard libjpeg API** decodes JPEG → RGB24  
✅ **Automatic header handling** (no injection needed)  
✅ **RGB24 → RGB565 conversion** in avi_mjpg_mgr.c  
✅ **LVGL displays pre-decoded pixels** (no decoding in LVGL)  
✅ **PSRAM buffers** prevent SRAM exhaustion

---

## Additional Resources

### Related Documentation

- **components/lv_ui/src/README.md** - Complete decoder journey and technical details
- **components/espressif__libjpeg-turbo/CMakeLists.txt** - Local build configuration

### External Tools

- **FFmpeg**: https://ffmpeg.org/
- **libjpeg-turbo**: https://libjpeg-turbo.org/

---

## Lessons Learned

1. **Managed components can break** with toolchain updates (CMake version conflicts)
2. **Standard libjpeg API is robust** and fully ESP-IDF compatible
3. **Local component overrides** provide control when managed components fail
4. **ExternalProject_Add** enables building third-party libraries from source
5. **Pre-decoding frames** avoids LVGL decoder limitations
6. **PSRAM is essential** for video frame buffers on ESP32-S3
7. **Multiple decoder attempts may be needed** before finding the right solution

---

## License & Credits

- **libjpeg-turbo**: BSD-style license, copyright by libjpeg-turbo contributors
- **AVI/MJPEG implementation**: Part of t4-s3_hal_bsp-lvgl project
- **LVGL**: MIT license

---

**Last Updated**: February 2026  
**Status**: Production Ready  
**Tested On**: ESP32-S3, ESP-IDF 5.x, LVGL 9.0
