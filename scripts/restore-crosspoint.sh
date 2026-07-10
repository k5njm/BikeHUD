#!/usr/bin/env bash
# Restore CrossPoint (or any app image) onto X4, or restore a full 16 MB dump.
#
# Usage:
#   # App image at 0x10000 (CrossPoint release firmware.bin)
#   ./scripts/restore-crosspoint.sh /path/to/crosspoint-firmware.bin [port]
#
#   # Full dump taken by backup-x4.sh
#   ./scripts/restore-crosspoint.sh --full backups/x4-full-….bin [port]
set -euo pipefail

FULL=0
if [[ "${1:-}" == "--full" ]]; then
  FULL=1
  shift
fi

IMG="${1:-}"
PORT="${2:-}"

if [[ -z "${IMG}" || ! -f "${IMG}" ]]; then
  echo "Usage: $0 [--full] <firmware.bin|full-dump.bin> [port]"
  echo ""
  echo "  default: write_flash 0x10000  (CrossPoint / BikeHUD app image)"
  echo "  --full:  write_flash 0x0      (16 MB backup from backup-x4.sh)"
  echo ""
  echo "Or use the web flasher: https://crosspointreader.com/#flash-tools"
  exit 1
fi

if [[ -z "${PORT}" ]]; then
  # shellcheck disable=SC2207
  CANDIDATES=($(ls /dev/cu.usb* /dev/cu.wch* /dev/ttyACM* 2>/dev/null || true))
  if [[ ${#CANDIDATES[@]} -ne 1 ]]; then
    echo "Pass serial port explicitly (found ${#CANDIDATES[@]} candidates)."
    exit 1
  fi
  PORT="${CANDIDATES[0]}"
fi

if [[ -x "${HOME}/.platformio/penv/bin/esptool" ]]; then
  ESPTOOL=("${HOME}/.platformio/penv/bin/esptool")
elif command -v esptool >/dev/null 2>&1; then
  ESPTOOL=(esptool)
elif command -v esptool.py >/dev/null 2>&1; then
  ESPTOOL=(esptool.py)
elif [[ -x "${HOME}/.platformio/penv/bin/python" ]]; then
  ESPTOOL=("${HOME}/.platformio/penv/bin/python" -m esptool)
else
  echo "Need PlatformIO penv esptool or: pip install esptool"
  exit 1
fi

SIZE="$(wc -c <"${IMG}" | tr -d ' ')"
echo "Image: ${IMG} (${SIZE} bytes)"
echo "Port:  ${PORT}"

if [[ "${FULL}" -eq 1 ]]; then
  if [[ "${SIZE}" -ne 16777216 ]]; then
    echo "WARNING: --full expects 16 MB (16777216); this file is ${SIZE}."
    read -r -p "Continue anyway? [y/N] " ans
    [[ "${ans}" == "y" || "${ans}" == "Y" ]] || exit 1
  fi
  echo "Restoring FULL flash at 0x0 …"
  "${ESPTOOL[@]}" --chip esp32c3 --port "${PORT}" --baud 921600 \
    write_flash 0x0 "${IMG}"
else
  echo "Flashing app image at 0x10000 …"
  "${ESPTOOL[@]}" --chip esp32c3 --port "${PORT}" --baud 921600 \
    write_flash 0x10000 "${IMG}"
fi

echo "Done. Reset the X4 if it does not reboot into CrossPoint."
