# ESP-IDF Helper: idfsh

A lightweight interactive helper for common ESP‑IDF tasks: aggressive clean, ad‑hoc port/baud selection, build/flash/monitor flows, CMake reconfigure, and an enhanced size report with visual "Layman Analysis".

## Quick Start

From the repository root:

```bash
source tools/idfsh.sh
idfsh
```

If `idf.py` isn’t on PATH, `idfsh` will try to source ESP‑IDF’s `export.sh` from common locations. You can also source it manually:

```bash
. "$HOME/esp/esp-idf/export.sh"
```

## Menu Options

The interactive menu provides the following streamlined workflow:

1. **Set Port** - Choose or auto-detect serial port.
2. **Set Baud** - Change baud rate (default 460800).
3. **Reconfigure** - **Deep Clean**: Deletes `build/` AND `managed_components/`, then runs `idf.py reconfigure` which recreates both. Use this to fix dependency issues.
4. **Delete Build** - Deletes only the `build/` directory.
5. **Build** - Compiles the project.
6. **Flash** - Flashes firmware to the device.
7. **Monitor** - Opens serial monitor.
8. **Build & Flash** - Combines steps 5 and 6.
9. **Flash & Monitor** - Combines steps 6 and 7.
10. **Build, Flash & Monitor** - One-click build, flash, and monitor.
11. **Set Target** - Change chip target (e.g., esp32s3, esp32c3).
12. **Erase All of Flash (even NVS)** - Wipes the entire chip, including non-volatile storage and OTA data.
13. **Size Report** - Shows RAM and Flash usage with a **"Layman Analysis"** including progress bars and clear explanations.
0. **Quit** - Exit the helper.

## Overrides (Port & Baud)

You can override the serial port and baud on the command line or via environment:

```bash
ESPPORT=/dev/ttyACM0 ESPBAUD=921600 idfsh
# or persist for the shell session
export ESPPORT=/dev/ttyACM0
export ESPBAUD=921600
idfsh
```

When not set, `idfsh` will attempt to auto‑detect under `/dev/ttyACM*` and `/dev/ttyUSB*` and otherwise prompt you to choose.
Default port: `/dev/ttyACM0`. Default baud: `460800`.

## Installation

### Recommended: Install to ~/.local/bin/

For a clean, project-independent installation that's available in all bash shells:

```bash
# Create the directory if it doesn't exist
mkdir -p ~/.local/bin

# Copy the script
cp tools/idfsh.sh ~/.local/bin/

# Add to your .bashrc (one-time setup)
echo 'source ~/.local/bin/idfsh.sh' >> ~/.bashrc

# Reload your shell configuration
source ~/.bashrc
```

Now `idfsh` is available from any directory in any terminal.

### Alternative: Source from Project

If you prefer to source it directly from the project directory:

```bash
# From this project's root
echo "source $PWD/tools/idfsh.sh" >> ~/.bashrc
source ~/.bashrc
```

**Note:** This ties the function to this specific project path. If you move or delete the project, you'll need to update your `.bashrc`.
