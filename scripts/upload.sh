#!/bin/bash
# Explicit action, separate from tests. Does not erase NVS or install dependencies.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
usage() { echo 'Usage: bash scripts/upload.sh [--board c3|s3] --port /dev/cu.usbmodem... (builds and uploads; default: c3)'; }
BOARD=c3
PORT=''
while [[ $# -gt 0 ]]; do
  case "$1" in
    --board) [[ $# -ge 2 ]] || { usage >&2; exit 2; }; BOARD="$2"; shift 2 ;;
    --port) [[ $# -ge 2 ]] || { usage >&2; exit 2; }; PORT="$2"; shift 2 ;;
    --help|-h) usage; exit 0 ;;
    *) usage >&2; exit 2 ;;
  esac
done
if [[ -z "$PORT" ]]; then
  usage >&2
  exit 2
fi
source "$ROOT/scripts/board_config.sh"
axio_board_config "$BOARD"
bash scripts/compile.sh --board "$BOARD"
arduino-cli upload --fqbn "$FQBN" \
  --board-options "$BOARD_OPTIONS" \
  --port "$PORT" --input-dir "$ROOT/$OUTPUT" "$ROOT/firmware/Axio"
