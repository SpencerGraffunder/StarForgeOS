#!/usr/bin/env bash
# Flash StarForgeOS into the ota_1 slot (0x1A0000) for dual-boot with
# Hertz Hunter. PlatformIO always uploads to the ota_0 offset, so this
# uses esptool directly at the correct address.
#
# Usage: ./flash_dualboot.sh [serial-port]
#   serial-port defaults to auto-detect (single ESP32 connected)
#
# Prereq: Hertz Hunter was flashed first (it installs bootloader + the
# shared partition table). Verify with:
#   esptool.py --port <port> read_flash 0x1A0000 0x40 | xxd | head -1
set -euo pipefail

cd "$(dirname "$0")"
PORT="${1:--p}"

# Find a recent build (S3 preferred if present, else C3)
if [ -f .pio/build/nuclearcounter_s3/firmware.bin ]; then
  FW=.pio/build/nuclearcounter_s3/firmware.bin
  echo "Flashing: $FW ($(stat -f%z "$FW") bytes)"
elif [ -f .pio/build/nuclearcounter/firmware.bin ]; then
  FW=.pio/build/nuclearcounter/firmware.bin
  echo "Flashing: $FW ($(stat -f%z "$FW") bytes)"
else
  echo "No firmware found. Build first: pio run -e nuclearcounter (or nuclearcounter_s3)"
  exit 1
fi

# Locate esptool (PlatformIO's copy)
if command -v esptool >/dev/null 2>&1; then
  EOPT=esptool
else
  EOPT="$HOME/.platformio/packages/tool-esptoolpy/esptool.py"
fi

# Chip type is auto-detected from the connected board (C3 or S3).
python3 "$EOPT" $PORT --baud 921600 write_flash 0x1A0000 "$FW"
echo
echo "Done. Reset the board to boot. (Invalid otadata defaults to ota_0 = Hertz Hunter;)"
echo "use the in-app menus to switch.)"
