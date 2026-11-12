# LED feedback validation

Implemented on 12 November 2026 for both supported XIAO boards. The S3 uses its
onboard GPIO21 user LED, active LOW. At the user's request, C3 is provisionally
assigned the previously unused GPIO3 / D1 for an external LED, active HIGH.
Pin and polarity are centralized in `firmware/Axio/hardware_config.h`.

The agreed cues are one 500 ms startup pulse, two 100 ms successful-calibration
pulses, and three 100 ms shutdown pulses. Short pulses have 100 ms dark gaps;
every cue includes a final 100 ms dark interval. Idle is off. See
[the user guide](user-guide.md#led-feedback) and
[LED wiring](getting-started.md#hardware).

## Checks

| Check | Status | Evidence and scope |
| --- | --- | --- |
| Host suite | PASS | 117 tests; `.build/led-host-tests.log` |
| Baseline math and immutable sources | PASS | 1,345 unchanged mathematical vectors, reference hash and frozen fixture checks in the suite |
| LED state machine | PASS | Pulse boundaries, pending-calibration coalescing, shutdown priority, delayed servicing and unsigned timer rollover; `tests/feedback_led_harness.cpp` |
| C3 and S3 GPIO adapters | PASS | Correct pin/polarity, idle output, sleep hold and wake/recovery behavior against fake GPIO; `tests/feedback_led_runtime_harness.cpp` |
| Calibration success notification | PASS | Core tests confirm one callback on success and none on initialization/quaternion/Euler failure; calculation order unchanged |
| Wake qualification | PASS | Simulated accepted/rejected wake; no startup cue during qualification; rejected wake holds LED off |
| Sleep sequencing | PASS | Simulated sensor readback, service stop, LED completion, stable release, late wake failure and recovery checks |
| Physical-button runtime paths | PASS, simulated hardware | C3 and S3 host runtime observes six pulses total: startup, short-press calibration and shutdown; sensor/network shutdown remains enforced |
| C3 build | PASS | 1,150,389 B flash, 111,188 B static RAM; `.build/led-c3-compile.log` |
| S3 build | PASS | 1,035,848 B flash, 121,536 B static RAM; `.build/led-s3-compile.log` |
| Diff whitespace and local documentation links | PASS | Checked after documentation updates |
| S3 upload of the LED firmware | PASS | Subsequently requested explicitly by the user; ESP32-S3 identified on `/dev/cu.usbmodem2101`, written data hashes verified; `.build/led-s3-upload.log` |
| S3 USB PING, INFO, STATUS and CONFIG GET after upload | PASS | Board reports `XIAO_ESP32S3`, enabled CDC and valid IMU sample; `captures/led-s3-usb-2026-11-12.json` |
| S3 acquisition counters after upload | PASS | 504 acquisitions over 5.037539 s, zero new read errors; same USB capture; short observation only |
| S3 STA connection state | PASS, firmware-reported | STATUS reported connected after upload; no independent network/OSC reception test |
| C3 upload of the LED firmware | NOT RUN | No C3 connected for this upload request |
| Physical LED, calibration confirmation and sleep/wake | NOT RUN | No new hardware calibration, sleep cycle or electrical measurement |
| LED current, visible timing and sustained acquisition performance | NOT RUN | Host checks and the short USB counter window do not establish electrical timing or sustained IMU performance |
| OSC reception and explicit persistence comparison | NOT RUN | No network capture or before/after settings comparison |

The build warning at external `OSCMessage.cpp:897` (implicit fallthrough)
remains present on both targets. No dependency was installed. The host suite
ran outside the sandbox because its localhost HTTP/UDP tests require socket
access, which was denied in earlier sessions.

The runtime adds no blocking delays, I²C reads or automatic calibration. LED
shutdown starts only after sensor suspension and communications shutdown, and
must complete before sleep. A long-held button can outlast the cue: stable
release is still required. GPIO hold is configured for the inactive output
before sleep and released at startup. Exact physical behavior remains untested.

The subsequent S3 upload used `EraseFlash=none`; its logged erase/write ranges
exclude the default NVS region. No credential change, hardware calibration,
DIAG RESET or push was performed during validation. Existing network settings were used
by the firmware after upload; this is not a full persistence comparison.
