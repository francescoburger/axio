# Protocols

## OSC configuration

Current dashboard configurations use **schema 2**, with a global string ID,
numeric IPv4 destinations, listener/calibration/ACK settings and either an
editor definition or explicit messages. Schema 1 remains supported for older
configurations and the preset generator; its `hz`, routing and bundle controls
are historical compatibility fields. The dashboard edits the schema 2 single
editor representation.

Example complete schema 2 editor configuration:

```json
{
  "schema": 2,
  "id": "3",
  "listen_port": 16889,
  "calibration_address": "/calibration",
  "ack": {"enabled": false, "address": "/calibrated/{id}", "destination": 0},
  "destinations": [{"ip": "192.168.1.4", "port": 16888}],
  "editor": {
    "main_address": "/motion/{id}",
    "interval_ms": 20,
    "suffixes": ["orientation", "real", "world", "gyro", "quaternion"],
    "selected": [7, 7, 7, 7, 15]
  }
}
```

Replace the destination IP with the receiver's address. `selected` contains bit
masks for the fields in each group, in the catalog order below. Suffixes are
stored without the UI's leading slash. Equal expanded group addresses merge
their selected fields in group order. Schema 2 applies all messages to all
destinations; a single due message is plain OSC, multiple due messages form an
immediate bundle (timetag 1).

Validate/apply accept a complete object rather than a partial patch. Unknown,
duplicate or missing properties are rejected. Validation has no persistent
effect; Apply commits settings before activation. CONFIG GET returns canonical
JSON. Old `tare_address` keys are accepted, while replies use
`calibration_address` without changing saved endpoint or ACK values.

IDs contain 1–16 ASCII letters, digits, `_` or `-`. Addresses begin with `/`,
contain a nonempty path, and exclude trailing/double slashes, whitespace and
OSC pattern characters. `{id}` is the only substitution token. Expanded
addresses fit in 79 bytes plus the terminator; data, calibration and ACK
addresses must not collide. Destinations are unique numeric IPv4/port pairs;
hostname resolution is not used for OSC destinations. The validator excludes
first octet 0 or ≥224 and final octet 255; ports are 1–65,535.

| Resource | Current technical bound |
| --- | ---: |
| Destinations | 8 |
| Explicit messages | 16 |
| Arguments per message | 16 |
| OSC datagram | 1,400 B |
| Configuration JSON | 7,168 B |
| Schema 2 interval | 10–65,535 ms, integer |
| Schema 1 frequency | 1–100 Hz, integer |

These are provisional implementation bounds, not benchmarked product capacity.
Worst-case bundles must also fit the datagram budget, so not all maximum-size
combinations are valid. Requested frequency/interval does not guarantee timing
or delivery. The engine uses only fresh complete samples and skips backlog.

## OSC field catalog

Every data argument uses OSC float32 (`f`). No core value is clamped.

| Group order | Fields in argument order | Meaning |
| --- | --- | --- |
| Orientation | `yaw`, `pitch`, `roll` | Integer-truncated Euler scaling; yaw includes the saved integer reference |
| Real Acceleration | `axReal`, `ayReal`, `azReal` | Sensor-frame linear acceleration in m/s² divided by 35 |
| World Acceleration | `axWorld`, `ayWorld`, `azWorld` | Historical rotation using the relative quaternion, divided by 35 |
| Gyroscope | `gx`, `gy`, `gz` | Gyro in °/s truncated toward zero, divided by 1,500 |
| Quaternion | `q0`, `q1`, `q2`, `q3` | Relative quaternion w, x, y, z; reference conjugate × current |

The quaternion is not normalized by the core. Nominal ranges may be exceeded.
The world frame depends on reference calibration; it is not an absolute world
frame. Exact yaw remainder behavior, float precision and order of operations
are specified in [V1.1 baseline](baseline-v1.1.md).

## OSC calibration

Send a single OSC message to the configured listener port and calibration
address. Zero arguments, a single int32 trigger or a single finite float32
trigger are accepted; the trigger value does not set the reference. Bundles,
pattern addresses and additional arguments are not accepted.
Default port: **16889**, default address: **`/calibration`**.

