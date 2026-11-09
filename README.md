# Axio

Axio is a wireless motion tracker built with a **Seeed XIAO ESP32C3** and a
**Bosch BNO055**. It sends motion data over OSC and serves a local web dashboard
for monitoring, network setup and OSC configuration.

The first development milestone is complete: modular firmware, USB diagnostics,
multi-network Wi-Fi, a protected fallback access point, LAN discovery, a five-page
dashboard and button-controlled sleep/wake. The interface has been accepted and
the firmware has been tested on two XIAO boards, one fitted with an IMU.
See [validation and known limits](docs/validation.md) for the scope of those tests.

## Get started

1. Check the [hardware and build instructions](docs/getting-started.md).
2. Connect Axio to your Wi-Fi, or join its WPA2 fallback access point and open
   `http://192.168.4.1/`. The default AP password is `12345678`.
3. On the same LAN, open `http://axio.local/`. Each device also has an individual
   `axio-<full-MAC>.local` hostname and can be reached by its IP address.
4. In **OSC**, set the receiving computer's LAN IPv4 address and UDP port,
   choose the data fields and send interval, then select **Validate** and **Apply**.

The dashboard runs entirely on the device, without a cloud service or external
web assets. It has no user authentication on the LAN. OSC is suspended while
the fallback AP is active. The [user guide](docs/user-guide.md) covers the five
pages, calibration, the physical button and connection recovery.

## Documentation

| Read | Purpose |
| --- | --- |
| [Getting started](docs/getting-started.md) | Wiring, prerequisites, Arduino IDE/CLI, build and upload |
| [User guide](docs/user-guide.md) | Dashboard, Wi-Fi, OSC, calibration and sleep/wake |
| [Protocols](docs/protocols.md) | OSC fields, configuration schemas, USB commands and HTTP API |
| [Architecture](docs/architecture.md) | Firmware modules, scheduling, persistence and web assets |
| [Testing and benchmarks](docs/benchmarks.md) | Host checks, capture tools and measurement rules |
| [Validation and known limits](docs/validation.md) | Recorded outcomes, accepted limitations and untested cases |
| [V1.1 baseline](docs/baseline-v1.1.md) | Frozen mathematical behavior and historical source analysis |
| [Historical archive](docs/archive/README.md) | Original phase plans, verification reports and debugging evidence |

Active documentation is in English. Archived reports retain their original
Italian text and describe the revisions tested at the time; they are historical
evidence rather than current operating instructions.

## Repository map

| Path | Contents |
| --- | --- |
| `firmware/Axio/Axio.ino` | Main sketch; opens directly in Arduino IDE |
| `firmware/Axio/hardware_config.h` | Central pin and sensor address definitions |
| `firmware/Axio/src/` | Acquisition, OSC, connectivity, diagnostics, power and HTTP modules |
| `firmware/Axio/web/` | Dashboard HTML, CSS and JavaScript sources |
| `reference/HandTrackerV1_1.ino` | Immutable historical source |
| `tests/` | Host regression tests, runtime harnesses and hardware probes |
| `tests/fixtures/phase0-Axio.ino` | Frozen Phase 0 fixture checked byte for byte |
| `scripts/` | Toolchain checks, builds, asset packaging and explicit upload |
| `tools/` | USB diagnostics, OSC capture and hardware test tools |
| `toolchain.lock.json` | Verified Arduino toolchain versions |
| `.build/`, `captures/` | Ignored local build output and measurement evidence |

From the repository root, with the prerequisites installed:

```sh
bash scripts/compile.sh
python3 -B -m unittest discover -s tests -v
```

These commands do not upload firmware. Hardware setup, explicit upload and
optional USB tooling are described in [Getting started](docs/getting-started.md).
No automatic reference calibration runs at boot. The quaternion and yaw
calculations preserve the original V1.1 behavior.
