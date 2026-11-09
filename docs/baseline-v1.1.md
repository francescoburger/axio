# HandTracker V1.1 baseline

Immutable source: [`reference/HandTrackerV1_1.ino`](../reference/HandTrackerV1_1.ino).
Expected SHA-256:

```text
40c8113e8cb8d0ef0229ffc50282933e889a94c3e8795f8dac942a1968aaf259
```

The original source is never renamed, reformatted or corrected. The frozen
[`tests/fixtures/phase0-Axio.ino`](../tests/fixtures/phase0-Axio.ino) differs only
by replacing its two pin definitions with the hardware configuration include.
Tests verify the hash and fixture bytes separately from the refactored production
core. The current main sketch is no longer the Phase 0 copy.

The [original baseline analysis](archive/baseline-v1.1.md) is archived unchanged.
This English reference preserves its mathematical specification and distinguishes
historical firmware behavior from the current implementation.

## Dependencies and initialization

The verified reconstruction uses ESP32 core 3.3.12, OSC (CNMAT) 4.0.0,
WiFiManager 2.0.17, Adafruit BNO055 1.6.4, Unified Sensor 1.1.15 and BusIO 1.17.4.
The versions used for the original historical build are unknown.

Historical `setup()` selects CPU 160 MHz and serial 115200, loads preferences,
starts Wi-Fi/portal and UDP, then initializes the sensor. Its `Wire.begin(5,6)`
means GPIO SDA=5/SCL=6, corresponding to D3/D4 in the locked XIAO variant.
The production wiring is **SDA GPIO6/D4, SCL GPIO7/D5**, BNO055 **0x29**,
defined in `firmware/Axio/hardware_config.h`.

The BNO055 sensor ID is 55, address 0x29 and default mode NDOF. On failed init,
the historical code sets `imuok=0` with no retry. On success it waits 1,000 ms,
applies `REMAP_CONFIG_P0=0x21`, `REMAP_SIGN_P0=0x04` and enables the external
crystal. It does not read internal calibration quality or save sensor offsets.

