# ESP-IDF Helper: idfsh

A lightweight interactive helper for common ESP‑IDF tasks: aggressive clean, ad‑hoc port/baud selection, build/flash/monitor flows, CMake reconfigure, and an enhanced size report with status.

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

## Persist Across Sessions

Add `idfsh` to your shell startup so it’s always available:

```bash
echo "source /home/kenneth/test_repos/t4-s3_base-apps/tools/idfsh.sh" >> ~/.bashrc
# then reload
source ~/.bashrc
```

## Alternate Location (Central Install)

If you prefer a single, central location for `idfsh` (outside this repo), you can move it and update your shell to source from there:

```bash
# Create a central tools directory
mkdir -p /home/kenneth/Documents/PlatformIO/PIOesp-idf

# Move the script
mv /home/kenneth/test_repos/t4-s3_base-apps/tools/idfsh.sh \
   /home/kenneth/Documents/PlatformIO/PIOesp-idf/idfsh.sh

# Update ~/.bashrc: remove old line and add the new source line
grep -v '^source /home/kenneth/test_repos/t4-s3_base-apps/tools/idfsh.sh$' ~/.bashrc > ~/.bashrc.tmp && mv ~/.bashrc.tmp ~/.bashrc
echo 'source /home/kenneth/Documents/PlatformIO/PIOesp-idf/idfsh.sh' >> ~/.bashrc

# Reload for the current session
source ~/.bashrc

# Verify
command -v idfsh && echo "idfsh is available"
```

Optional: keep this repo’s convenience by symlinking back:

```bash
ln -sf /home/kenneth/Documents/PlatformIO/PIOesp-idf/idfsh.sh \
	/home/kenneth/test_repos/t4-s3_base-apps/tools/idfsh.sh
```

## Run Directly as a Script

The file includes a main block that runs the menu if executed directly:

```bash
bash tools/idfsh.sh
# or make it executable
chmod +x tools/idfsh.sh
./tools/idfsh.sh
```

## Common Menu Options

- Full Clean: removes `build/` and `managed_components/` plus common generated files
- Set Port / Set Baud: pick serial device and speed
- Build / Flash / Monitor: single or chained flows
- Build & Flash / Flash & Monitor / Build, Flash & Monitor: convenience chains
- Set Target: set ESP‑IDF target (e.g., `esp32s3`)
- Erase Flash: full chip erase (see warning below)
- Size Report: enhanced report with status light (GREEN/YELLOW/RED)
- Reconfigure: runs `idf.py reconfigure` to regenerate CMake and dependencies

### Size Report details
- Runs `idf.py size` and computes app size vs smallest app partition using ESP‑IDF’s `check_sizes.py`.
- Prints: `App size: used / total bytes (percent%). Status: GREEN/YELLOW/RED`.
	- GREEN: under 75% of the smallest app partition
	- YELLOW: 75–89%
	- RED: 90% or more
- If `esptool.py` and `ESPPORT` are available, shows chip info (`esptool.py --port $ESPPORT chip_id`).

### Erase Flash warning
- `erase_flash` wipes the entire flash chip, including NVS and OTA data partitions.
- The menu prompts for confirmation before proceeding.

### Reconfigure
- Runs `idf.py reconfigure` to refresh CMake configuration and component dependencies.

## Troubleshooting

- `idf.py not found`: Ensure ESP‑IDF is exported. If you maintain a custom install, set `IDF_PATH` and source `$IDF_PATH/export.sh`.
- Serial ports missing: Check your device path (e.g., `/dev/ttyACM0`), udev permissions, or try unplug/replug.
