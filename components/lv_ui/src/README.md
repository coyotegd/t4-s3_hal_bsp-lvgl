# AVI MJPEG Journey: A Tale of JPEG Decoders

## The Initial Success

AVI MJPEG playback initially worked perfectly using **standard libjpeg API** from the managed `espressif/libjpeg-turbo` component. The code decoded JPEG frames cleanly without issues.

## The CMake Upgrade Breakage

Then a CMake upgrade to 3.31+ broke everything:

```text
CMake Error at CMakeLists.txt:1 (cmake_minimum_required):
  CMake 3.31 does not support cmake_minimum_required lower than 3.5
```

The managed component's CMakeLists.txt specified `cmake_minimum_required(VERSION 2.8.12)`, which CMake 3.31+ no longer supports. Editing `managed_components/espressif__libjpeg-turbo/CMakeLists.txt` was futile—the component manager would re-download and overwrite any changes on reconfigure.

## Failed Alternative Attempts

Facing a broken build, we tried multiple other JPEG decoder libraries:

1. **LVGL's built-in JPEG decoder** - Failed to decode MJPEG frames properly
2. **TJPGD (TinyJPEG Decoder)** - Incompatible with AVI MJPEG format
3. **Another lightweight decoder** - Missing features needed for MJPEG

All alternatives either couldn't handle MJPEG frames or lacked necessary decoding capabilities.

## Why Not TurboJPEG?

While libjpeg-turbo includes TurboJPEG API, we discovered it's incompatible with ESP-IDF:

```text
undefined reference to `__wrap_longjmp'
```

TurboJPEG's error handling uses `setjmp`/`longjmp`, which conflicts with ESP-IDF's linker wrapping. Since **standard libjpeg API** was already working before the CMake issue, there was no reason to pursue TurboJPEG.

## The Solution: Build from Source with Local Component Override

After extensive investigation, we created a local component override that builds libjpeg-turbo from source, restoring the working standard libjpeg API functionality:

1. **Downloads libjpeg-turbo from GitHub** during build (v3.0.4)
2. **Builds only standard libjpeg** (disables TurboJPEG API entirely)
3. **Uses standard libjpeg API** (same as before, no code changes needed)
4. **Integrates seamlessly** with ESP-IDF's toolchain despite CMake 3.31+

### How It Works

The solution lives in `components/espressif__libjpeg-turbo/CMakeLists.txt`:

**Key Components:**

- **ExternalProject_Add:** Downloads and builds libjpeg-turbo 3.0.4 from GitHub during compilation
- **`WITH_TURBOJPEG=FALSE`:** Disables the incompatible TurboJPEG API
- **Custom Install:** Copies built library and headers to a known location
- **Target Linking:** Links the freshly-built `libjpeg.a` to the component

**Build Process:**

1. During CMake configuration, `ExternalProject_Add` sets up the download
2. During build, it clones libjpeg-turbo from GitHub
3. Configures with ESP-IDF's toolchain and Xtensa compiler flags
4. Builds only the standard libjpeg static library
5. Installs headers (`jpeglib.h`, `jmorecfg.h`, etc.) and `libjpeg.a`
6. Makes them available to other components

**Code:**

The `avi_mjpg_mgr.c` already used standard libjpeg API (`jpeg_create_decompress()`, `jpeg_read_header()`, `jpeg_read_scanlines()`), so no code changes were needed—just fixed the build system to compile the library again.

Previously, we had included fallback header injection code (DHT/DQT tables) for MJPEG frames with missing headers. Since standard libjpeg handles this internally, we were able to remove ~180 unnecessary lines, reducing the file from ~430 to ~260 lines.

## Why This Works

**Standard libjpeg API:**

- No setjmp/longjmp in the public API
- Error handling uses callbacks instead
- Fully compatible with ESP-IDF's linker wrapping
- Handles MJPEG frames with missing headers automatically

**Local Component Override:**

- Takes precedence over managed components
- Component manager doesn't overwrite it
- Complete control over build configuration
- Can update by changing `GIT_TAG` in CMakeLists.txt

## The Result

✅ **Working again** with modern CMake 3.31+  
✅ **No managed component dependency** issues  
✅ **AVI MJPEG playback works** at ~15 FPS (same as before)  
✅ **Cleaner code** (removed 180 lines of redundant header injection)  
✅ **Binary size:** 1.9 MB (54% free space)

## Lessons Learned

1. **Managed components** are convenient but can break with toolchain updates  
2. **TurboJPEG API** is incompatible with ESP-IDF (setjmp/longjmp wrapping)  
3. **Standard libjpeg API** is portable and fully ESP-IDF compatible  
4. **ExternalProject_Add** enables building third-party libraries from source  
5. **Local component overrides** provide full control when managed components fail