When calibration succeeds, optional acknowledgement is one OSC message at the
configured ACK address with type tag `,i` and value `1`, sent to the selected
zero-based destination index. A failed/missing IMU produces no success ACK.
Local UDP acceptance does not prove ACK delivery. Calibration replaces the
quaternion and yaw references in RAM and does not write them to NVS.

Default `/tare` and `/calibration` endpoint aliases are retained where they do
not conflict with configured output/ACK addresses; custom endpoints are not
silently aliased. Existing saved `/tare` addresses and ACKs are preserved.

## USB diagnostics

USB CDC requests are ASCII lines, with one request in flight:

```text
AXIO 1 PING
{"axio":1,"id":"1","ok":true,"data":{"pong":true}}
```

The ID is 1–16 ASCII letters, digits, `_` or `-`. LF terminates a line; CRLF is
accepted. The line limit is 8,192 bytes excluding LF, the reply buffer is
8,192 bytes and the absolute line timeout is 1,000 ms. JSON syntax depth is
limited to eight. Responses correlate the ID, or use `id:null` if it cannot be
recovered. There is no periodic diagnostics output. The Python client uses
session-specific IDs to ignore delayed responses after USB reopening.

| Command | Effect |
| --- | --- |
| `PING` | Communication check |
| `INFO`, `STATUS`, `METRICS` | Identity, state and diagnostic counters/timing |
| `CONFIG GET` | Read complete active OSC configuration |
| `CONFIG VALIDATE <json>` | Validate a complete OSC candidate without applying |
| `CONFIG APPLY <json>` | Validate, persist and activate OSC configuration |
| `CALIBRATION` (`TARE` alias) | Change quaternion and yaw references in RAM |
| `DIAG RESET` | Reset diagnostic windows; preserve settings/reference |
| `WIFI GET` | Read SSID hex and priority without passwords |
| `WIFI SET <priority> <ssid_hex> <password_hex-or-dash>` | Save/update a network; `-` means open STA |
| `WIFI DELETE <ssid_hex>` | Remove the selected saved network |
| `AP CREDENTIALS` | Explicit USB read of AP SSID and password |
| `AP REKEY` | Restore the fixed default AP secret; refused when an AP password is managed by network settings |
| `DISCOVERY GET` | Read the peer cache without starting a scan |

Wi-Fi priorities are 0–255, with larger values tried first; ties preserve
insertion order. SSIDs are 1–32 bytes. Hex is transport encoding, not encryption.
STA passwords can be empty, 8–63 printable ASCII characters or a 64-digit hex
PSK. SET/DELETE commit before activation and may interrupt connectivity.
The AP always requires WPA2. Credential reads are not part of periodic status.

IMU rates come from acquisition counters/time windows. Engine `udp_attempts`
and `udp_queued` measure local send attempts/acceptance, not network delivery.
Discovery and connectivity counters have boot-lifetime scopes separate from
resettable IMU/OSC windows. See [measurement rules](benchmarks.md).

## HTTP API

The dashboard uses HTTP on the active STA/AP address. Mutations check the Host,
Origin and per-boot session token. This is a local, unauthenticated interface,
not a public cloud API. Network changes can disconnect the client after commit.

| Method and route | Operation |
| --- | --- |
| GET `/api/session` | Obtain browser session token |
| GET `/api/snapshot` | Device, state, OSC, networks without passwords, peers and diagnostics |
| GET `/api/sample` | Complete cached motion sample, refreshed at most at 10 Hz |
| GET `/api/battery` | Voltage; percentage unavailable |
| GET `/api/power` | Power transition state, errors and wake cause |
| POST `/api/identity` | Save the global Tracker ID |
| POST `/api/osc/validate`, `/api/osc/apply` | Validate or persist/activate OSC settings |
| POST `/api/network/validate`, `/api/network/apply` | Validate or persist/activate networks and AP password |
| POST `/api/command` | Restricted calibration/diagnostics commands |
| POST `/api/control` | Confirmed Restart/Sleep with response before transition |

Network updates carry a revision and original record references to preserve
unchanged secrets and reject stale edits. HTTP is served by one connection at
a time; concurrent-client timeouts are a [known accepted limit](validation.md).
