# ASXNanoStream Encoder (C++)

> **A single-header C++ library to compress IoT payloads by up to 99% using the ASXNanoStream Protocol.**

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Platform](https://img.shields.io/badge/Platform-Arduino%20|%20ESP32%20|%20STM32-blue)](https://github.com/)

## 🚀 Overview

**ASXNanoStream** is a domain-specific protocol designed to replace JSON in bandwidth-constrained environments like **LoRaWAN, Sigfox, and Satellite IoT**.

This library allows any microcontroller (Arduino, ESP32, etc.) to generate highly compressed ASX payloads on the fly, without complex memory allocation or external dependencies.

### Why use this?
| Feature | Standard JSON | ASXNanoStream |
|---------|---------------|---------------|
| **Payload Size** | Heavy (keys, brackets) | **Tiny** (RLE + Delta Encoding) |
| **Cost (Satellite)** | $$$ High | **$ Low** |
| **Memory Usage** | High (JsonBuffer) | **Low** (String stream) |
| **Format** | `{"v": [25, 25, 25]}` | `B25#x3-0v` |

---

## 📦 Installation

This is a **header-only** library. No complex build process required.

1. Install **ASXNanoStream** from the Arduino IDE Library Manager, or download a release ZIP and use **Sketch > Include Library > Add .ZIP Library**.
2. For a manual installation, copy the complete library, including every header in `src/`.
3. Include it in your code:
   ```cpp
   #include <ASXNanoStream.h>
   ```


---

## ⚡ Usage Examples

### 1. Basic Setup

```cpp
#include <ASXNanoStream.h>

AsxNanoStream asx;

void setup() {
  Serial.begin(115200);
}
```

### 2. Sending Sensor Telemetry (Analog/Delta)
Perfect for Temperature, Humidity, or Battery levels. The library automatically calculates deltas (`+1v`, `-5v`) and compresses repetitions (`xN-`).

```cpp
void loop() {
  asx.reset();
  // Set baseline value (e.g., 25.0°C -> 250)
  asx.setBaseline(250);

  // Add readings (Library will compress this automatically)
  asx.addAnalog(250, 5000); // 25.0°C
  asx.addAnalog(250, 5000); // 25.0°C (No change -> x2)
  asx.addAnalog(251, 5000); // 25.1°C (+1 delta)

  String payload = asx.getPayload();
  // Output: "B250#x2-+0v5000ms+1v5000ms"

  LoRa.send(payload);
}
```

### 3. Sending Binary States (Status/Heartbeat)
Ideal for digital inputs, relays, or simple status flags.

```cpp
void sendHeartbeat() {
  asx.reset();
  // 5 consecutive ON readings with duration (Library compresses into RLE)
  for(int i=0; i<5; i++) {
      asx.addBinary(true, 50);
  }

  // Output: "x5-1b50ms" (Compressed RLE)
  Serial.println(asx.getPayload());
}

void sendOptimalLiteral() {
  asx.reset();
  // 2 consecutive ON readings without duration
  asx.addBinary(true);
  asx.addBinary(true);

  // Output: "1b1b" (Optimal literal repetition: 4 bytes vs 5 bytes for "x2-1b")
  Serial.println(asx.getPayload());
}
```

---

## 🔧 API Reference

| Method | Description | Example |
|:-------|:------------|:--------|
| `AsxNanoStream(initialCapacity)` | Constructor with optional pre-allocated buffer capacity. | `AsxNanoStream asx(128);` |
| `reserve(bytes)` | Pre-allocates buffer memory to prevent dynamic heap fragmentation on low-SRAM MCUs. | `asx.reserve(128);` |
| `reset()` | Clears the buffer for a new message. | `asx.reset();` |
| `setBaseline(int32_t value)` | Sets the starting value for delta encoding (flushes pending commands). | `asx.setBaseline(250); // B250#` |
| `addAnalog(int32_t value, durationMs)` | Adds an analog reading. Calculates delta (`+N/-N`), checks RLE vs literal cost. | `asx.addAnalog(260, 100); // +10v100ms` |
| `addBinary(bool state, durationMs)` | Adds a binary state (`1b`/`0b`). Chooses smaller representation (e.g. `1b1b` vs `x3-1b`). | `asx.addBinary(true); // 1b` |
| `length()` | Returns the exact payload size in bytes without mutating state or flushing buffers. | `int bytes = asx.length();` |
| `getPayload()` | Flushes pending buffers and returns the final compressed ASX string. | `String s = asx.getPayload();` |

> [!TIP]
> **SRAM Memory Best Practice for 8-bit Microcontrollers (e.g., Arduino Uno / ATmega328P with 2 KB SRAM):**
> Pre-allocate your typical packet size using `asx.reserve(128)` (or `AsxNanoStream asx(128)`) during setup. Always call `asx.reset()` after transmitting each packet. This prevents dynamic heap allocations and memory fragmentation across long-running telemetry loops.

---

## ⚡ ASXNanoStream V2 (Binary Codec)

For next-generation deployments, the library includes the **ASXNanoStream V2 Binary Codec** ([`AsxEncoderV2.h`](src/AsxEncoderV2.h)) with immutable physical profiles ([`ASXProfileV2.h`](src/ASXProfileV2.h)).

### Highlights
- **100% Zero Heap Allocation**: Operates on a pre-allocated stack or static buffer.
- **Adaptive Auto-Selector**: Evaluates all 6 compression modes (`RAW`, `CONSTANT`, `DELTA_VARINT`, `DELTA_BITPACK`, `RANGE_BITPACK`, `DELTA_RLE`) in static memory and selects the smallest byte representation automatically.
- **Energy-Optimized Tie-Breaker**: Prioritizes lowest MCU CPU/battery consumption on byte ties.
- **Buffer & MTU Protection**: Built-in capacity checks (`CanFit`, `GetRemainingCapacity`, `BeginFrame`, `EndFrame`).

### V2 Usage Example

```cpp
#include <AsxEncoderV2.h>
#include <ASXProfileV2.h>

// Static buffer: No dynamic memory allocations
uint8_t txBuffer[128];
AsxEncoderV2 v2(txBuffer, sizeof(txBuffer));
uint32_t seqNumber = 1;

void sendV2Telemetry() {
    // 1. Begin V2 Binary Frame (Magic 0xA2, Profile 1, Version 1, Sequence, Flags=0)
    if (!v2.BeginFrame(1, 1, seqNumber++)) return;

    // 2. Prepare 60 samples (e.g. 25.0°C oscillating to 25.1°C)
    int64_t tempReadings[60];
    for (int i = 0; i < 60; i++) {
        tempReadings[i] = (i % 2 == 0) ? 250 : 251;
    }

    // 3. Encode block: Auto-Selector chooses RANGE_BITPACK (11 bytes body vs 120 bytes RAW!)
    if (!v2.EncodeBlockAuto(0, AsxDataType::Int16, tempReadings, 60)) return;

    // 4. End frame (patches block count in-place)
    if (!v2.EndFrame()) return;

    // 5. Transmit raw binary buffer
    LoRa.write(v2.GetBuffer(), v2.GetLength());
}
```

### V2 API Reference

| Method | Description |
|:-------|:------------|
| `AsxEncoderV2(uint8_t* buffer, size_t capacity)` | Constructor requiring a pre-allocated buffer. |
| `BeginFrame(profileId, version, seq, flags, timestamp)` | Writes V2 header (`0xA2`), metadata and reserves block count space. |
| `EndFrame()` | Patches the final block count in-place. |
| `EncodeBlockAuto(channelId, type, samples, count)` | Selects and writes the best compression mode for the block. |
| `EncodeBlockRAW(...)` | Explicitly writes RAW Little-Endian values. |
| `EncodeBlockCONSTANT(...)` | Explicitly writes 1 value for $N$ identical samples. |
| `EncodeBlockDELTA_VARINT(...)` | Explicitly writes delta varints with ZigZag/ULEB128. |
| `EncodeBlockDELTA_BITPACK(...)` | Explicitly writes bit-packed ZigZag deltas. |
| `EncodeBlockRANGE_BITPACK(...)` | Explicitly writes bit-packed offsets from minimum value. |
| `EncodeBlockDELTA_RLE(...)` | Explicitly writes run-length encoded delta sequences. |
| `CanFit(size_t bytes)` | Verifies buffer capacity prior to writing. |
| `GetRemainingCapacity()` | Returns available bytes in buffer before MTU limit. |
| `GetLength()` / `GetBuffer()` | Accesses the encoded binary frame for radio transmission. |

---

## Memory and resource validation

V2 uses the caller's buffer and does not allocate heap memory. V1 and the downlink decoder use Arduino `String`; reserve sufficient space and check encoder return values. Allocation failures must not be treated as successfully encoded telemetry.

`tests/HardwareResourceProfiler.cpp` measures host execution and checks V2 allocations. Host timing, object size and modeled battery life are estimates, not measured AVR flash, SRAM or battery guarantees. Compile the examples for the actual target and account for stack, input samples and the radio library in the application's memory budget.

---

## 📥 Embedded Decoder Light (`AsxNanoStreamDecoderLight.h`)

The callback API accepts complete `K<key>:<integer>#` commands and `0b`/`1b` binary literals. Values must fit the target's `int` (16 bits on AVR). Malformed commands, overflow and incomplete tokens do not dispatch callbacks; parsing resumes after the next `#`. The parser tolerates noise between commands and uses Arduino `String`, including for callback keys.

```cpp
#include <AsxNanoStreamDecoderLight.h>

void onCommand(String key, int value) {
    if (key == "IRRIG" && (value == 0 || value == 1)) {
        digitalWrite(RELAY_PIN, value ? HIGH : LOW);
    }
}

AsxNanoStreamDecoder decoder(onCommand);
// After receiving a complete, authenticated downlink:
// decoder.parse(String("KIRRIG:1#"));
```

The application must authenticate downlinks and enforce its actuator allowlist and value limits before operating hardware. CRC detects corruption; it does not authenticate a sender.

---

## 🧪 Test & Validation Harnesses

The library includes native C++ test and benchmark harnesses in `tests/`:

| Harness | File | Purpose |
|:--------|:-----|:--------|
| **Corpus Hardware Runner** | [`tests/CorpusHardwareRunner.cpp`](tests/CorpusHardwareRunner.cpp) | Checks canonical V1 outputs, emits V2 fixtures and validates Decoder Light callbacks on native compilers (Clang, GCC, MSVC, Arduino shim). |
| **Resource Profiler** | [`tests/HardwareResourceProfiler.cpp`](tests/HardwareResourceProfiler.cpp) | Measures host execution time, models MCU costs, and checks V2 heap allocation via operator new override. |
| **Firmware Size Probe** | [`tests/FirmwareSizeProbe.cpp`](tests/FirmwareSizeProbe.cpp) | Compiles isolated V2 object files and analyzes `.text`, `.data`, `.bss`, `.rdata` using `llvm-size`. |

### Compiling & Running Locally (MinGW / Clang / GCC)

```bash
mkdir -p build
# 1. Compile and run Corpus Hardware Runner
clang++ -O3 -std=c++17 tests/CorpusHardwareRunner.cpp -Isrc -Itests -o tests/corpus_hardware_runner.exe
./tests/corpus_hardware_runner.exe

# 2. Compile and run Hardware Resource Profiler (with 0-heap verification)
clang++ -O3 -std=c++17 tests/HardwareResourceProfiler.cpp -Isrc -Itests -o tests/hardware_resource_profiler.exe
./tests/hardware_resource_profiler.exe

# 3. Analyze isolated firmware object size with llvm-size
clang++ -c -O3 -std=c++17 tests/FirmwareSizeProbe.cpp -Isrc -Itests -o tests/firmware_size_probe.o
llvm-size tests/firmware_size_probe.o
```

---

## V2 input and frame safety

The V2 integer API accepts Boolean (0/1), Int16, UInt16, Int32 and UInt32 values within their wire ranges. It rejects Float32 instead of silently truncating it. Sample pointers must refer to an array with at least the declared number of entries; null pointers, zero counts and counts exceeding 65,536 or the target's addressable array size are rejected. A frame permits at most 256 blocks and 65,536 samples in total. Each block body is limited to 16,383 bytes. The caller must check every encoding result and finalize successfully before transmitting.

When `AsxFrameFlags::HasCRC` is set, `EndFrame()` appends CRC-16/CCITT-FALSE in little-endian order, covering the header and blocks. Reserve two bytes for the trailer and check every return value before sending. A finalized frame cannot be modified until `Reset()` or `BeginFrame()` starts a new one. The cloud decoder rejects missing/incorrect CRC, reserved flags, extra bytes and nonzero bit padding.

`tests/EncoderV2SafetyTests.cpp` exercises invalid inputs, type ranges, frame lifecycle, capacity limits and the shared C++/C# CRC vector. Run it with AddressSanitizer when validating on a host compiler. Native host results do not substitute for validation on the target microcontroller.

## ☁️ ASX Cloud Gateway Platform

`ASXNanoStream Encoder` is the Edge component of a complete IoT stack.

What this cloud service will provide:
- Secure multi-tenant uplink ingestion for ASX/Base64 telemetry.
- ASX payload decoding pipeline with JSON delivery to your backend.
- Webhook integration with message-signing support (HMAC).
- Downlink command flow for cloud-to-device operations.
- Analytics for payload efficiency and estimated communication cost savings.

**Official cloud platform URL:**
http://asxsoftware.com.br

---

## 🤝 Contributing
Contributions are welcome!
1. Fork the Project
2. Create your Feature Branch (`git checkout -b feature/AmazingFeature`)
3. Commit your Changes (`git commit -m 'Add some AmazingFeature'`)
4. Push to the Branch (`git push origin feature/AmazingFeature`)
5. Open a Pull Request

## 📄 License
Distributed under the MIT License. See `LICENSE` for more information.

---

**Note:** This is the **Encoder (Edge)** library. To decode these payloads back to JSON on your server/cloud, you will need an ASX Decoder (available for Python, C#, and Node.js).

## Release validation

See [release notes](CHANGELOG.md). To run the three safety suites on a host with Clang:

```bash
mkdir -p build
for test in EncoderV1SafetyTests EncoderV2SafetyTests DecoderLightSafetyTests; do
  clang++ -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-sanitize-recover=all -g -Isrc -Itests "tests/$test.cpp" -o "build/$test" || exit 1
  "build/$test" || exit 1
done
```

On Windows, put the Clang sanitizer runtime directory on `PATH`. For Arduino builds, install the `arduino:avr` core and compile `BasicDelta`, `BasicDecoder` and `BasicV2` with `arduino-cli compile --fqbn arduino:avr:uno --library . examples/<example>`.
