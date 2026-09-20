# Changelog

## 1.0.4 - 2026-09-20

Security and compatibility release for the Arduino Library Manager.

- Harden downlink parsing: reject malformed, truncated and out-of-range integer commands; prevent separators crossing command boundaries; accept only valid binary literals; scan malformed input linearly.
- Make V1 capacity calculations safe on 16-bit targets, bound counters, and check storage reservation before changing telemetry state. Preserve the full uint32 duration API and report allocation failure.
- Add V2 binary encoding with six modes and built-in profiles. Validate pointers, sample counts, numeric ranges, buffer capacity and frame/block lifecycle. Generate CRC-16/CCITT-FALSE when requested.
- Reject unrepresentable V2 sample arrays and oversized automatically selected blocks before writing them.
- Add safety regression suites, host corpus/profiler checks and the BasicV2 Arduino example. Exclude generated binaries from releases.
- Correct the documented decoder API and distinguish host profiling estimates from target MCU measurements.

Validation: Clang 22.1.8 with AddressSanitizer and UndefinedBehaviorSanitizer; all three safety suites and the V1/V2/downlink corpus passed. The optimized host profiler passed. BasicDelta, BasicDecoder and BasicV2 compiled for Arduino Uno and Mega with Arduino CLI 1.5.1 and AVR core 1.8.8. These are compilation checks, not hardware execution tests.

Compatibility: valid downlink commands keep the existing callback API. Previously accepted malformed or out-of-range commands are now ignored. V1 `length()` returns `size_t` to represent lengths correctly on AVR. Applications must continue checking encoder results and authenticating downlinks; CRC provides corruption detection, not authentication.
