# Firmware modules

Arduino compiles this directory recursively. The main sketch,
[`../Axio.ino`](../Axio.ino), coordinates acquisition, OSC, connectivity,
USB diagnostics, power and the local dashboard.

| Directory | Role |
| --- | --- |
| `core/` | Checked BNO055 reads, exact V1.1 reference math, samples, state and 10 ms scheduling |
| `osc/` | Configuration schemas, single editor, codec, engine and persistence journal |
| `transport/` | UDP/NVS runtime; retained historical transport for regression |
| `connectivity/` | Multi-network STA, protected fallback AP, provisioning, mDNS/discovery and common entry-point election |
| `diagnostics/` | Bounded ASCII request framing, JSONL service and budgeted USB RX/TX |
| `power/` | Battery ADC sampling, button, wake qualification and controlled deep sleep |
| `web/` | Bounded HTTP runtime, service handlers, JSON writer and generated flash assets |

Pins and the BNO055 address come exclusively from
[`../hardware_config.h`](../hardware_config.h). Transport consumes complete
snapshots without I²C. Sensor blocks are sequential, so a complete sample is
not atomic. On read failure the last valid sample remains available with an
explicit error state; sequence/time do not advance and it is not sent as new
OSC data.

Dashboard source assets are in [`../web/`](../web/).
Run `python3 -B scripts/package_web.py` from the repository root after editing
them; the CLI build does this automatically. The generated header is included
for Arduino IDE compatibility. The full dashboard is served on STA and AP;
the common `axio.local` entry point is elected on STA.

See [architecture](../../../docs/architecture.md),
[protocols](../../../docs/protocols.md) and
[validation and known limits](../../../docs/validation.md).
