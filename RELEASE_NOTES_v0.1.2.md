# Release Notes: v0.1.2

## UI Improvements and Power Management Enhancements

### 🎨 User Interface

- **PM Status Page**: Redesigned with clean table layout (left-aligned labels, right-aligned values)
- **Keyboard Styling**: White text on all keys for better visibility
- **WiFi/Time Sync**: Progressive status messages ("waiting for Wi-Fi" → "http d/t syncing" → actual time)

### ⚡ Power Management

- **Intelligent First Boot**: BC1.2 PA 1.5A (1500mA) with 1000mA fast charge, 350mA system load, 100mA safety margin
- **PM Settings**: Display actual Pre-Charge and Termination current values from PMIC registers
- **Context-Aware Defaults**: PM Defaults button adapts to selected battery capacity and USB source type

### 📚 Documentation

- **PM Settings Guide**: Comprehensive explanation of first boot behavior, defaults, and adaptive charging
- **AVI MJPEG Technical Docs**: Complete journey from managed components to local build solution (`components/lv_ui/src/README.md`)
- **AVI MJPEG User Guide**: Video conversion workflow, FFmpeg commands, API usage (`docs/avi_mjpg_lvgl.md`)

### 🏗️ Architecture

- **BSP Component**: Restructured with proper `include/` and `src/` directory layout
- **libjpeg-turbo**: Relocated to local component with ExternalProject_Add for CMake 3.31+ compatibility
- **AVI/MJPEG Manager**: New component for production-ready MJPEG video playback

### 🔧 Technical

- Removed deprecated managed component files
- Updated media files (PNG format for static images)
- Synchronized with base-apps parent repository

### 📦 Upgrading

```bash
git pull && git checkout v0.1.2
git submodule update --init --recursive
idf.py fullclean && idf.py build
idf.py -p /dev/ttyUSB0 flash
```

**Breaking Changes**: None - fully backward compatible

**Requirements**: ESP-IDF v5.x, CMake 3.5+, LilyGo T4-S3 board

**Full Changelog**: https://github.com/coyotegd/t4-s3_hal_bsp-lvgl/compare/v0.1.1...v0.1.2
