# Button-only boot validation

Implemented on 14 November 2026. Normal operation now requires a physical
button wake from deep sleep and the existing long-press qualification on both
XIAO variants. Every other boot defaults to logically off.

**Status: implementation complete and accepted by the user.** After the S3
upload, the user confirmed that it works and that deep-sleep consumption is
acceptable. No numeric current measurement or instrument trace was supplied.
The USB power-restoration test after complete battery discharge remains
**NOT RUN**, deferred to the user's later test. The user explicitly closed
the implementation task and will reopen it if problems arise; no further
implementation or hardware testing is pending in this task.

## Implementation

Previously, `qualifyButtonWake()` returned immediately for power-on and reset,
allowing normal setup to start automatically. It now requires both
`esp_reset_reason() == ESP_RST_DEEPSLEEP` and the board's button wake cause.
The RTC wake-stub timestamp is used only on that path. Power loss cannot
preserve an ON authorization: there is no persisted power flag and no added
NVS writes or dependency.

| Board | Button | Deep-sleep wake | Bias |
| --- | --- | --- | --- |
| XIAO ESP32C3 | D3 / GPIO5, active LOW | GPIO LOW | SDK internal pull-up |
| XIAO ESP32S3 | D3 / GPIO4, active LOW | RTC EXT0 LOW | Explicit RTC pull-up, pull-down disabled |

