# ESP32 MJPEG Video Playback with LVGL

> **Complete AVI/MJPEG video playback implementation for ESP32-S3 using libjpeg and LVGL**

This document describes the fully working AVI/MJPEG video playback system implemented in this project. The system successfully decodes and displays MJPEG video files on the ESP32-S3 with LVGL.

## ✅ System Status: Complete and Working

The AVI/MJPEG player is **fully functional** with:
- ✅ Reliable MJPEG AVI parsing
- ✅ JPEG decoding using standard libjpeg
- ✅ Correct RGB565 color output
- ✅ Smooth playback in LVGL
- ✅ Automatic looping support
- ✅ PSRAM buffering for large frames

## 🎯 Quick Start

### 1. Copy AVI Files to SD Card

Place MJPEG AVI files in the `4_sd_card/` directory and copy them to your physical SD card.

**Sample files included:**
- `fallingcube.avi`
- `spacetime.avi`
- `circletriangle.avi`
- `pulpfictiondance.avi`
- `eye.avi`
- `fractal.avi`
- `hearttunnel.avi`
- `redplasma.avi`
- `starspin.avi`
- `waterrings.avi`

### 2. Use in Your Code

```c
#include "ui_avi.h"

void app_main(void) {
    // ... after bsp_init() and lv_ui_init() ...
    
    lvgl_mgr_lock();
    
    // Create AVI player widget
    lv_obj_t * avi_player = ui_avi_create(lv_screen_active());
    lv_obj_center(avi_player);
    
    // Load file from SD card (S: prefix = /sdcard)
    ui_avi_set_src(avi_player, "S:/fallingcube.avi");
    
    // Start playback
    ui_avi_play(avi_player);
    
    lvgl_mgr_unlock();
}
```

## 📚 API Reference

| Function | Description |
|----------|-------------|
| `ui_avi_create(parent)` | Creates an AVI player widget |
| `ui_avi_set_src(obj, path)` | Loads AVI file ("S:/filename.avi" or "/sdcard/filename.avi") |
| `ui_avi_play(obj)` | Starts/resumes playback |
| `ui_avi_pause(obj)` | Pauses playback |
| `ui_avi_stop(obj)` | Stops playback and rewinds to beginning |

## 🏗️ Architecture

### System Overview

```
AVI File (SD Card)
    ↓
avi_mjpg_open() - Parse RIFF/AVI headers
    ↓
avi_mjpg_get_next_frame() - Extract JPEG chunk
    ↓
libjpeg: jpeg_mem_src() - Setup memory source
    ↓
libjpeg: jpeg_read_header() - Read JPEG headers
    ↓
libjpeg: jpeg_start_decompress() - Begin decoding
    ↓
libjpeg: jpeg_read_scanlines() - Decode to RGB24
    ↓
Convert RGB24 → RGB565 - Manual pixel conversion
    ↓
Return RGB565 buffer (PSRAM)
    ↓
lv_image_set_src() - Display in LVGL
```

### Key Components

**File:** `components/t4s3_bsp/src/avi_mjpg_mgr.c`
- AVI/RIFF container parser
- Locates and extracts MJPEG video chunks
- Standard libjpeg integration
- RGB24 to RGB565 pixel conversion
- PSRAM buffer management

**File:** `components/lv_ui/src/ui_avi.c`
- LVGL widget wrapper
- Frame timing using `lv_timer`
- Automatic looping
- Playback controls

### Decoding Pipeline Details

1. **AVI Parsing**: Simple RIFF parser locates the 'movi' list containing video frames
2. **Frame Extraction**: Identifies video chunks (ending with 'dc') and reads JPEG data
3. **JPEG Decoding**: Uses standard libjpeg to decode to RGB24 (3 bytes per pixel)
4. **Color Conversion**: Converts RGB24 to RGB565 format for LVGL:
   ```c
   rgb565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
   ```
5. **Display**: Wraps RGB565 buffer in `lv_img_dsc_t` and displays via LVGL

### RGB565 Format

Each decoded frame is provided to LVGL as:

```c
lv_img_dsc_t {
    .header.magic = LV_IMAGE_HEADER_MAGIC
    .header.cf = LV_COLOR_FORMAT_RGB565  // Raw pixel data
    .header.w = width
    .header.h = height
    .data = RGB565 buffer pointer (PSRAM)
    .data_size = width * height * 2 bytes
}
```

**LVGL receives pre-decoded RGB565 pixels** - no JPEG decoding happens within LVGL itself.

## 🎬 Creating MJPEG AVI Files

