#!/usr/bin/env bash
# Full 16 MB SPI-flash backup of an Xteink X4 (CrossPoint, stock, or BikeHUD).
# Usage:
#   ./scripts/backup-x4.sh [port]
#   ./scripts/backup-x4.sh /dev/cu.usbmodem2101
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT_DIR="${ROOT}/backups"
mkdir -p "${OUT_DIR}"

PORT="${1:-}"
if [[ -z "${PORT}" ]]; then
  # shellcheck disable=SC2207
  CANDIDATES=($(ls /dev/cu.usb* /dev/cu.wch* /dev/ttyACM* 2>/dev/null || true))
  if [[ ${#CANDIDATES[@]} -eq 0 ]]; then
    echo "No serial port found. Plug in the X4 (awake) and pass the port:"
    echo "  $0 /dev/cu.usbmodemXXXX"
    exit 1
  fi
  if [[ ${#CANDIDATES[@]} -gt 1 ]]; then
    echo "Multiple ports; pass one explicitly:"
    printf '  %s\n' "${CANDIDATES[@]}"
    exit 1
  fi
  PORT="${CANDIDATES[0]}"
fi

STAMP="$(date +%Y%m%d-%H%M%S)"
OUT="${OUT_DIR}/x4-full-${STAMP}.bin"

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

echo "Port: ${PORT}"
echo "Writing full flash dump → ${OUT}"
echo "(16 MB — several minutes)"

"${ESPTOOL[@]}" --chip esp32c3 --port "${PORT}" --baud 921600 \
  read_flash 0x0 0x1000000 "${OUT}"

SIZE="$(wc -c <"${OUT}" | tr -d ' ')"
echo "Done. ${SIZE} bytes → ${OUT}"
if [[ "${SIZE}" -ne 16777216 ]]; then
  echo "WARNING: expected 16777216 bytes (16 MB). Verify before erasing the device."
fi