The pin mapping and gyro interpretation can be checked against the locked local
headers and library sources, also available as the
[ESP32 3.3.12 XIAO variant](https://github.com/espressif/arduino-esp32/blob/3.3.12/variants/XIAO_ESP32C3/pins_arduino.h)
and [Adafruit BNO055 1.6.4 implementation](https://github.com/adafruit/Adafruit_BNO055/blob/1.6.4/Adafruit_BNO055.cpp).

## Historical Wi-Fi and OSC

The V1.1 NVS namespace is `configs`; initial values are `init=true`,
`targetIp=0.0.0.0`, `port=16888`, `id=0`. WiFiManager owns network
credentials. Portal buffers are 40 bytes for IP, 5 for port and 3 for ID.
The callback uses `atoi`, `atol` into unsigned int and
`IPAddress.fromString` without explicit validation of their results.

Hostname/AP SSID is `Tracker_<DEVICE_ID>`, built only during setup in a 16-byte
buffer. `autoConnect` has no AP password. The portal is configured as
nonblocking and serviced on every loop; setup continues after a reported
connection failure. These are historical behaviors, not current Axio setup
instructions.

Each loop services the portal, checks `millis()-lastSensorUpdate >= 10`, assigns
`lastSensorUpdate=currentMillis`, calls `bno_loop()`, then handles incoming OSC.
There is no catch-up. Unsigned subtraction handles ordinary timer wrap, not
hard real-time deadlines. With `imuok=0`, the read loop returns immediately.

A historical acquisition reads quaternion, Euler, linear acceleration and gyro
separately and attempts one immediate bundle with four messages in this order:

| Address | Type tag | Values |
| --- | --- | --- |
| `/ypr<ID>` | `,fff` | Normalized yaw, pitch, roll |
| `/real<ID>` | `,fff` | Sensor linear acceleration /35 |
| `/world<ID>` | `,fff` | Acceleration rotated by relative quaternion /35 |
| `/gyro<ID>` | `,fff` | Truncated gyro components /1,500 |

The destination is `outIp:outPort`, default port 16888, local socket 16889.
Addresses use `snprintf` into `char addr[16]`. No quaternion, sample sequence,
acquisition timestamp or UDP outcome is transmitted. OSC 4.0.0 uses immediate
bundle timetag 1; it is not the sample time.

At most one incoming datagram is parsed per loop as an OSCMessage, not a bundle.
Valid `dispatch("/tare", osctare)` calls reference calibration regardless of
arguments, without acknowledgement or authentication. The historical handler
does not guard against `imuok=0`. Current command handling is described in
[Protocols](protocols.md).

## Reference calibration and quaternion

User calibration sets a reference pose; it is distinct from Bosch's internal
sensor calibration. The two reference reads are sequential:

```cpp
imu::Quaternion quat = bno.getQuat();
imu::Vector<3> euler = bno.getVector(Adafruit_BNO055::VECTOR_EULER);
quatOffset = quat.conjugate();
yawOffset = euler.x();
```

`yawOffset` is an `int`: assigning a double truncates toward zero
(359.9375 → 359, −0.9375 → 0). Quaternion components use double in the installed
library. For reference quaternion `t=(tw,tx,ty,tz)`, the offset is
`o=(tw,-tx,-ty,-tz)`: the **conjugate**, without division by squared norm.
Before the first calibration, the offset is identity and yaw offset is zero.
There is no automatic reference calibration or persistence.

Every sample computes `quatRelative = quatOffset * quat` in that order.
For current quaternion `u=(uw,ux,uy,uz)`, the Hamilton product is:

```text
w = ow*uw - ox*ux - oy*uy - oz*uz
x = ow*ux + ox*uw + oy*uz - oz*uy
y = ow*uy - ox*uz + oy*uw + oz*ux
z = ow*uz + ox*uy - oy*ux + oz*uw
```

Each line preserves the C++ left-to-right expression order. The four double
results are then assigned to float `q0,q1,q2,q3` (w,x,y,z). There is no
normalization or q/−q sign selection. At the reference pose, conjugate
multiplication yields identity only for a unit quaternion: `t=(2,0,0,0)`
gives `(4,0,0,0)`. Repeated calibration replaces rather than accumulates
the reference.

## Exact Euler conversions

```cpp
yaw   = ((((int)euler.x() - yawOffset + 360 + 180) % 360) / 360.0) * 2 - 1;
roll  = -1 * (int)euler.z() / 180.0;
pitch = (int)euler.y() / 90.0;
```

1. Each component is truncated toward zero, not floored or rounded. Reference
   yaw was truncated separately: `int(x)-int(t)` differs from `int(x-t)`.
2. Yaw uses integer subtraction, addition of **360 then 180**, integer remainder
   `%360`, double division by 360.0, multiplication by 2, subtraction of 1,
   then float assignment.
3. Roll computes `-1 * int(z)` as an integer before double division.
   Pitch divides `int(y)` by 90.0. Both results are assigned to float.

For heading and reference in [0,360), yaw has steps of 1/180 in [−1,179/180],
with a discontinuity at a +180° difference:

| Heading | Reference | Yaw before float rounding |
| --- | --- | --- |
| 0 | 0 | 0 |
| 179.9375 | 0 | 179/180 |
| 180 | 0 | −1 |
| 359.9375 | 0 | −1/180 |
| 0 | 359.9375 | 1/180 |
| 10.9 | 10.1 | 0; both truncate to 10 |

C++ remainder has the numerator's sign. At heading −901 and reference 0, the
numerator is −361, remainder −1 and yaw approximately −1.00556. Adding 540 is
not a general wrap for arbitrary inputs. There is no yaw/pitch/roll clamp.
Historical comments describe pitch ±90 and roll ±180 while code uses y/90 and
−z/180; preserve the formulas rather than swapping axes. Nonfinite values,
out-of-range integer casts and integer overflow have no portable C++ reference
result and are not assigned invented oracle values.

## Acceleration: operation order and precision

`VECTOR_LINEARACCEL` returns m/s². Double reads are assigned to float
`axReal,ayReal,azReal`, then rotated using the **relative** float quaternion:

```cpp
axWorld = (1 - 2 * q2 * q2 - 2 * q3 * q3) * axReal + (2 * q1 * q2 - 2 * q0 * q3) *
          ayReal + (2 * q1 * q3 + 2 * q0 * q2) * azReal;
ayWorld = (2 * q1 * q2 + 2 * q0 * q3) * axReal + (1 - 2 * q1 * q1 - 2 * q3 * q3) *
          ayReal + (2 * q2 * q3 - 2 * q0 * q1) * azReal;
azWorld = (2 * q1 * q3 - 2 * q0 * q2) * axReal + (2 * q2 * q3 + 2 * q0 * q1) *
          ayReal + (1 - 2 * q1 * q1 - 2 * q2 * q2) * azReal;
```

These products/sums are float, with the shown parentheses and associativity.
Sensor components remain unscaled until all three world components are
calculated. Then, in order `axReal,ayReal,azReal,axWorld,ayWorld,azWorld`,
each executes `value /= 35.0`: double promotion, division, then float assignment.

The historical comment mentions ±40 m/s²; the actual divisor is **35**.
There is no clamp: 70 m/s² produces 2. The world frame depends on calibration
and is not necessarily an absolute world frame. Nonunit quaternions distort
the matrix. Do not move scaling before rotation or replace this matrix.

## Gyroscope

```cpp
gx = int(gyro.x()) / 1500.0;
gy = int(gyro.y()) / 1500.0;
gz = int(gyro.z()) / 1500.0;
```

Double-to-int casts truncate toward zero, followed by double division and float
assignment. Examples: ±0.99 → 0; 1.99 → 1/1,500; −1.99 → −1/1,500; 3,000 → 2.
There is no clamp.

The V1.1 comment says rad/s. In Adafruit BNO055 1.6.4,
`getVector(VECTOR_GYROSCOPE)` divides registers by 16 and declares degrees/s;
it does not apply the radians conversion used elsewhere by `getEvent`.
The original calculation is preserved.

## Historical risks and regression coverage

Historical issues are documented, not corrected in the frozen source:

- Different I²C pins from current wiring; address and external crystal depend
  on the physical breakout.
- Sequential reference/sample reads do not necessarily describe one instant;
  no internal fusion-quality check.
- Truncations, yaw discontinuity, no clamps, unnormalized conjugate, relative
  world frame and misleading historical scale/unit comments.
- Initial target 0.0.0.0, unvalidated network input/outcomes, open historical AP
  and no sender filtering for calibration.
- Unbounded `sprintf(wifiName, "Tracker_%d", DEVICE_ID)` into 16 bytes can
  overflow for arbitrary NVS IDs. Portal buffers may truncate long IDs or
  five-digit ports because they include space for a terminator.
- No application init retry, calibration allowed after init failure, unused
  `bnoInitialized`, and no sample sequence/timestamp.
- A 10 ms requested period does not prove 100 distinct acquisitions or reliably
  received packets per second.

Host tests extract the original C++ functions and exercise both baseline and
production core using real installed `imumaths` operations. An independent
Python oracle models float32, truncation and C++ remainder. The 1,345 historical
vectors include yaw degrees/fractional references, wrap and signs,
noncommutative/nonunit quaternions, out-of-scale acceleration, sub-degree gyro
and 250 fixed-seed random samples. Absolute tolerance is 1e−7 or relative 2e−6
for host/target differences; this is not a bit-for-bit ESP32 or physical test.
Textual fixture/hash checks remain separate.