### Basic Conversion

Convert any video format (GIF, MP4, WebM, etc.) to MJPEG AVI:

```bash
ffmpeg -i input.gif \
  -vf scale=320:-1:flags=lanczos \
  -an \
  -vcodec mjpeg \
  -pix_fmt yuvj422p \
  -q:v 8 \
  -f avi \
  output.avi
```

### Flag Explanations

| Flag | Purpose |
|------|---------|
| `-vf scale=320:-1` | Resize to 320px width (maintain aspect ratio) |
| `-an` | Remove audio (ESP32 doesn't support it) |
| `-vcodec mjpeg` | Use Motion JPEG codec (one JPEG per frame) |
| `-pix_fmt yuvj422p` | Baseline JPEG compatible pixel format |
| `-q:v 8` | Quality (1-31, lower = better, 8 is good default) |
| `-f avi` | AVI container format |

### Frame Rate Control

Lower frame rates improve performance and stability:

```bash
ffmpeg -i input.mp4 \
  -vf "fps=10,scale=320:-1" \
  -vcodec mjpeg \
  -pix_fmt yuvj422p \
  -q:v 10 \
  output.avi
```

**💡 Recommendation:** 10-15 FPS is ideal for ESP32-S3.

### Quality vs Size

```bash
# High quality (larger file)
-q:v 3

# Balanced (recommended)
-q:v 8

# Smaller file (lower quality)
-q:v 15
```

## 🔍 Verification

### Check Codec

```bash
ffprobe output.avi
```

**Expected output:**
```
Video: mjpeg (Baseline), yuvj422p
```

### Test a Frame

Extract and verify a single frame:

```bash
ffmpeg -i output.avi -vframes 1 test.jpg
file test.jpg
```

Should show: `JPEG image data, baseline`

## 💾 Memory Considerations

### Resolution Limits (ESP32-S3)

| Resolution | RGB565 Size | Status |
|------------|-------------|--------|
| 240×240 | 115 KB | ✅ Safe |
| 320×240 | 150 KB | ✅ Recommended |
| 400×300 | 234 KB | ✅ Good |
| 480×320 | 300 KB | ⚠️ May work with PSRAM |
| 640×480 | 600 KB | ❌ Too large for most use cases |

### Memory Usage

```
Per Frame Memory:
- JPEG buffer: Variable (typically 20-100 KB)
- RGB565 output: width × height × 2 bytes
- RGB24 temp buffer: width × height × 3 bytes (during decode only)
```

**PSRAM is essential** for video playback. All frame buffers are allocated in PSRAM using `MALLOC_CAP_SPIRAM`.

## ⚙️ Technical Implementation Details

### Why Standard libjpeg?

The implementation uses **standard libjpeg** because:
- ✅ Industry-standard JPEG decoder
- ✅ Handles all JPEG variants reliably
- ✅ Well-tested and robust
- ✅ Complete JPEG compliance
- ✅ Excellent performance on ESP32-S3
- ✅ No dependency conflicts

### Previous Approaches Tried

1. **LVGL's tjpgd** ❌
   - Limited JPEG support
   - Failed on some MJPEG frames
   - Hard to debug

2. **NanoJPEG** ⚠️
   - Lightweight single-file decoder
   - Had RGB/BGR color channel issues
   - Required complete JPEG headers

3. **Standard libjpeg** ✅ **CURRENT**
   - Works perfectly
   - Handles all MJPEG variants
   - Reliable color output

### Color Format

The RGB565 conversion formula:
```c
uint16_t rgb565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
```

**Bit layout:**
```
15 14 13 12 11 | 10 09 08 07 06 05 | 04 03 02 01 00
R  R  R  R  R  | G  G  G  G  G  G  | B  B  B  B  B
```

This produces standard RGB565 format that LVGL expects.

## 🎯 Path Conventions

The AVI player supports two path formats:

1. **S: prefix** (recommended): `"S:/filename.avi"`
   - Automatically maps to `/sdcard/`
   
2. **Full path**: `"/sdcard/filename.avi"`
   - Direct filesystem path

Both formats work identically.

## 🔄 Playback Features

### Automatic Looping

When a video reaches the end:
1. `avi_mjpg_get_next_frame()` returns `ESP_ERR_INVALID_STATE`
2. Player automatically calls `avi_mjpg_rewind()`
3. Playback continues from the first frame

### Frame Timing

- Frame rate is read from AVI file header (`avih` chunk)
- Converted to millisecond delay: `delay = 1000 / fps`
- LVGL timer handles frame updates
- Default fallback: 30 FPS (33ms delay)

### Playback Controls

```c
lv_obj_t *player = ui_avi_create(parent);
ui_avi_set_src(player, "S:/video.avi");

ui_avi_play(player);   // Start playing
ui_avi_pause(player);  // Pause (maintains position)
ui_avi_stop(player);   // Stop and rewind
ui_avi_play(player);   // Resume from pause or restart after stop
```

## 🔧 Advanced Usage

### Custom Frame Rate

The frame rate is automatically read from the AVI file, but you can create custom frame rates during encoding:

```bash
# Create 12 FPS video
ffmpeg -i input.gif -vf "fps=12,scale=320:-1" \
  -vcodec mjpeg -pix_fmt yuvj422p -q:v 8 output.avi
```

### Multiple Videos

You can create multiple AVI player instances:

```c
lv_obj_t *player1 = ui_avi_create(parent);
ui_avi_set_src(player1, "S:/video1.avi");
lv_obj_set_pos(player1, 0, 0);
ui_avi_play(player1);

lv_obj_t *player2 = ui_avi_create(parent);
ui_avi_set_src(player2, "S:/video2.avi");
lv_obj_set_pos(player2, 320, 0);
ui_avi_play(player2);
```

### Sizing and Positioning

The player is a standard LVGL image widget:

```c
lv_obj_t *player = ui_avi_create(parent);
ui_avi_set_src(player, "S:/video.avi");

// Center on screen
lv_obj_center(player);

// Or set specific position
lv_obj_set_pos(player, 10, 20);

// Set size (scales the image)
lv_obj_set_size(player, 300, 200);
```

## 📊 Performance Optimization

### Best Practices

1. **Use appropriate resolution**: 320×240 is a sweet spot
2. **Lower frame rate**: 10-15 FPS is often sufficient
3. **Optimize quality**: `-q:v 8` to `-q:v 12` balances size and quality
4. **Use PSRAM**: Essential for frame buffers
5. **Single player at a time**: Multiple simultaneous players may impact performance

### FFmpeg Optimization Example

```bash
ffmpeg -i input.mp4 \
  -vf "fps=12,scale=320:240" \
  -an \
  -vcodec mjpeg \
  -pix_fmt yuvj422p \
  -q:v 10 \
  -f avi \
  optimized.avi
```

## 🐛 Troubleshooting

### Video doesn't play

1. **Check file format**: Must be MJPEG in AVI container
   ```bash
   ffprobe yourfile.avi
   ```

2. **Verify SD card mount**: File must be accessible at `/sdcard/`

3. **Check ESP logs**: Look for errors from `AVI_MJPG` or `ui_avi` tags

### Colors look wrong

- This should not happen with the current implementation
- If it does, verify the AVI was encoded with `-pix_fmt yuvj422p`
- Check that libjpeg is properly installed

### Playback is choppy

1. **Reduce resolution**: Try 240×240 or 320×240
2. **Lower frame rate**: Use `fps=10` or `fps=12`
3. **Reduce quality**: Use higher `-q:v` value (e.g., `-q:v 12`)
4. **Verify PSRAM**: Make sure PSRAM is enabled in menuconfig

### Out of memory errors

1. **Use lower resolution**: Each pixel requires 2 bytes (RGB565)
2. **Enable PSRAM**: Required for video playback
3. **Close other players**: Free resources before opening new videos

## 📝 File Structure

```
components/
├── t4s3_bsp/
│   ├── include/avi_mjpg_mgr.h    # AVI parser and decoder API
│   └── src/avi_mjpg_mgr.c        # Implementation (libjpeg)
└── lv_ui/
    ├── include/ui_avi.h          # LVGL widget API
    └── src/ui_avi.c              # Widget implementation

4_sd_card/
└── *.avi                         # Sample MJPEG videos

docs/
└── avi_mjpg_lvgl.md             # This documentation
```

## 🎓 Summary

This AVI/MJPEG player provides a complete, working solution for video playback on ESP32-S3 with LVGL:

- ✅ **Complete**: All features implemented and working
- ✅ **Reliable**: Uses standard libjpeg for robust JPEG decoding
- ✅ **Performant**: Optimized for ESP32-S3 with PSRAM
- ✅ **Simple API**: Easy to integrate into LVGL applications
- ✅ **Well-documented**: Clear usage examples and FFmpeg conversion guide

The system successfully plays MJPEG AVI files with correct colors, smooth playback, and automatic looping. It's ready for production use in your ESP32-S3 LVGL applications.
