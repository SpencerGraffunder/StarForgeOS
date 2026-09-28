#!/usr/bin/env bash
# Flash StarForgeOS into the ota_1 slot for dual-boot with NuclearCounter.
# PlatformIO always uploads to the ota_0 offset, so this uses esptool
# directly at the correct address (chip-detected: S3 0x1A0000, C3 0x150000).
#
# Usage: ./flash_dualboot.sh [serial-port]
#   serial-port defaults to auto-detect (single ESP32 connected)
#
# Prereq: NuclearCounter was flashed first (it installs bootloader + the
# shared partition table). Verify with:
#   esptool.py --port <port> read_flash 0x10000 0x40 | xxd | head -1
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

# Detect the chip to pick the right ota_1 offset (S3: 0x1A0000, C3: 0x150000).
CHIP_INFO=$(python3 "$EOPT" $PORT chip-id 2>/dev/null || true)
if echo "$CHIP_INFO" | grep -qi 'ESP32-S3'; then
  OTA1_OFF=0x1A0000
elif echo "$CHIP_INFO" | grep -qi 'ESP32-C3'; then
  OTA1_OFF=0x150000
else
  echo "WARNING: could not detect chip type; assuming S3 (ota_1 @ 0x1A0000)"
  echo "$CHIP_INFO"
  OTA1_OFF=0x1A0000
fi
echo "ota_1 offset: $OTA1_OFF"

python3 "$EOPT" $PORT --baud 921600 write_flash "$OTA1_OFF" "$FW"
echo
echo "Done. Reset the board to boot. (Invalid otadata defaults to ota_0 = NuclearCounter;)"
echo "use the in-app menus to switch.)"
