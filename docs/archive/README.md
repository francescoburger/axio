# Historical archive

These documents record completed development phases, experiments, verification
sessions and superseded plans. They retain their original Italian text and
reported PASS/FAIL/NOT RUN outcomes. All 28 pre-reorganization documents are
preserved here without content changes, including the original baseline analysis
and benchmark plan. Dates, paths, hardware addresses and authorization notes
describe the sessions in which they were written.

For current instructions use the [English documentation](../README.md),
[user guide](../user-guide.md) and [validation summary](../validation.md).
Archived plans do not authorize new uploads, calibration, network changes or
destructive tests. Paths written as inline code refer to the repository layout
at the time; local captures/logs are not part of the Git distribution.

## Foundations and OSC

| Document | Historical purpose |
| --- | --- |
| [Baseline V1.1](baseline-v1.1.md) | Original source analysis and exact math |
| [Benchmark plan](benchmarks.md) | Original Phase 0 measurement procedure |
| [Phase 0 verification](phase0-verification.md) | Baseline reconstruction and fixtures |
| [Phase 1 verification](phase1-verification.md) | Modular core and regression evidence |
| [Phase 2 OSC](phase2-osc.md) | Schema 1 engine, USB configuration and Checkpoint B procedures |
| [Phase 2 verification](phase2-verification.md) | Checkpoints A/B, corrections and hardware results |

## Connectivity

| Document | Historical purpose |
| --- | --- |
| [Phase 3 connectivity](phase3-connectivity.md) | STA, protected AP, provisioning and discovery design |
| [Phase 3 verification](phase3-verification.md) | Checkpoints 3A–3D and real-device results |
| [3B hardware plan](phase3b-hardware-plan.md) | Original AP/provisioning test plan |
| [3C LAN plan](phase3c-lan-plan.md) | Original discovery test plan |

## Dashboard, network and endurance

| Document | Historical purpose |
| --- | --- |
| [Phase 4 implementation](phase4-implementation.md) | Dashboard implementation plan |
| [Phase 4 verification](phase4-verification.md) | Functional tests, user acceptance and outstanding coverage |
| [Button/battery correction](phase4-button-battery-fix.md) | Initial power and ADC fixes |
| [Endurance plan](phase4-endurance-plan.md) | One-hour test setup |
| [Endurance verification](phase4-endurance-verification.md) | Completed hour with HTTP failures |
| [HTTP contention](phase4-http-contention-verification.md) | Concurrent probe/browser limit accepted by the user |
| [Network verification](phase4-network-verification.md) | Coordinated restart and network interruption results |
| [mDNS decision](phase4-mdns-decision.md) | Failed delegated common-name experiment |
| [mDNS investigation](phase4-mdns-investigation.md) | Diagnosis of duplicate owners/discovery recovery |
| [Entry-point verification](phase4-entry-point-verification.md) | Native ownership, election and failover |
| [mDNS reconciliation](phase4-mdns-reconciliation.md) | Periodic announcements and follow-up |
| [mDNS partition verification](phase4-mdns-partition-verification.md) | Instrumented two-leader partition/rejoin test |

## Debugging and final wake recovery

| Document | Historical purpose |
| --- | --- |
| [Corrections 001–005](debug-corrections-verification.md) | Layout, terminology and full dashboard over AP |
| [Round 2](debug-round2-verification.md) | Navigation, configuration controls and numeric OSC triggers |
| [Round 3](debug-round3-verification.md) | XYZ logo and isometric Motion view |
| [Round 4](debug-round4-verification.md) | Peer links, address inputs and unsaved drafts |
| [Round 5](debug-round5-verification.md) | Button timing revision; physical wake subsequently failed |
| [Wake recovery](wake-recovery-verification.md) | Final LOW wake/release fix, upload and user-confirmed physical cycle |
