# User guide

## Connect to Axio

Axio tries saved Wi-Fi networks by priority. A successful connection stays
active even if another saved network becomes available. After about 15 seconds
without a usable STA connection, it enables a WPA2 fallback AP while continuing
to try saved networks.

Join the AP named `Axio-<Tracker ID>` and open `http://192.168.4.1/` manually.
The default password is `12345678`; a previously saved AP password takes
precedence. The full dashboard is available over AP. OSC and mDNS are suspended
while AP is enabled. The AP closes after STA connects, so the browser may need
to reconnect using the device's LAN IP or hostname.

On the same LAN, open `http://axio.local/`. With several trackers, one device
owns this common entry point; it may be a different tracker from the one you
last visited. Each tracker also keeps its individual
`axio-<12 lowercase hexadecimal MAC digits>.local` hostname. **Network** shows
the hostname, IP and common entry-point state. Peer links open individual
trackers in a new tab.

mDNS depends on local multicast. Guest networks, client isolation, VLANs and
firewall/router settings may prevent resolution or discovery. Try the device's
IP address if `.local` is unavailable. Automatic failover of the common name
can have a visible interruption and is not a guarantee during network partitions.

## Overview

View the tracker identity, status and discovered peers. This page is the single
place to edit the global **Tracker ID**, used by OSC `{id}` placeholders and the
AP name. The individual hardware hostname remains independent of that ID.

**Restart** and **Sleep** require confirmation. Saved settings survive restart;
the in-memory calibration reference starts again at identity quaternion and
zero yaw. Sleep is deep sleep rather than an electrical power switch.
Every reset, including **Restart**, leaves Axio logically off; release the
physical button and hold it again to resume operation.

The battery header displays measured voltage. A missing measurement is shown as
`—`. State of charge, charging status and remaining runtime are not estimated.

## Motion

Motion shows orientation, sensor-frame linear acceleration, relative world-frame
acceleration and gyroscope data. Quaternion values and the isometric orientation
view can be shown together. The view maps yaw to Y, pitch to X and roll to Z;
this display transform does not change transmitted data or core calculations.

Telemetry is requested while Motion is visible and Live. **Pause**, leaving the
page or hiding the browser page stops those requests. The client targets 10 Hz
and backs off to 5 Hz after errors; this display rate is separate from IMU
acquisition and OSC transmission.

Place the tracker in the desired reference pose before selecting **Calibration**
or briefly pressing and releasing the physical button. This replaces the
quaternion and yaw references in RAM. It is reference calibration, distinct
from the BNO055's internal sensor calibration. It is not saved across restart,
and no automatic reference calibration occurs at boot.

