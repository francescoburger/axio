# Architecture

## Acquisition and scheduling

[`Axio.ino`](../firmware/Axio/Axio.ino) coordinates the firmware in one Arduino
loop. The IMU scheduler uses a 10 ms period, unsigned `millis()` subtraction
and no catch-up. Acquisition completes before transport consumes a snapshot;
transport never reads I²C. HTTP and ADC work are budgeted around the next
acquisition, while intentional power transitions suspend normal operation.

`core/BnoDevice.h` checks hardware reads using the pins and address in
`hardware_config.h`. `core/ImuManager.h` owns initialization, complete samples
and the single reference-calibration entry point. There is no new application
retry on failed sensor initialization.

Quaternion, Euler, acceleration and gyro blocks are read sequentially: a
complete sample is not an atomic sensor observation. Successful samples carry
a sequence and a monotonic 64-bit completion timestamp. Failed reads retain the
last valid sample and set an explicit state; they do not advance sequence/time
or publish a fresh OSC sample. I²C success does not establish fusion quality.

`core/Calibration.h` preserves the V1.1 formulas, product order, truncations,
remainder semantics and precision. Initial references are identity/zero, with
no automatic reference calibration. See [the baseline](baseline-v1.1.md).

## Modules

| Directory | Responsibility |
| --- | --- |
| `src/core/` | Sensor adapter, calibration, samples, tracker state, diagnostics and scheduling |
| `src/osc/` | Configuration, editor compilation, codec, scheduling, routing and two-slot journal |
| `src/transport/` | UDP/NVS adapter and retained historical serialization for regression |
| `src/connectivity/` | STA policy, network journal, WPA2 fallback AP, provisioning, discovery and entry-point election |
| `src/diagnostics/` | Bounded serial framing, commands and budgeted USB RX/TX |
| `src/power/` | Battery sampling, LED feedback, button/wake policy and controlled deep sleep |
| `src/web/` | HTTP parser/runtime, service handlers, JSON writer and generated flash assets |
| `web/` | HTML, CSS and JavaScript sources |

All paths in the table are relative to `firmware/Axio/`. Arduino compiles
`src/` recursively. Policy and codec modules can be compiled on the host;
runtime adapters connect them to ESP32 APIs. The historical serializer is
retained for regression, while the active loop uses `OscRuntime`.

## OSC and persistence

The OSC engine consumes fresh complete snapshots and does not resend the same
sequence. Missed deadlines do not generate catch-up bursts. Requested intervals
depend on sample availability and do not establish a guaranteed output rate.
Schema 2 sends all messages to every configured destination, bundling multiple
due messages. Schema 1 remains readable for compatibility.

OSC and Wi-Fi settings use separate two-slot journals. Each write prepares the
inactive slot, verifies the stored bytes and CRC, then commits through an
atomic selector. Active RAM settings change only after a successful commit.
Recovery can use a prior committed generation; an uncommitted new slot is not
treated as valid fallback. Physical power-cut and NVS wear testing remain
separate from the host fault model.

The Wi-Fi journal reads AXW1 and writes AXW2, including an optional AP password.
Legacy SDK network settings are read before radio configuration switches to RAM
storage; valid legacy credentials can be migrated without erasing the SDK copy.
An intentionally saved empty list is not reimported at the next boot. OSC
storage is separate from networks and historical `configs` preferences.

## Connectivity and discovery

STA attempts follow priority and insertion order, with a 10 s attempt budget,
disconnect confirmation, settling time and exponential backoff up to 30 s.
There is no periodic application scan or RSSI-based preemption. The fallback
AP is requested after 15 s offline, retains WPA2 and closes after STA connects.
OSC requires STA IPv4 and disabled AP mode.

Discovery runs only on STA through the mDNS component included with the locked
ESP32 core. `_axio._udp` advertises tracker identity/listener information and
`_http._tcp` advertises the dashboard. Queries are asynchronous and share a
budget with entry-point election. A bounded peer cache is kept in RAM; it does
not create OSC routes or persist discovered peers.

`EntryElection.h` and `DiscoveryRuntime.*` manage `axio.local` through one native
mDNS responder. The leader uses native hostname `axio` and retains its individual
hostname as a delegate; standby devices use their individual native hostname.
Native probing, conflict renaming, role confirmation and periodic announcements
support ownership and reconciliation. The auxiliary `_axio-entry._udp` service
helps discover candidates. This is not consensus under arbitrary packet loss
or partitions. See [recorded network tests](validation.md).

## HTTP and dashboard assets

The full dashboard is served on the active STA or fallback AP address. A bounded
nonblocking socket runtime handles one connection at a time: 256 B RX and 512 B
TX per tick, headers up to 1,024 B, compact bodies up to 3,072 B and an absolute
5 s timeout. Host, Origin and a random per-boot token are checked for mutations;
these checks do not provide user authentication.

`DashboardService.h` dispatches a restricted set of operations rather than
bridging arbitrary USB commands. `JsonWriter.h` emits JSON without a dynamic
DOM. `ImageSize.h` reads image headers once at startup and avoids a full flash
image verification in the loop.

[`scripts/package_web.py`](../scripts/package_web.py) generates the committed
`src/web/WebAssets.h` from `web/`, with deterministic gzip assets stored in flash.
The build runs packaging automatically. There are no CDNs, remote fonts, web
frameworks or external 3D libraries. Motion rendering runs in the browser;
display-only quaternion normalization does not modify core/OSC values.

Web telemetry is requested only for visible Live Motion. The shared sample
response is refreshed at most at 10 Hz. Battery sampling averages 16 ADC reads
without a burst, preserving integer division in the configured voltage formula;
there is no state-of-charge estimate or automatic low-battery shutdown.

## Power

The power controller sequences acknowledgement, sensor CONFIG/SUSPEND with
readback, service shutdown, LED shutdown feedback, stable button release and
active-LOW wake setup (GPIO5 on C3, RTC EXT0 GPIO4 on S3).
At the 5 s physical-button threshold it can stop the sensor/network before
release, remaining in `waiting_sleep_release` until deep sleep can safely begin.
Failures attempt sensor/service recovery. Wake follows normal startup and does
not restore or automatically recalibrate the RAM reference.

`FeedbackLed.h` advances at most one timed LED edge per loop call using unsigned
`millis()` arithmetic. `FeedbackLedRuntime.cpp` maps it to the board-specific
pin and polarity in `hardware_config.h`. Firmware initializes the output off
before wake qualification, signals startup after setup, and holds the inactive
GPIO level across deep sleep. `ImuManager::calibrateReference()` calls an optional
notification only after successful reference calibration, so every production
command source shares the same feedback. The power controller waits for the
shutdown cue as well as stable button release before sleeping.
