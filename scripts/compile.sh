#!/bin/bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
if [[ $# -gt 1 || ( $# -eq 1 && "$1" != '--baseline' ) ]]; then
  echo 'Uso: bash scripts/compile.sh [--baseline]' >&2
  exit 2
fi
command -v arduino-cli >/dev/null || { echo 'FAIL: Arduino CLI assente' >&2; exit 1; }
mkdir -p .build/tmp .build/cache .build/arduino .build/artifacts
# Evitare cache e temporanei di compilazione esterni al repository.
export TMPDIR="$ROOT/.build/tmp"
export ARDUINO_BUILD_CACHE_PATH="$ROOT/.build/cache"
python3 -B scripts/check_toolchain.py
python3 -B scripts/package_web.py
SKETCH="$ROOT/firmware/Axio"
BUILD="$ROOT/.build/arduino"
OUTPUT="$ROOT/.build/artifacts"
if [[ "${1:-}" == '--baseline' ]]; then
  SKETCH="$ROOT/.build/baseline/HandTrackerV1_1"
  BUILD="$ROOT/.build/baseline-build"
  OUTPUT="$ROOT/.build/baseline-artifacts"
  mkdir -p "$SKETCH"
  cp "$ROOT/reference/HandTrackerV1_1.ino" "$SKETCH/HandTrackerV1_1.ino"
fi
# FQBN e opzioni ricavati dal core esp32 3.3.12 installato.
arduino-cli compile \
  --fqbn esp32:esp32:XIAO_ESP32C3 \
  --board-options 'CDCOnBoot=default,CPUFreq=160,FlashFreq=80,FlashMode=qio,FlashSize=4M,PartitionScheme=default,DebugLevel=none,EraseFlash=none,UploadSpeed=921600' \
  --build-path "$BUILD" \
  --warnings all "$SKETCH"
# --output-dir attiva gli hook savehex del core, che esporterebbero anche
# dentro lo sketch. Copiare esplicitamente gli artefatti evita quella scrittura.
mkdir -p "$OUTPUT"
cp "$BUILD"/*.bin "$BUILD"/*.elf "$BUILD"/*.map "$OUTPUT/"
echo "PASS: compilazione $SKETCH (nessun upload)"