Values retain V1.1 scaling and are not clamped to ±1. “World” acceleration is
relative to the chosen reference pose. See the [field catalog](protocols.md#osc-field-catalog)
and [baseline mathematics](baseline-v1.1.md) for the exact conventions.

## OSC

Add the receiving computer's numeric LAN IPv4 address and UDP port under
**Destinations**. All destinations receive all configured messages. A loopback
default is not a route to your computer; replace it with an actual destination.
The current implementation permits up to eight destinations; this is a
provisional resource bound, not a benchmarked capacity guarantee.

Set **Main Address**, choose fields in the five groups and enter group suffixes.
The preview shows expanded addresses and argument order. `{id}` expands to the
global Tracker ID. Groups sharing an address are merged in group order. An empty
suffix uses Main Address directly; deselected groups produce no message.

**Send Interval** is an integer from 10 to 65,535 ms. One due message is sent
as a plain OSC message; several due messages are bundled together. Scheduling
uses fresh complete samples, so the interval is a request rather than a promise
of exact packet timing. Every data argument is OSC float32.

**Validate** checks the draft without saving it. **Apply** validates, persists
and activates it. Changes remain drafts until applied; navigation warns about
unsaved edits. Keep the receiver on the same reachable network and allow its
UDP port through the host firewall where needed.

Under **Advanced**, set the listener port, calibration address and optional
calibration acknowledgement address/destination. The default listener is 16889
and the default command address is `/calibration`. Existing saved `/tare`
addresses remain supported. See [calibration over OSC](protocols.md#osc-calibration).

The existing **Configuration Files** controls handle OSC editor JSON: Import
loads a validated draft and still requires Apply; Export downloads the draft.
They do not back up Wi-Fi credentials or the whole device. The imported Tracker
ID is replaced with the current global ID.

## Network

Save up to eight networks and order them from highest to lowest priority. Saved
passwords are not prefilled. Leaving a saved password untouched preserves it;
explicitly clearing a STA password selects an open network. Renaming or
reordering an existing row preserves its secret through its original record
reference. A new network needs its credentials entered explicitly.

The AP password field starts empty to mean unchanged. Enter a replacement if
needed; the AP remains protected with WPA2. Use **Validate** before **Apply**.
Applying saves the configuration before changing radio settings and may
disconnect the browser. Reconnect by individual hostname/IP or fallback AP.
If the response is lost, inspect the device's current state before repeating
the change: it may already have been committed.

There is no captive DNS redirect, Internet sharing or automatic OSC routing
based on discovered peers. The dashboard has no LAN user authentication;
its per-boot token and Host/Origin checks protect browser requests and do not
constitute account-based access control.

## Diagnostics

Refresh system, sensor, OSC and network metrics on demand. **Detailed Metrics**
exposes additional counters. **Reset Metrics** requires confirmation and clears
resettable counters while keeping saved settings and the calibration reference.
Boot-lifetime values such as minimum heap have a different scope.

An IMU state of `init_failed` means sensor initialization failed; a board
without a BNO055 can still serve the dashboard and participate in discovery.
On a read error, the last valid sample remains available with an explicit error
state; its timestamp and sequence do not advance. Stale data is not sent as a
new OSC sample.

## Physical button and sleep

- Brief press and release while running: calibrate the reference.
- Hold for 5 seconds: suspend the sensor and stop communications. The LED then
  flashes three times. Deep sleep starts after both the LED sequence completes
  and the button has been released stably for 100 ms.
- Hold for about 3 seconds to wake. The internal acceptance threshold is 1.6 s;
  perceived wake time includes boot. A button held during startup must be
  released before normal button actions are armed.
- Connecting or disconnecting USB, restoring power after battery discharge,
  and any reset leave Axio logically off. After power is available, release
  the button and make a fresh long press. A button already held during power
  restoration must first be released for 100 ms before deep sleep is armed.

Normal startup requires both a deep-sleep reset and the board's button wake
cause, followed by the existing long-press check. Other boots return to sleep
before IMU, application USB diagnostics, Wi-Fi, OSC or HTTP initialization.
No power-state flag is written to flash. See
[button-only boot validation and hardware limits](button-boot-validation.md).

Wake uses the active-LOW button with a pull-up: GPIO5 on C3, GPIO4 with RTC
EXT0 on S3. A long sleep press does not also calibrate
on release. The sensor's SUSPEND setting is read back before sleep; preparation
errors prevent normal completion and trigger a recovery attempt. Deep-sleep
current and exact physical timing have not been independently measured.

## LED feedback

| Event | Signal |
| --- | --- |
| Startup or accepted wake | One 500 ms pulse after firmware setup completes |
| Successful reference calibration | Two 100 ms pulses, separated by 100 ms off |
| Entering deep sleep | Three 100 ms pulses, separated by 100 ms off, after the sensor and communications stop |

Each signal ends with a 100 ms dark interval. The LED is off during normal
operation and is held at its inactive level for deep sleep. Startup feedback
indicates that firmware setup has finished; it is not a sensor-health check.
A rejected short wake press keeps the LED off and returns to sleep.

Calibration feedback is shared by the physical button, dashboard, USB and OSC,
including supported legacy aliases. Failed calibration does not generate a
confirmation. A calibration arriving during startup waits until the startup
signal finishes. Repeated requests during a signal coalesce into one pending
confirmation. Shutdown takes priority and clears pending confirmations;
sleep-preparation recovery cancels the shutdown signal.

The patterns use the normal loop without blocking delays. Delayed loop service
can extend a pulse; missed transitions are not replayed in a burst. C3 uses an
external LED on GPIO3 / D1, active HIGH; S3 uses its onboard GPIO21 user LED,
active LOW. Both pin and polarity are set in
[`hardware_config.h`](../firmware/Axio/hardware_config.h). See
[Getting started](getting-started.md#hardware) for wiring.

## Known operating limits

The HTTP server handles one connection at a time. Concurrent clients can cause
timeouts: this was reproduced with a probe plus a browser and accepted for the
current milestone. Two distinct browser clients were not separately tested.
Longer endurance, battery accuracy and network partition coverage also have
limits. Consult [validation and known limits](validation.md) before relying on
a particular performance or availability target.
