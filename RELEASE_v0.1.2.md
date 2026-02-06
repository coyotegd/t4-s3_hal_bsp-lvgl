# Release v0.1.2: UI Improvements and Power Management Enhancements

**Release Date**: February 6, 2026  
**Tag**: v0.1.2  
**Type**: Feature Release

---

## Overview

This release brings significant improvements to the user interface, power management system, and documentation. Key highlights include a redesigned PM Status page with table layout, intelligent power management defaults, progressive WiFi/time synchronization status messages, and comprehensive documentation for AVI/MJPEG video playback.

---

## 🎨 User Interface Improvements

### PM Status Page Redesign

- **Table Layout**: Refactored to clean table format with:
  - Left-justified field descriptions
  - Right-justified values
  - Consistent spacing and alignment
- **Enhanced Visibility**: All system power metrics displayed in organized, scannable format
- **Real-time Updates**: Live values for voltages, currents, and charging status

### Keyboard Styling

- **Improved Contrast**: White text on all keys including edge control buttons
- **Better Visibility**: Enhanced readability across all keyboard layouts
- **Consistent Design**: Unified styling throughout the UI

### WiFi/Time Sync Status

- **Progressive Status Messages**: Clear feedback during connection and sync process:
  1. "waiting for Wi-Fi connection" - Initial state
  2. "http d/t syncing" - Fetching time from internet
  3. Actual time display - Once synchronized
- **User-Friendly**: Eliminates confusion about sync status

---

## ⚡ Power Management Enhancements

### Intelligent First Boot Defaults

- **Optimized Charging**: BC1.2 PA 1.5A (1500mA input limit)
- **Practical Charging Current**: 1000mA fast charge on first boot
- **Reduced Safety Margin**: 100mA (down from previous default)
- **Balanced System Load**: 350mA allocation for ESP32-S3 + display + WiFi

### PM Settings Improvements

- **Actual PMIC Values**: Display real Pre-Charge and Termination current values read from PMIC registers
- **Context-Aware Defaults**: PM Defaults button now adapts to:
  - Selected battery capacity
  - Selected USB source type
  - Calculates optimal charging currents based on actual hardware configuration
- **Adaptive Charging Policy**: Automatically adjusts charging parameters based on available headroom

---

## 📚 Documentation

### New Comprehensive Guides

1. **PM Settings & Intelligent Defaults** (README.md)
   - First boot behavior explanation
   - PM Defaults button context-awareness
   - Adaptive charging policy details
   - User workflow guidance
   - Safety features documentation

2. **AVI MJPEG Playback Guide** (components/lv_ui/src/README.md)
   - Complete technical documentation
   - Journey from managed components to local build solution
   - CMake version conflict resolution
   - ExternalProject_Add integration details
   - Build system architecture

3. **AVI MJPEG User Guide** (docs/avi_mjpg_lvgl.md)
   - End-to-end video conversion workflow
   - FFmpeg conversion commands
   - API usage examples
   - Performance guidelines
   - Troubleshooting section

---

## 🏗️ Component Architecture

### Restructured BSP Component

- **Proper Directory Layout**:
  - `include/` - Public headers
  - `src/` - Implementation files
- **Better Organization**: Cleaner component structure following ESP-IDF best practices

### libjpeg-turbo Integration

- **Relocated to `components/`**: Moved from managed_components for better control
- **ExternalProject_Add Build**: Downloads and builds libjpeg-turbo 3.0.4 from source
- **CMake 3.31+ Compatible**: Resolved cmake_minimum_required version conflicts
- **Local Component Override**: Avoids managed component dependency issues

### AVI/MJPEG Manager

- **New Component**: `components/t4s3_bsp/src/avi_mjpg_mgr.c`
- **Full JPEG Compliance**: Standard libjpeg API integration
- **PSRAM Optimization**: Efficient memory management for video playback
- **RGB565 Conversion**: Clean implementation with no color format issues

### Removed Deprecated Files

- Cleaned up old managed libjpeg-turbo component files
- Removed deprecated lvgl_mgr files from root level

---

## 🔧 Technical Details

### Build System

