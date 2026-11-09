# Testing and benchmarks

## Host verification

From the repository root:

```sh
python3 -B -m unittest discover -s tests -v
bash scripts/compile.sh
bash scripts/compile.sh --baseline
git diff --check
```

The suite needs Python's standard library, a C++ compiler and the installed
Adafruit BNO055 headers. It covers the 1,345 historical math vectors, frozen
source/fixture checks, production core, OSC/JSON/journals, connectivity,
HTTP runtime, power and fault handling. Host simulations are not hardware
measurements. Some tests open local sockets and require an environment that
permits localhost networking.

Builds and tests do not upload firmware. The toolchain is locked; see
[Getting started](getting-started.md). Classify results as PASS, FAIL or NOT RUN,
with duration, conditions, logs and limitations. Prior recorded outcomes are
summarized in [Validation](validation.md).

## Separate the measurements

| Quantity | What it establishes |
| --- | --- |
| Received UDP datagrams/s | Packets observed by the host socket for a source |
| Received OSC messages | Decoded messages, possibly several per bundle |
| Firmware IMU acquisition rate | Successful acquisition counter delta / firmware time-window delta |
| BNO055 internal update rate | A separate sensor property; not measured by counting firmware reads |
| Receive jitter | Variation of host-observed packet intervals |
| Acquisition jitter | Variation of instrumented firmware acquisition intervals |
| End-to-end latency | Requires a defined common time reference or independent instrumentation |

OSC reception does **not** measure the IMU rate. Local UDP enqueue success does
not prove delivery. Capture-local packet IDs cannot reveal sender loss,
duplicates or reordering. Immediate OSC bundle timetags are not sample timestamps.

The original V1.1 exposes neither sample sequence/time nor acquisition counters.
Current Axio keeps monotonic 64-bit timestamps, sample sequence and USB
diagnostics, but its timing differs from the historical read/send loop.
Instrumentation and synchronous I²C/NVS/socket operations add overhead; do not
claim timing equivalence.

## Capture OSC

Configure the tracker's destination as the receiving computer's actual LAN IP
and chosen port, then use a new output filename:

```sh
python3 -B tools/osc_receiver.py --bind 0.0.0.0 --port 16888 --duration 60 --output captures/osc-idle-01.jsonl
python3 -B tools/analyze_capture.py captures/osc-idle-01.jsonl
```

The receiver uses only Python's standard library. It refuses to overwrite
captures; duration 0 runs until Ctrl-C. Dedicate the port to this receiver and
check the host firewall if needed. Its OSC decoder supports tags
`i f s b h d T F N I`, records bundle timetags without scheduling delivery, and
records malformed/unsupported packets with their raw base64 payload.

Each JSONL record is one datagram, with capture-local `packet_id`, host UNIX
and monotonic nanosecond timestamps, source IP/port, size, raw payload, and
decoded messages or an error. Timestamps are taken after `recvfrom` and before
parsing: they are process observations, not radio/kernel arrival times.

The analyzer groups by source IP/port in capture order:

- `received_udp_hz = (N-1)/(last-first)` across all datagrams.
- `received_osc_hz` uses only successfully decoded OSC datagrams.
- `osc_messages` counts messages inside packets, not acquisitions.
- UDP interval statistics include mean, median, nearest-rank p95, maximum and
  population standard deviation; the last is the receive-jitter definition.
- Fewer than two distinct timestamps gives a null rate. Empty capture gives
  `[]`. Initial/final silence is excluded unless window boundaries are recorded
  separately. Invalid packets remain in UDP interval statistics.
- `imu_hz` remains null; it is not inferred from received traffic.

Record gaps and bursts without automatically labeling them packet loss.
Do not combine restarts or sources without an explicit analysis.

## USB and hardware checks

Optional USB tooling requires `pyserial` in the local virtual environment.
A read-only default check is:

```sh
.venv/bin/python -B tools/hardware_test.py --port /dev/cu.usbmodem2101 --metrics-seconds 10 --report captures/usb-01.json
```

Replace the port and choose a new filename. The tool reads INFO, STATUS,
CONFIG GET and before/after METRICS. Missing port/pyserial means NOT RUN (exit 2);
failed checks mean exit 1. Wi-Fi checks require `--require-wifi`, optionally
with `--wifi-wait-seconds 20`. Receiving OSC requires `--osc-seconds` and a
destination already configured on the tracker.

`--calibrate` (old alias `--tare`) changes RAM reference values.
`--reset-metrics` changes counters. Use either only when intentionally
performing that action; neither is needed for normal observation.

`tools/osc_hardware_test.py` checks received format/routing against an already
applied configuration, optionally comparing CONFIG GET/METRICS over USB.
It does not Apply settings; calibration is opt-in and requires enabled ACK.
`tools/osc_config.py` generates schema 1 presets or performs explicit USB
GET/VALIDATE/APPLY. Use `--help` for exact flags; applying persists settings.

## Reproducible hardware measurement

1. Record the board/breakout, wiring, source revision, toolchain/options,
   power source, network/AP, channel, distance, host OS/load and receiver setup.
2. Build and upload intentionally. Confirm BNO055 readiness via USB. Allow Wi-Fi
   and sensor fusion to settle; record the actual settling time.
3. Record active OSC configuration and diagnostics before the window. Avoid
   calibration, Apply, reset or network changes during a steady-state run.
4. Repeat at least three equal windows for idle, controlled movement and
   dashboard interaction. Record any calibration separately.
5. Take post-window firmware counters. Compare matched deltas/time scopes;
   counter resets invalidate a continuous-window calculation.
6. Keep disconnect/reconnect, multi-client and power tests separate, documenting
   restoration of settings. Preserve intermediate failures as evidence.

The scenario generator `tools/osc_benchmark.py` creates 36 schema 1 workload
candidates under explicit output paths. Its results are NOT RUN until validated
and measured on hardware. It does not establish sustainable destination/message
capacity or change firmware automatically.

Long-running, network-recovery and mDNS partition probes are available in
`tools/` and `tests/hardware/`. Some intentionally change radio state or upload
instrumented firmware; their explicit action flags and restoration requirements
must be understood before use. They are not part of a routine documentation check.

Record raw data, metric definitions, window boundaries and observed limits.
A functional PASS does not certify an unspecified performance target, exact
100 Hz, zero packet loss or absence of leaks. The
[original Phase 0 benchmark plan](archive/benchmarks.md) remains historical;
current hardware outcomes and untested work are listed in
[Validation](validation.md).
