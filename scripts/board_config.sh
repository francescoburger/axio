#!/bin/bash
# Shared build/upload options from the locked ESP32 3.3.12 core.
axio_board_config() {
  case "$1" in
    c3)
      FQBN='esp32:esp32:XIAO_ESP32C3'
      BOARD_OPTIONS='CDCOnBoot=default,CPUFreq=160,FlashFreq=80,FlashMode=qio,FlashSize=4M,PartitionScheme=default,DebugLevel=none,EraseFlash=none,UploadSpeed=921600'
      BUILD='.build/arduino'
      OUTPUT='.build/artifacts'
      ;;
    s3)
      FQBN='esp32:esp32:XIAO_ESP32S3'
      BOARD_OPTIONS='USBMode=hwcdc,CDCOnBoot=default,UploadMode=default,CPUFreq=240,FlashMode=qio,FlashSize=8M,PartitionScheme=default_8MB,PSRAM=disabled,LoopCore=1,EventsCore=1,DebugLevel=none,EraseFlash=none,UploadSpeed=921600'
      BUILD='.build/s3/arduino'
      OUTPUT='.build/s3/artifacts'
      ;;
    *) echo 'FAIL: board must be c3 or s3' >&2; return 2 ;;
  esac
}