- **CMake Improvements**: Better ExternalProject configuration
- **Toolchain Integration**: Proper ESP-IDF toolchain flags propagation
- **Component Dependencies**: Cleaned up and optimized

### Media Files

- **Updated Sample Images**: 
  - Replaced `marauder.jpg` with `marauder.png`
  - Replaced `twist_face.jpg` with `twistface.png`
- **Better Formats**: PNG for static images, AVI/MJPEG for video

### Configuration

- **VS Code Settings**: Updated for better portability
- **Dependencies**: Updated dependencies.lock
- **Component Registry**: Synced with ESP Component Registry

---

## 🔄 Synchronization

All changes have been synchronized with the base-apps parent repository, ensuring consistency across both HAL/BSP and application codebases.

---

## 📦 What's Included

### Key Files Changed (66 files)

- **UI Components**: 10 files updated
- **Power Management**: 5 files enhanced
- **Documentation**: 3 new comprehensive guides
- **Component Architecture**: 8 files restructured
- **Build System**: 5 files updated
- **libjpeg-turbo**: Full local component implementation

### Lines of Code

- **Additions**: Significant documentation and feature additions
- **Deletions**: Removed deprecated managed component files
- **Net Impact**: Cleaner, better-documented codebase

---

## 🚀 Upgrading from v0.1.1

### Breaking Changes

**None** - This is a backward-compatible release.

### Recommended Steps

1. **Pull Latest Code**:
   ```bash
   git pull
   git checkout v0.1.2
   ```

2. **Update Submodules** (if applicable):
   ```bash
   git submodule update --init --recursive
   ```

3. **Clean Build**:
   ```bash
   idf.py fullclean
   idf.py build
   ```

4. **Flash Firmware**:
   ```bash
   idf.py -p /dev/ttyUSB0 flash
   ```

### Configuration Changes

- **No Manual Changes Required**: All defaults are intelligent and self-configuring
- **Optional**: Review PM Settings page to verify power configuration matches your hardware

---

## 📋 Requirements

### Hardware

- **LilyGo T4-S3** (2.41" AMOLED) development board
- **SD Card**: For video playback feature
- **USB Type-C Cable**: For power and programming

### Software

- **ESP-IDF**: v5.x
- **CMake**: 3.5+ (tested with 3.31+)
- **Python**: 3.x (for ESP-IDF)
- **FFmpeg**: (Optional) For video conversion

---

## 🐛 Known Issues

**None** - All known issues from v0.1.1 have been resolved.

---

## 🎯 What's Next (v0.1.3 Roadmap)

Potential improvements for future releases:

- [ ] Additional UI themes and customization options
- [ ] Enhanced video playback controls (seek, speed adjustment)
- [ ] More power management profiles
- [ ] Bluetooth implementation
- [ ] Additional sensor integrations

---

## 📝 Commit History

### Major Commits (v0.1.1 → v0.1.2)

```
19de6af Release v0.1.2: UI improvements and power management enhancements
c1ccf93 Add USB Type-C voltage/version selectors, portable settings, and ESP-IDF helper tools
f7fbb93 gitignore: ignore sdkconfig; rely on sdkconfig.defaults
ee74457 docs: add troubleshooting section for hardcoded paths and clone setup
0999845 vscode: make settings portable (remove hardcoded IDF/tool paths)
3c8deae lv_ui: add global PM Status labels for Pre-Charge and Termination currents
de52046 Update sdkconfig.defaults with stability fixes
f53dbd3 Fix: Wi-Fi scan safety and UI styling updates
d0ad433 docs: Update AVI playback details in README
```

---

## 🙏 Acknowledgments

Special thanks to the ESP-IDF, LVGL, and libjpeg-turbo communities for their excellent libraries and documentation.

---

## 📞 Support

- **Issues**: https://github.com/coyotegd/t4-s3_hal_bsp-lvgl/issues
- **Discussions**: https://github.com/coyotegd/t4-s3_hal_bsp-lvgl/discussions
- **Documentation**: See README.md and docs/ directory

---

## 📄 License

This project maintains the same license as v0.1.1. See LICENSE file for details.

---

**Full Changelog**: https://github.com/coyotegd/t4-s3_hal_bsp-lvgl/compare/v0.1.1...v0.1.2