Pin numbers match the installed Arduino 3.3.12 XIAO variant headers and remain
centralized in `hardware_config.h`. C3 GPIO5 is in the deep-sleep-capable
GPIO0–5 range; the installed C3 SDK enables
`CONFIG_ESP_SLEEP_GPIO_ENABLE_INTERNAL_RESISTORS`. S3 GPIO4 is an RTC GPIO.
The existing wake methods are retained, following Espressif's
[C3 sleep documentation](https://docs.espressif.com/projects/esp-idf/en/v5.5.3/esp32c3/api-reference/system/sleep_modes.html),
[S3 EXT0 documentation](https://docs.espressif.com/projects/esp-idf/en/v5.5.3/esp32s3/api-reference/system/sleep_modes.html#external-wakeup-ext0)
and [reset reasons](https://docs.espressif.com/projects/esp-idf/en/v5.5.3/esp32c3/api-reference/system/misc_system_api.html#reset-reason).

Non-button boots and rejected short wakes hold the LED off, wait for a stable
100 ms button release, configure only the button as a wake source and enter
deep sleep. Previously configured wake sources are cleared. A button held
across power restoration cannot cause a repeated LOW-wake loop; release it
and make a fresh long press after power returns. The release wait uses the
current boot's monotonic timer, independently of RTC-retained data.

The 1.6 s internal wake threshold, RTC timing from the wake stub, release
debounce, 5 s running hold and normal sleep sequence remain unchanged. Normal
sleep still verifies BNO055 SUSPEND, stops communications, finishes LED feedback
and waits for release. A wake-configuration failure in the early boot gate
keeps application setup blocked, without retry/reset loops; it requires repair
and external reset, and cannot guarantee deep-sleep current in that fault case.

The gate runs before CPU frequency changes, flash-info preparation, explicit
USB diagnostics, IMU, connectivity, OSC and dashboard setup. Only the feedback
LED's inactive output is initialized first. All reset types are deliberately
treated alike: dashboard **Restart** also leaves the device off until a new
button activation. Saved networks and settings are preserved.

## Checks

| Check | Status | Evidence and scope |
| --- | --- | --- |
| C3/S3 wake adapter and power-reset simulations | PASS | `tests/wake_runtime_harness.cpp`; compiled against both board branches with warnings as errors |
| Power restoration and reset policy | PASS, simulated | Power-on, external, software, panic, watchdog, brownout, USB, JTAG and unknown reset; undefined, stale button and timer wake causes; stale RTC timestamp ignored |
| Button activation after each reset | PASS, simulated | Each reset case returns to sleep, then accepts the next deliberate long button wake |
| Button release and wake-loop prevention | PASS, simulated | Held button across restoration, release bounce and re-press after short-wake rejection; one sleep entry after stable release |
| Wake failure policy | PASS, simulated | Wake-source clearing, GPIO/EXT0 configuration and S3 RTC pull failures block application startup without resets or repeated configuration attempts |
| Existing long-press and shutdown sequence | PASS, simulated | Targeted Phase 4 tests cover hold thresholds, sensor readback, service stop, LED completion and release |
| Full host regression suite | PASS | 117 tests; `.build/button-boot-tests-authorized.log` |
| Initial suite inside restricted sandbox | FAIL, environment restriction | HTTP/UDP socket access denied (11 errors, one failure); `.build/button-boot-tests.log`; same suite subsequently passed outside the sandbox |
| Immutable baseline, frozen fixture and mathematical regression | PASS | Hash/byte checks and 1,345 unchanged baseline/core/oracle vectors in the full suite |
| C3 firmware build | PASS | 1,150,815 B flash, 111,188 B static RAM; `.build/button-boot-c3-compile.log`; no upload |
| S3 firmware build | PASS | 1,036,336 B flash, 121,544 B static RAM; `.build/button-boot-s3-compile.log`; no upload |
| Diff whitespace and changed documentation links | PASS | `git diff --check`; 65 local Markdown links resolve |
| S3 upload | PASS | Subsequently authorized by the user; ESP32-S3 identified on `/dev/cu.usbmodem2101`, written data hashes verified, command exit 0; `.build/button-boot-s3-upload.log` |
| Basic operation after S3 upload | PASS, user-reported | User confirmed that the implementation works; individual scenarios were not itemized |
| Deep-sleep consumption | PASS, user-reported acceptance | User reported good consumption; no numeric value or independent measurement recorded |
| USB power restoration after complete battery discharge | NOT RUN | User deferred the test and accepted implementation completion without it |
| C3 upload and full physical scenario matrix below | NOT RUN | No C3 upload or instrumented/exhaustive electrical validation performed |

Builds use the existing locked toolchain; no dependency was installed. Host
GPIO/RTC mocks prove software branch behavior, not electrical wake capability
or actual charging, USB switching, current or timing. The historical external
OSC library fallthrough warning is not changed by this work.

## Subsequent S3 upload

The user explicitly requested the upload after reviewing the implementation.
The connected chip was identified as ESP32-S3 revision v0.2, and
`bash scripts/upload.sh --board s3 --port /dev/cu.usbmodem2101` rebuilt and
uploaded the S3 target with `EraseFlash=none`. Esptool verified written data
hashes and reset the board. The logged erase/write ranges exclude NVS at
`0x9000–0xdfff`; no saved-setting comparison was performed after upload.

Initial chip identification inside the sandbox was **FAIL, environment
restriction** (serial-port access denied); identification after granting USB
access was **PASS**. Evidence is in `.build/button-boot-chip-id.log` and
`.build/button-boot-chip-id-authorized.log`. Esptool's message that S3 has no
chip ID and returns its MAC instead is informational.

Post-upload USB diagnostics and instrumented off/wake checks are **NOT RUN**.
The user subsequently confirmed working behavior and acceptable deep-sleep
consumption, as recorded above. The new reset policy requires a fresh physical
long press before application diagnostics and networking start. Neither upload
success nor the general user report establishes the full electrical scenario
matrix or a numeric deep-sleep current.

## Hardware limits and optional future device checks

Software cannot prevent the MCU, ROM, bootloader and framework from briefly
starting when electrical power returns or reset is asserted. The application
then returns to deep sleep. Boot current, brownout behavior, supply switching
and exact button timing require measurement. A completely unpowered board
cannot respond to a GPIO button: restore sufficient power first. Firmware
cannot prevent repeated electrical brownouts from an unstable supply.

Deep sleep is not electrical disconnection. The charger, regulator, charge
LED, battery divider and BNO055 can still consume power. On the early-off
path, I²C is not initialized and the sensor is not configured: after cold
power-up or a reset while it was running, BNO055 SUSPEND is not guaranteed.
The existing voluntary shutdown still suspends it. Minimum whole-device
current after arbitrary power loss cannot be guaranteed by the MCU gate.
S3 retains EXT0's RTC power requirements to preserve the existing button circuit.

USB enumeration and boot messages may occur before the application gate;
application diagnostics and network services stay unavailable while off.
Subsequent uploads may require the board's existing BOOT/RESET download
procedure. A stuck-LOW button waits for release with the application blocked;
this fault is not claimed to have deep-sleep current.

The following procedure is retained for any future expanded validation on
**each board**; it does not leave the accepted implementation task open. Use
a suitable controlled supply or protected battery procedure rather than
damaging a cell by over-discharge:

1. **Voluntary shutdown:** hold 5 s, confirm sensor/communications stop, LED
   sequence and deep sleep after release; leave idle and check current.
2. **Voluntary activation:** confirm a short press returns to sleep and the
   existing long press starts the device; keep holding through startup and
   confirm it must be released before normal actions. Repeat after shutdown.
3. **Complete power loss and charge connection:** remove power, restore it
   with the button released and confirm no application activity; then verify
   a fresh long press works. Repeat with the button held during restoration,
   release it and make a fresh press; check for wake loops.
4. **USB removal after charging:** with battery connected and Axio off,
   disconnect USB and confirm it stays off, including any supply-transition
   reset; then verify button activation.
5. **Unexpected reset:** while running and while off, reset externally and
   check it stays off until a fresh long press; separately validate brownout
   and watchdog cases with an approved controlled test.
6. **C3/S3 parity:** repeat the complete sequence on D3/GPIO5 and D3/GPIO4,
   verify internal pull-up stability, USB upload recovery, real timing and
   off-state current, including the sensor after cold power restoration.

Hardware evidence is limited to the S3 upload and the subsequent user report
described above. No agent-run functional hardware test, IMU performance claim,
calibration, low-battery automatic shutdown, NVS erase or settings migration
is part of this change.
