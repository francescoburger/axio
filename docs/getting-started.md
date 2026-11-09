# Getting started

## Hardware

Axio targets the Seeed XIAO ESP32C3 with a BNO055 at I²C address **0x29**.
All pin definitions are centralized in
[`hardware_config.h`](../firmware/Axio/hardware_config.h).

| Signal | XIAO pin | Firmware definition |
| --- | --- | --- |
| BNO055 SDA | D4 / GPIO6 | `SDA_PIN` |
| BNO055 SCL | D5 / GPIO7 | `SCL_PIN` |
| Button to GND, internal pull-up | GPIO5 | `BUTTON_PIN`, active LOW |
| Battery ADC input | A0 / D0 / GPIO2 | `BATTERY_ADC_PIN` |

Use a common ground and check the supply and logic levels for your BNO055
breakout. The battery voltage formula assumes the existing divider scales the
ADC input by two; this repository does not provide a complete battery or
charging circuit design. GPIO2 is the ADC input, not a direct connection for
an arbitrary battery voltage.

The sensor configuration preserves axis remap P0 and the external crystal
setting from V1.1. The old reference sketch uses different I²C pins and remains
unchanged; build the main Axio sketch for the confirmed wiring above.

## Prerequisites

The scripts were developed on macOS. Builds need Arduino CLI, Python 3.9+ and
the versions recorded in [`toolchain.lock.json`](../toolchain.lock.json).
Host regression tests also need a C++ compiler and the Adafruit BNO055 headers.

| Component | Locked version |
| --- | --- |
| Arduino CLI | 1.5.1 |
| ESP32 Arduino core | 3.3.12 |
| Adafruit BNO055 | 1.6.4 |
| Adafruit Unified Sensor | 1.1.15 |
| Adafruit BusIO | 1.17.4 |
| OSC (CNMAT) | 4.0.0 |
| WiFiManager | 2.0.17 |

WiFiManager remains in the locked dependencies for historical source builds.
Current connectivity uses Axio's own manager. WiFi, DNSServer, WiFiClient,
WiFiUdp, Preferences and Wire come with the ESP32 core. No web framework or
Python package is required for the host suite or OSC capture tools.

If preparing a new development machine, the following are explicit setup
commands. They require network access and write to the Arduino user profile;
the repository's build scripts never install dependencies automatically.

```sh
arduino-cli core update-index --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli core install esp32:esp32@3.3.12 --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli lib update-index
arduino-cli lib install 'Adafruit Unified Sensor@1.1.15' 'Adafruit BusIO@1.17.4' 'Adafruit BNO055@1.6.4' 'OSC@4.0.0' 'WiFiManager@2.0.17'
```

Install Arduino CLI 1.5.1 separately if needed. On macOS, a missing C++ compiler
can be supplied by the Command Line Tools (`xcode-select --install`).

## Build and test

Run from your clone's root directory:

```sh
python3 -B scripts/check_toolchain.py
bash scripts/compile.sh
python3 -B -m unittest discover -s tests -v
```

The toolchain check fails on missing or mismatched versions. Build artifacts
are written to `.build/artifacts/`, with cache and temporary files under
`.build/`. Firmware compilation also packages the dashboard assets. No upload
is performed by these commands.

The build uses FQBN `esp32:esp32:XIAO_ESP32C3`, CPU 160 MHz, 4 MB flash,
the default partition layout and enabled USB CDC. The scripts record all board
options explicitly. Matching versions improve reproducibility; they do not
guarantee identical binaries across hosts or detect locally modified libraries.

Host tests normally look for `imumaths` in
`~/Documents/Arduino/libraries/Adafruit_BNO055`. To use another installation:

```sh
BNO055_LIBRARY='/path/to/Adafruit_BNO055' python3 -B -m unittest discover -s tests -v
```

To compile the immutable historical sketch in a temporary Arduino folder:

```sh
bash scripts/compile.sh --baseline
```

This writes separate output to `.build/baseline-artifacts/` and leaves the
reference source unchanged.

## Arduino IDE

Open [`firmware/Axio/Axio.ino`](../firmware/Axio/Axio.ino), select
**XIAO_ESP32C3**, and use the same core and library versions. Set
**USB CDC On Boot: Enabled**. In the locked core, this corresponds to the CLI
option `CDCOnBoot=default`. With CDC disabled, `Serial` uses UART0 and the Axio
build rejects that configuration.

The generated `src/web/WebAssets.h` is included in the repository for IDE
builds. After editing the HTML/CSS/JavaScript sources, regenerate it first:

```sh
python3 -B scripts/package_web.py
```

## Upload and first connection

Uploading is a separate, intentional action. Select the actual USB port and
close Serial Monitor before running:

```sh
bash scripts/upload.sh --port /dev/cu.usbmodem2101
```

Replace the example port with your device's port. The script builds and uploads
with `EraseFlash=none`; it does not perform a factory reset. USB ports may
change after reset or upload.

With no usable saved network, the WPA2 fallback AP is requested after about
15 seconds offline. Join `Axio-<Tracker ID>` using the default password
`12345678` (or your saved AP password), then open `http://192.168.4.1/` manually.
The AP closes once STA connects. Continue with the
[network and dashboard instructions](user-guide.md).

## Optional USB tools

Real USB tests require `pyserial`; install it only if you need those tools:

```sh
python3 -m venv .venv
.venv/bin/python -m pip install -r tools/requirements-hardware.txt
.venv/bin/python -B tools/hardware_test.py --list-ports
.venv/bin/python -B tools/hardware_test.py --port /dev/cu.usbmodem2101 --metrics-seconds 10 --report captures/usb-01.json
```

This test does not upload, calibrate the reference or reset metrics by default.
Use a new report filename each time. Missing hardware/prerequisites produce
NOT RUN (exit 2); failed checks produce exit 1. Optional state-changing flags
are described by `--help` and in [Testing and benchmarks](benchmarks.md).
