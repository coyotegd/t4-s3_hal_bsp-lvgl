# shellcheck shell=bash
# idfsh: ESP-IDF helper shell function with an interactive menu
# - Aggressive full clean (removes build/ and managed_components/)
# - Ad-hoc port/baud selection
# - Build, flash, monitor convenience flows
#
# Usage:
#   source tools/idfsh.sh   # define the function in your shell
#   idfsh                   # run the interactive helper
#
# Optional environment variables before calling idfsh:
#   ESPPORT=/dev/ttyACM0    # serial port to use
#   ESPBAUD=460800          # baud rate for flash/monitor

idfsh() {
  set -o pipefail

  local PROJECT_DIR
  PROJECT_DIR="${PROJECT_DIR:-$PWD}"

  # Defaults (can be overridden via environment before calling idfsh)
  ESPPORT="${ESPPORT:-/dev/ttyACM0}"
  ESPBAUD="${ESPBAUD:-460800}"

  # --- Helpers --------------------------------------------------------------
  ensure_idf() {
    if command -v idf.py >/dev/null 2>&1; then
      return 0
    fi

    local candidates=()
    if [[ -n "${IDF_PATH:-}" ]]; then
      candidates+=("$IDF_PATH/export.sh")
    fi
    candidates+=(
      "$HOME/esp/esp-idf/export.sh"
      "/opt/esp/idf/export.sh"
      "/opt/esp/esp-idf/export.sh"
      "$HOME/.espressif/tools/idf/export.sh"
    )

    for f in "${candidates[@]}"; do
      if [[ -f "$f" ]]; then
        # shellcheck source=/dev/null
        . "$f"
        if command -v idf.py >/dev/null 2>&1; then
          echo "[idfsh] Sourced: $f"
          return 0
        fi
      fi
    done

    echo "[idfsh] idf.py not found. Please source ESP-IDF's export.sh first." >&2
    return 1
  }

  detect_default_port() {
    # Try common Linux device names
    local ports
    ports=(/dev/ttyACM* /dev/ttyUSB*)
    for p in "${ports[@]}"; do
      if [[ -e "$p" ]]; then
        echo "$p"
        return 0
      fi
    done
    return 1
  }

  choose_port() {
    local found=()
    local p
    for p in /dev/ttyACM* /dev/ttyUSB*; do
      [[ -e "$p" ]] && found+=("$p")
    done

    if [[ ${#found[@]} -eq 0 ]]; then
      echo "[idfsh] No serial ports found under /dev/ttyACM* or /dev/ttyUSB*." >&2
      read -r -p "Enter port manually (or leave empty to cancel): " p
      if [[ -n "$p" ]]; then
        ESPPORT="$p"
      fi
      return 0
    fi

    echo "[idfsh] Available ports:"
    local i=1
    for p in "${found[@]}"; do
      printf "  %2d) %s\n" "$i" "$p"
      ((i++))
    done
    read -r -p "Select a port [1-${#found[@]}] (or press Enter for ${found[0]}): " sel
    if [[ -z "$sel" ]]; then
      ESPPORT="${found[0]}"
    elif [[ "$sel" =~ ^[0-9]+$ ]] && (( sel >= 1 && sel <= ${#found[@]} )); then
      ESPPORT="${found[$((sel-1))]}"
    else
      echo "[idfsh] Invalid selection, keeping current: ${ESPPORT:-unset}"
    fi
  }

  set_baud() {
    read -r -p "Enter baud (e.g., 460800, 921600) [current: $ESPBAUD]: " b
    if [[ -n "$b" ]]; then
      if [[ "$b" =~ ^[0-9]+$ ]]; then
        ESPBAUD="$b"
      else
        echo "[idfsh] Invalid baud, keeping $ESPBAUD"
      fi
    fi
  }

  do_fullclean() {
    echo "[idfsh] Aggressive clean in: $PROJECT_DIR"
    if [[ -d "$PROJECT_DIR/build" ]]; then
      rm -rf "$PROJECT_DIR/build"
      echo "  removed build/"
    else
      echo "  build/ not present"
    fi
    if [[ -d "$PROJECT_DIR/managed_components" ]]; then
      rm -rf "$PROJECT_DIR/managed_components"
      echo "  removed managed_components/"
    else
      echo "  managed_components/ not present"
    fi
    # Remove common generated files if present
    local gen_files=(
      "$PROJECT_DIR/compile_commands.json"
      "$PROJECT_DIR/flasher_args.json"
    )
    for gf in "${gen_files[@]}"; do
      if [[ -f "$gf" ]]; then
        rm -f "$gf"
        echo "  removed $(basename "$gf")"
      fi
    done
    echo "[idfsh] Clean done."
  }

  do_build() {
    ensure_idf || return 1
    ( cd "$PROJECT_DIR" && idf.py build ) || return 1
  }

  do_flash() {
    ensure_idf || return 1
    if [[ -z "$ESPPORT" ]]; then
      local d
      if d=$(detect_default_port); then
        ESPPORT="$d"
        echo "[idfsh] Using detected port: $ESPPORT"
      else
        echo "[idfsh] No port set; please choose one."
        choose_port
        [[ -z "$ESPPORT" ]] && { echo "[idfsh] Port not set." >&2; return 1; }
      fi
    fi
    ( cd "$PROJECT_DIR" && idf.py -p "$ESPPORT" -b "$ESPBAUD" flash ) || return 1
  }

  do_monitor() {
    ensure_idf || return 1
    if [[ -z "$ESPPORT" ]]; then
      local d
      if d=$(detect_default_port); then
        ESPPORT="$d"
        echo "[idfsh] Using detected port: $ESPPORT"
      else
        echo "[idfsh] No port set; please choose one."
        choose_port
        [[ -z "$ESPPORT" ]] && { echo "[idfsh] Port not set." >&2; return 1; }
      fi
    fi
    ( cd "$PROJECT_DIR" && idf.py -p "$ESPPORT" -b "$ESPBAUD" monitor ) || return 1
  }

  do_build_flash_monitor() {
    ensure_idf || return 1
    if [[ -z "$ESPPORT" ]]; then
      local d
      if d=$(detect_default_port); then
        ESPPORT="$d"
        echo "[idfsh] Using detected port: $ESPPORT"
      else
        echo "[idfsh] No port set; please choose one."
        choose_port
        [[ -z "$ESPPORT" ]] && { echo "[idfsh] Port not set." >&2; return 1; }
      fi
    fi
    ( cd "$PROJECT_DIR" && idf.py -p "$ESPPORT" -b "$ESPBAUD" build flash monitor ) || return 1
  }

  do_flash_monitor() {
    ensure_idf || return 1
    if [[ -z "$ESPPORT" ]]; then
      local d
      if d=$(detect_default_port); then
        ESPPORT="$d"
        echo "[idfsh] Using detected port: $ESPPORT"
      else
        echo "[idfsh] No port set; please choose one."
        choose_port
        [[ -z "$ESPPORT" ]] && { echo "[idfsh] Port not set." >&2; return 1; }
      fi
    fi
    ( cd "$PROJECT_DIR" && idf.py -p "$ESPPORT" -b "$ESPBAUD" flash monitor ) || return 1
  }

  do_set_target() {
    ensure_idf || return 1
    # Common ESP-IDF targets (ESP-IDF 5.x)
    local targets=(
      esp32 esp32s2 esp32s3 esp32c2 esp32c3 esp32c6 esp32h2 esp32p4 linux
    )
    local current
    current=$(idf.py get-target 2>/dev/null | awk '{print $NF}')
    echo "[idfsh] Available targets:"
    local i=1
    for t in "${targets[@]}"; do
      printf "  %2d) %s\n" "$i" "$t"
      ((i++))
    done
    echo "[idfsh] Current: ${current:-unknown}"
    read -r -p "Select a target by number or enter a name (Enter to keep current): " sel
    local target
    if [[ -z "$sel" ]]; then
      echo "[idfsh] Keeping current target: ${current:-unknown}"
      return 0
    elif [[ "$sel" =~ ^[0-9]+$ ]] && (( sel >= 1 && sel <= ${#targets[@]} )); then
      target="${targets[$((sel-1))]}"
    else
      target="$sel"
    fi
    ( cd "$PROJECT_DIR" && idf.py set-target "$target" ) || return 1
  }

  do_erase_flash() {
    ensure_idf || return 1
    if [[ -z "$ESPPORT" ]]; then
      local d
      if d=$(detect_default_port); then
        ESPPORT="$d"
        echo "[idfsh] Using detected port: $ESPPORT"
      else
        echo "[idfsh] No port set; please choose one."
        choose_port
        [[ -z "$ESPPORT" ]] && { echo "[idfsh] Port not set." >&2; return 1; }
      fi
    fi
    echo "[idfsh] WARNING: erase_flash will wipe the entire chip, including NVS and OTA data."
    read -r -p "Proceed with full flash erase? [y/N]: " ans
    if [[ ! "$ans" =~ ^[Yy]$ ]]; then
      echo "[idfsh] Erase cancelled."
      return 0
    fi
    ( cd "$PROJECT_DIR" && idf.py -p "$ESPPORT" -b "$ESPBAUD" erase_flash ) || return 1
  }

  do_size() {
    ensure_idf || return 1
    local build_dir="$PROJECT_DIR/build"
    # Base size report
    ( cd "$PROJECT_DIR" && idf.py size ) || true

    # Derive app binary path from project_description.json if available
    local json="$build_dir/project_description.json"
    local app_bin=""
    if [[ -f "$json" ]]; then
      app_bin=$(python3 - "$json" <<'PY'
import json,sys
path=sys.argv[1]
try:
    j=json.load(open(path))
    print(j.get("app_bin",""))
except Exception as e:
    print("")
PY
)
    fi
    if [[ -z "$app_bin" ]]; then
      # Fallback: first non-bootloader bin in build dir
      app_bin=$(ls "$build_dir"/*.bin 2>/dev/null | grep -v bootloader | head -n1)
    fi

    local part_bin="$build_dir/partition_table/partition-table.bin"
    if [[ -n "$app_bin" && -f "$part_bin" && -n "${IDF_PATH:-}" && -f "$IDF_PATH/components/partition_table/check_sizes.py" ]]; then
      local out
      out=$("$IDF_PATH/components/partition_table/check_sizes.py" --offset 0x8000 partition --type app "$part_bin" "$app_bin" 2>/dev/null || true)
      echo "$out"
      # Parse sizes from check_sizes.py output
      local used_hex part_hex used part pct status
      used_hex=$(echo "$out" | grep -oE 'binary size 0x[0-9a-fA-F]+' | sed 's/.*0x//' | head -n1)
      part_hex=$(echo "$out" | grep -oE 'Smallest app partition is 0x[0-9a-fA-F]+' | sed 's/.*0x//' | head -n1)
      if [[ -n "$used_hex" && -n "$part_hex" ]]; then
        used=$((16#$used_hex))
        part=$((16#$part_hex))
        pct=$(( used * 100 / part ))
        status="GREEN"
        if (( pct >= 90 )); then
          status="RED"
        elif (( pct >= 75 )); then
          status="YELLOW"
        fi
        echo "[idfsh] App size: ${used} / ${part} bytes (${pct}%). Status: ${status}"
      fi
    else
      echo "[idfsh] Size check: missing app bin or partition table; build the project first."
    fi

    # Optional: chip info via esptool (if port available)
    if command -v esptool.py >/dev/null 2>&1 && [[ -n "$ESPPORT" ]]; then
      echo "[idfsh] Chip info (via esptool):"
      esptool.py --port "$ESPPORT" chip_id || true
    fi
  }

  do_reconfigure() {
    ensure_idf || return 1
    ( cd "$PROJECT_DIR" && idf.py reconfigure ) || return 1
  }

  # --- Menu ----------------------------------------------------------------
  while true; do
    echo
    echo "[idfsh] Project: $PROJECT_DIR"
    echo "[idfsh] Port:    ${ESPPORT:-unset}"
    echo "[idfsh] Baud:    $ESPBAUD"
    echo
    cat <<'MENU'
  1) Full Clean (remove build/ & managed_components/)
  2) Set Port
  3) Set Baud
  4) Build
  5) Flash
  6) Monitor
  7) Build & Flash
  8) Flash & Monitor
  9) Build, Flash & Monitor
 10) Set Target
 11) Erase Flash
 12) Size Report
 13) Reconfigure (CMake regen)
  0) Quit
MENU
    read -r -p "Select an option: " choice
    case "$choice" in
      1) do_fullclean ;; 2) choose_port ;; 3) set_baud ;;
      4) do_build ;; 5) do_flash ;; 6) do_monitor ;;
      7) do_build && do_flash ;; 8) do_flash_monitor ;;
      9) do_build_flash_monitor ;;
     10) do_set_target ;;
     11) do_erase_flash ;;
     12) do_size ;;
     13) do_reconfigure ;;
      0) echo "[idfsh] Bye."; break ;;
      *) echo "[idfsh] Invalid choice." ;;
    esac
  done
}

# If executed directly as a script, run the function once.
if [[ "${BASH_SOURCE[0]}" == "$0" ]]; then
  idfsh "$@"
fi
