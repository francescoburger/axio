# XIAO ESP32S3 support and validation

On 12 November 2026, the user requested installation on a Seeed XIAO ESP32S3,
confirmed the connected USB board, and confirmed the same pad wiring as Axio:
SDA D4, SCL D5, button D3 and battery divider D0. This session adds board
selection and records the initial S3 upload and USB check. Historical C3
hardware results do not establish S3 behavior.

## Changes

`hardware_config.h` selects the user-confirmed GPIO mapping by Arduino board:

| Signal | Pad | C3 GPIO | S3 GPIO |
| --- | --- | --- | --- |
| BNO055 SDA | D4 | 6 | 5 |
| BNO055 SCL | D5 | 7 | 6 |
| Button to GND | D3 | 5 | 4 |
| Battery divider ADC | D0 | 2 | 1 |

The BNO055 address remains 0x29. The S3 uses RTC EXT0 LOW wake with an RTC
pull-up and restores digital GPIO mode before evaluating the existing wake
hold. The C3 keeps its GPIO wake API. Reference calibration, sensor conversion
and scheduling code are unchanged. USB INFO and the existing dashboard report
the selected board instead of a fixed C3 label.

Build and upload accept `--board c3|s3`, defaulting to C3, and share board
options in `scripts/board_config.sh`. S3 artifacts are isolated in
`.build/s3/artifacts/`. Its target uses 240 MHz CPU, 8 MB flash, default 8 MB
partitions, hardware USB CDC/JTAG, enabled CDC and disabled PSRAM. Baseline
builds remain C3 only. See [Getting started](getting-started.md) for commands.

## Recorded checks

| Check | Status | Evidence and limit |
| --- | --- | --- |
| Toolchain and locked libraries | PASS | Version checks in the build logs |
| Host suite | PASS | 114 tests, including pin selection and simulated C3/S3 wake paths; `.build/s3-host-tests-authorized.log` |
| Mathematical regression and immutable sources | PASS | 1,345 unchanged baseline/core vectors and source hash/fixture byte checks in the host suite |
| Initial host suite in sandbox | FAIL, environment restriction | HTTP/UDP sockets denied; `.build/s3-host-tests.log`; rerun outside sandbox passed |
| C3 build | PASS | 1,149,279 B flash, 111,172 B static RAM; `.build/c3-compatibility-compile.log`; no C3 upload |
| Final S3 build | PASS | 1,034,148 B flash, 121,520 B static RAM; `.build/s3-upload-authorized.log` |
| Initial USB upload attempt in sandbox | FAIL, environment restriction | Serial open denied before flashing; `.build/s3-upload.log` |
| S3 upload | PASS | Loader identified ESP32-S3 revision v0.2; written hashes verified; `.build/s3-upload-authorized.log` |
| USB PING, INFO, STATUS and CONFIG GET | PASS | Reports `XIAO_ESP32S3`, enabled CDC and valid IMU sample; `captures/xiao-s3-bringup-2026-11-12.json` |
| BNO055 acquisition counters | PASS | 502 acquisitions over 5.017417 s, zero new read errors; same USB capture; short counter observation only |
| AP startup and HTTP listener state | PASS, firmware-reported | USB status reports AP ready and HTTP active after 15 s offline; no independent Wi-Fi/browser check |
| STA connection, mDNS and OSC reception | NOT RUN | STA was disconnected during the USB check; no network configuration changes or UDP reception test |
| Physical sleep/wake and sleep current | NOT RUN | Host tests do not establish electrical wake behavior, hold timing or current |
| Battery accuracy | NOT RUN | ADC mapping verified in software only |
| Calibration and DIAG RESET on hardware | NOT RUN | Neither command requested or sent |
| Settings persistence across another restart | NOT RUN | No before/after configuration comparison or deliberate persistence test |
| S3 endurance and C3 hardware regression | NOT RUN | No extended run or new C3 session |

The first clean S3 build emitted the existing external OSC library warning
`OSCMessage.cpp:897` (implicit fallthrough); see `.build/s3-compile.log`.
The upload uses `EraseFlash=none`. Its logged erase/write ranges exclude the
default NVS region, 0x9000–0xdfff; this is not a separate persistence test.
The firmware reported an existing migrated network configuration.

Acquisition counters are distinct from OSC send attempts and received packets.
The short USB window does not establish sustained performance or equivalence
with C3. No credentials or captures are intended for Git, and no commit or push
was performed.
