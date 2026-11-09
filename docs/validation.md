# Validation and known limits

This page summarizes the recorded state at the close of the first development
milestone, 9 November 2026. It does not report a new hardware session. The
original reports in [the archive](archive/README.md) preserve intermediate
failures, revisions and detailed methods. Their local `.build/` logs and
`captures/` files are ignored by Git and may not be available in a fresh clone.

## Recorded outcomes

| Check | Status | Scope and source |
| --- | --- | --- |
| Final wake-recovery host suite | PASS | 111 tests, including 1,345 unchanged mathematical vectors; [report](archive/wake-recovery-verification.md) |
| Frozen source and Phase 0 fixture | PASS | Hash/byte regression checks in the host suite |
| Final wake-recovery build | PASS | 1,149,265 B flash (87%), 111,172 B static RAM (33%); [report](archive/wake-recovery-verification.md) |
| Main-board upload and USB/HTTP recovery | PASS | IMU ready, valid web samples, networks/hostname and complete OSC settings preserved; [report](archive/wake-recovery-verification.md) |
| Physical sleep/wake after the recovery | PASS, user-reported manual check | User confirmed the cycle after upload; no instrumented duration/current measurement; [report](archive/wake-recovery-verification.md) |
| Dashboard interface | PASS, user acceptance | Five-page interface accepted; exhaustive viewport/error matrix not claimed; [Phase 4 report](archive/phase4-verification.md) |
| Dashboard over real AP | PASS, user-reported manual check | Observed dashboard use; not a complete AP provisioning/transition test; [Phase 4 report](archive/phase4-verification.md) |
| OSC reception, calibration and ACK | PASS, recorded LAN tests and user checks | Format/routing and manual calibration/ACK confirmed; [Phase 4 report](archive/phase4-verification.md) |
| Multi-network STA, protected fallback AP and discovery | PASS in tested paths | Two XIAO boards, second without IMU; [Phase 3 report](archive/phase3-verification.md) and [network report](archive/phase4-network-verification.md) |
| Common-name election and failover | PASS in tested paths | Two boards; individual identities preserved and failover observed in both directions; [entry-point report](archive/phase4-entry-point-verification.md) |
| Reconciliation of two active leaders | PASS, instrumented test | Controlled mDNS RX partition/rejoin; production restored on both; [partition report](archive/phase4-mdns-partition-verification.md) |
| One-hour endurance: overall criterion | FAIL | Window completed, IMU/OSC/settings checks passed, HTTP errors occurred; [endurance report](archive/phase4-endurance-verification.md) |
| Concurrent probe plus browser HTTP | FAIL; limitation accepted by user | 0 errors before browser, 9 during, 0 after closing it; firmware retained by agreement; [contention report](archive/phase4-http-contention-verification.md) |

The second board's expected `init_failed` state without a BNO055 is not an IMU
PASS. Hardware tests preceded different revisions: an earlier two-board
network result does not mean the final wake-recovery image was retested on
both boards.

## Performance evidence

The one-hour run recorded **359,161 successful IMU acquisitions**, averaging
**99.7646 Hz**, with zero new read errors. Separately, the Mac received
**345,954 valid OSC datagrams** and **1,037,862 messages** at about
**96.0983 datagrams/s**. HTTP sample requests produced **33,035 valid samples
from 33,045 requests**, with ten errors; an additional snapshot timed out.
These numbers belong to the tested endurance revision and conditions, not a
universal rate claim for every build.

The controlled HTTP comparison reproduced degradation under probe-plus-browser
load. The current one-connection HTTP runtime can delay or time out concurrent
clients. The user accepted this limitation; the original FAIL remains recorded.
Two separate browsers were not independently tested.

The final wake-recovery build has a small remaining application-partition
margin. The known external build warning is `OSCMessage.cpp:897`, implicit
fallthrough. Static RAM totals do not equal runtime free heap, and sampled
heap stability does not prove absence of leaks.

## Superseded failures

The original delegated common mDNS alias could advertise two A records and
failed exclusivity. Native responder ownership/election replaced it, followed
by periodic announcements for reconciliation. The failure remains in the
[historical mDNS decision](archive/phase4-mdns-decision.md) and the later
[reconciliation report](archive/phase4-mdns-reconciliation.md).

The round 5 HIGH-level wake approach failed the physical wake cycle after
passing host tests. The final recovery uses LOW wake with a pull-up and waits
for stable release before entering deep sleep, after stopping sensor/network
at the 5 s threshold. The new physical cycle was confirmed manually by the
user. Read the [recovery report](archive/wake-recovery-verification.md) rather
than using round 5 as the current power specification.

## Not run or not established

| Check | Status | Limit |
| --- | --- | --- |
| Endurance beyond the recorded hour | NOT RUN | No long-term stability/leak guarantee |
| Independent deep-sleep current and timing measurements | NOT RUN | Functional wake/SUSPEND readback is not a power measurement |
| ADC accuracy against an external instrument; state-of-charge curve | NOT RUN | Voltage only, no battery percentage/runtime guarantee |
| Physical NVS power-cut/wear tests | NOT RUN | Host journal fault tests do not replace electrical tests |
| Three or more XIAO boards and broad controlled radio/multicast loss | NOT RUN | Two-board cases do not prove universal convergence |
| Two independent concurrent browsers | NOT RUN | Probe-plus-browser failure is the measured case |
| Guaranteed zero packet loss/duplication | NOT RUN | Host/firmware windows differ; UDP enqueue is not delivery |
| Exhaustive mobile/desktop, import and error-state UI matrix | NOT RUN | Functional checks and visual acceptance have narrower scope |
| Full retest of real network/AP password changes and every AP→STA path on final firmware | NOT RUN | Avoided forcing changes to the user's saved networks |

No new upload, calibration, DIAG RESET, radio transition or power test is part
of the documentation reorganization. Use [Testing and benchmarks](benchmarks.md)
for reproducible checks and the distinction between host and hardware evidence.

## Documentation reorganization checks

Checks performed on 9 November 2026 for the English documentation and CLI text:

| Check | Status | Evidence and scope |
| --- | --- | --- |
| Host suite after translation | PASS | 111 tests, 1,345 unchanged math vectors; `.build/documentation-tests-authorized.log` |
| Initial suite in restricted sandbox | FAIL, environment restriction | Local socket binds were denied; original output retained in `.build/documentation-tests.log`, then the suite passed outside the sandbox |
| Archived document integrity | PASS | All 28 original documents match their pre-move SHA-256 values |
| Immutable baseline and fixture | PASS | Byte comparison with Git HEAD and baseline tests |
| Relative Markdown links | PASS | 135 links resolve to existing local targets, including archived reports |
| Python/shell syntax and translated CLI help | PASS | 28 Python modules parsed, build/upload shell syntax checked, six help entry points exercised |
| Firmware rebuild and new hardware tests | NOT RUN | Firmware changes are comments only; no upload or device commands required |

The documentation refresh changes guidance, organization, comments and human-readable
tool messages. It does not change protocol keys, command flags, pin values or
sensor calculations. No commit or push was performed.
