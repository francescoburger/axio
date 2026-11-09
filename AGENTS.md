# Axio — repository rules

- Axio project, axio repository; Seeed XIAO ESP32C3 and BNO055, macOS,
  Arduino IDE and CLI. Main sketch: `firmware/Axio/Axio.ino`.
- Phase **2 completed and approved** by the user, including Checkpoints A and B.
  Checkpoint 3A/3B/3C software completed; Checkpoint 3D authorized:
  upload and testing on the XIAO, Wi-Fi/AP/mDNS/discovery/OSC integration.
  Preserve NVS and existing networks; no reference calibration, DIAG RESET or
  destructive tests. Stop at the end of 3D. Do not start the dashboard or Phase 4
  without approval. No commit, merge or push without explicit approval.
  These are the original phase authorization gates; historical completion
  reports do not authorize new hardware work. The current documentation task
  does not extend hardware or feature permissions.
- The fixed AP password `12345678` was explicitly requested by the user;
  it supersedes the previous random-secret requirement. WPA2 remains mandatory.
- `reference/HandTrackerV1_1.ino` is immutable: no renaming, fixes,
  formatting or replacement. Expected hash in `tests/test_baseline.py` and
  `docs/baseline-v1.1.md`. Document historical defects rather than fixing them.
- Preserve quaternion **and yaw** reference calibration exactly, including
  multiplication order, truncation toward zero, C++ remainder, precision and
  operation order. Do not substitute inverses, normalization, clamps or
  different conversions.
- Centralize every pin through `#define` in
  `firmware/Axio/hardware_config.h`. Confirmed wiring: SDA GPIO6/D4,
  SCL GPIO7/D5; BNO055 address 0x29 in the same file. I²C uses these definitions.
- The Phase 0 copy is frozen in `tests/fixtures/phase0-Axio.ino`: it differs
  from the baseline only by the hardware include. Preserve the byte check and
  all historical vectors/tests, together with production core tests.
- Maintain Arduino IDE compatibility: matching sketch and folder names;
  modules in `src/core/` and `src/transport/`. Transport serializes the sample
  without performing I²C reads.
- Initial references are identity quaternion/zero yaw; no automatic reference
  calibration. The single command entry point is
  `ImuManager::calibrateReference()`. Publish only complete reads; on error,
  retain the last valid sample and an explicit state. No new init retries.
- Use a 10 ms scheduler without catch-up, unsigned `millis()` and a monotonic
  64-bit timestamp. Do not claim timing equivalence or performance without hardware.
- Work inside the repository; no installations, external changes or destructive
  actions without approval. No publishing or remote pushes.
- Avoid unnecessary dependencies. Scripts, builds, caches and tests write to
  `.build/`; captures go to `captures/`. Do not commit credentials or captures.
- Commands: `bash scripts/compile.sh`, `bash scripts/compile.sh --baseline`,
  `python3 -B -m unittest discover -s tests -v`. These do not upload firmware.
- Classify every check as PASS, FAIL or NOT RUN; record limits and warnings.
  Never invent hardware results; OSC reception does not measure the IMU rate.
- Keep active and newly written documentation, CLI help and user-facing text
  in English. Historical reports belong in `docs/archive/`, retain their
  original language and evidence, and are indexed by its English README.
  Do not rewrite immutable sources to translate their historical comments.

Product and future-phase constraints:

- Configurable OSC authorized at Checkpoint B; LAN web dashboard without
  authentication, multiple Wi-Fi networks, protected fallback AP, mDNS,
  button, LED, battery and deep sleep.
- No requirement to retain the Legacy OSC protocol in the final version.
- No fixed product limits on OSC destinations or messages before benchmarks.
- No configuration import/export or automatic shutdown for low battery.
  Do not add these behaviors on your own initiative.
