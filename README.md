# Axio

Axio is a wireless motion tracker built with a **Seeed XIAO ESP32C3 or ESP32S3** and a
**Bosch BNO055**. It sends motion data over OSC and serves a local web dashboard
for monitoring, network setup and OSC configuration.

## Get started

1. Check the [hardware and build instructions](docs/getting-started.md).
2. Connect Axio to your Wi-Fi, or join its WPA2 fallback access point and open
   `http://192.168.4.1/`. The default AP password is `12345678`.
3. On the same LAN, open `http://axio.local/`.
4. In **OSC**, set the receiving computer's LAN IPv4 address and UDP port,
   choose the data fields and send interval, then select **Validate** and **Apply**.

The dashboard runs entirely on the device, without a cloud service or external
web assets. It has no user authentication on the LAN. OSC is suspended while
the fallback AP is active. The [user guide](docs/user-guide.md) covers the five
pages, calibration, the physical button and connection recovery.

The [feedback LED](docs/user-guide.md#led-feedback) signals startup, successful
reference calibration and deep sleep. It uses the onboard GPIO21 user LED on
S3, or an external LED on GPIO3 / D1 on C3. Pin and polarity are configurable in
[`hardware_config.h`](firmware/Axio/hardware_config.h).

## Upload from Arduino IDE

1. In Arduino IDE **Settings / Preferences**, add this URL under
   **Additional Boards Manager URLs**:

   ```text
   https://espressif.github.io/arduino-esp32/package_esp32_index.json
   ```

   Open **Boards Manager** and install **esp32 by Espressif Systems**, version
   **3.3.12**.
2. In **Library Manager**, install the following versions. Allow the IDE to
   install the Adafruit dependencies, then check their versions against this
   table:

   | Library | Version |
   | --- | --- |
   | Adafruit BNO055 | 1.6.4 |
   | Adafruit Unified Sensor | 1.1.15 |
   | Adafruit BusIO | 1.17.4 |
   | OSC by CNMAT | 4.0.0 |

3. Open [`firmware/Axio/Axio.ino`](firmware/Axio/Axio.ino), keeping the complete
   `Axio` sketch folder and its `src/` directory together. Under **Tools → Board**,
   select **XIAO_ESP32C3** or **XIAO_ESP32S3** for your connected board.
4. Set the board options under **Tools**:

   | Option | XIAO_ESP32C3 | XIAO_ESP32S3 |
   | --- | --- | --- |
   | USB CDC On Boot | Enabled | Enabled |
   | CPU Frequency | 160 MHz | 240 MHz |
   | Flash Size | 4 MB | 8 MB |
   | Flash Mode | QIO | QIO 80MHz |
   | Partition Scheme | Default 4MB with spiffs (1.2MB APP/1.5MB SPIFFS) | Default with spiffs (3MB APP/1.5MB SPIFFS) |
   | Upload Speed | 921600 | 921600 |
   | Core Debug Level | None | None |
   | Erase All Flash Before Sketch Upload | Disabled | Disabled |
   | USB Mode | — | Hardware CDC and JTAG |
   | Upload Mode | — | UART0 / Hardware CDC |
   | PSRAM | — | Disabled |
   | Arduino Runs On | — | Core 1 |
   | Events Run On | — | Core 1 |

   For C3, also set **Flash Frequency: 80 MHz**. A dash means the option is not
   needed for that board. Pin mapping is selected automatically; use the
   [wiring table](docs/getting-started.md#hardware) for your board.
5. Connect the XIAO by USB, select its actual port under **Tools → Port**, close
   Serial Monitor, and click **Upload**. Wait for the upload to finish before
   following the Wi-Fi connection steps above. The port may change after reset.

The dashboard assets are already included for IDE builds. If you edit the web
sources, regenerate them with `python3 -B scripts/package_web.py` before
uploading. See [Getting started](docs/getting-started.md#arduino-ide) for more
details and the CLI alternative.

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
