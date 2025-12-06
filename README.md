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

1. Download the [`AsxNanoStream.h`](AsxNanoStream.h) file.
2. Copy it to your project folder (or `libraries` folder).
3. Include it in your code:
   ```cpp
   #include "AsxNanoStream.h"
   ```


---

## ⚡ Usage Examples

### 1. Basic Setup

```cpp
#include "AsxNanoStream.h"

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
  // Blink LED logic: ON (50ms), OFF (50ms) - Repeat 5 times
  for(int i=0; i<5; i++) {
      asx.addBinary(true, 50);
      asx.addBinary(false, 50);
  }

  // Output: "x5-1b50ms0b50ms" (Compressed loop)
  Serial.println(asx.getPayload());
}
```


### 4. Key-Value Mode (JSON-like)
If you need to send named fields (Metadata, Config).

```cpp
asx.reset();
asx.add("id", "SENSOR_01");
asx.add("bat", 95);
// Output: "id:SENSOR_01#bat:95#"
```


---

## 🔧 API Reference

| Method | Description | Example |
|:-------|:------------|:--------|
| `reset()` | Clears the buffer for a new message. | |
| `setBaseline(int val)` | Sets the starting value for delta encoding. | `B100#` |
| `addAnalog(int val, ms)` | Adds a value. Calculates delta (`+N/-N`) and RLE. | `+5v` |
| `addBinary(bool state, ms)` | Adds a binary state (`1b`/`0b`). Checks RLE. | `1b` |
| `add(key, value)` | Adds a key-value pair (String, Int, or Float). | `temp:25` |
| `getPayload()` | Returns the final compressed string. | |

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
