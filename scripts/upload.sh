#!/bin/bash
# Explicit action, separate from tests. Does not erase NVS or install dependencies.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
if [[ $# -ne 2 || "$1" != '--port' || -z "$2" ]]; then
  echo 'Usage: bash scripts/upload.sh --port /dev/cu.usbmodem... (builds and uploads)' >&2
  exit 2
fi
PORT="$2"
bash scripts/compile.sh
arduino-cli upload --fqbn esp32:esp32:XIAO_ESP32C3 \
  --board-options 'CDCOnBoot=default,CPUFreq=160,FlashFreq=80,FlashMode=qio,FlashSize=4M,PartitionScheme=default,DebugLevel=none,EraseFlash=none,UploadSpeed=921600' \
  --port "$PORT" --input-dir "$ROOT/.build/artifacts" "$ROOT/firmware/Axio"
