#!/bin/bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
usage() { echo 'Usage: bash scripts/compile.sh [--board c3|s3] [--baseline] (build only; default: c3)'; }
BOARD=c3
BASELINE=false
while [[ $# -gt 0 ]]; do
  case "$1" in
    --board) [[ $# -ge 2 ]] || { usage >&2; exit 2; }; BOARD="$2"; shift 2 ;;
    --baseline) BASELINE=true; shift ;;
    --help|-h) usage; exit 0 ;;
    *) usage >&2; exit 2 ;;
  esac
done
source "$ROOT/scripts/board_config.sh"
axio_board_config "$BOARD"
if $BASELINE && [[ "$BOARD" != c3 ]]; then
  echo 'FAIL: the immutable baseline is supported only on c3' >&2
  exit 2
fi
command -v arduino-cli >/dev/null || { echo 'FAIL: Arduino CLI missing' >&2; exit 1; }
mkdir -p .build/tmp .build/cache .build/arduino .build/artifacts
# Keep build caches and temporary files inside the repository.
export TMPDIR="$ROOT/.build/tmp"
export ARDUINO_BUILD_CACHE_PATH="$ROOT/.build/cache"
python3 -B scripts/check_toolchain.py
python3 -B scripts/package_web.py
SKETCH="$ROOT/firmware/Axio"
BUILD="$ROOT/$BUILD"
OUTPUT="$ROOT/$OUTPUT"
if $BASELINE; then
  SKETCH="$ROOT/.build/baseline/HandTrackerV1_1"
  BUILD="$ROOT/.build/baseline-build"
  OUTPUT="$ROOT/.build/baseline-artifacts"
  mkdir -p "$SKETCH"
  cp "$ROOT/reference/HandTrackerV1_1.ino" "$SKETCH/HandTrackerV1_1.ino"
fi
arduino-cli compile \
  --fqbn "$FQBN" \
  --board-options "$BOARD_OPTIONS" \
  --build-path "$BUILD" \
  --warnings all "$SKETCH"
# --output-dir triggers the core savehex hooks, which would also export
# into the sketch folder. Copy artifacts explicitly to avoid that write.
mkdir -p "$OUTPUT"
cp "$BUILD"/*.bin "$BUILD"/*.elf "$BUILD"/*.map "$OUTPUT/"
echo "PASS: built $SKETCH for $FQBN (no upload); artifacts: $OUTPUT"
