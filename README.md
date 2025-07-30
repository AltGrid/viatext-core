# viatext-core

*The brain of the machine. The switchboard in the dark.*

![VIATEXT Logo](../viatext.png)

**`viatext-core`** is the shared logic layer behind all ViaText nodes — whether embedded in a microcontroller, running inside a Linux daemon, or quietly routing packets between virtual agents in a multi-process system.

It’s written in **pure C++**, with no runtime bloat, no external dependencies, and strict alignment with the ViaText philosophy of **simplicity, portability, and autonomy**.

This is the component that speaks the protocol, builds and parses messages, handles stamps, and maintains consistent internal logic — no matter what physical layer is involved.

---

## 🔗 Project Umbrella

For full system context, see the main ViaText repo:

→ [github.com/altgrid/viatext](https://github.com/altgrid/viatext)

---

## 🎯 Purpose

`viatext-core` exists to:

- **Unify** logic across platforms: Arduino, ESP32, Raspberry Pi, Linux
- **Define** the ViaText message format (headers, stamps, routing info)
- **Provide** reusable C++ classes for parsing, encoding, and validation
- **Enable** portable nodes that share the same behavior across transports
- **Support** future extensions: filesystem bridge, LoRa gateways, mesh routing layers, etc.

It is *not* hardware-specific — that’s handled by firmware (e.g. [`viatext-ttgo-lora32-v21`](https://github.com/altgrid/viatext-ttgo-lora32-v21)) or Linux-side tooling (e.g. [`viatext-station`](https://github.com/altgrid/viatext-station)).

---

## 📦 Build Targets

The core is meant to be compiled and included in multiple environments:

- ✅ **Arduino / PlatformIO**: Runs on ESP32 LoRa boards
- ✅ **Makefile-based Linux builds**: Native CLI tools and services
- ✅ **Other environments** (e.g., Tails, Pi) via standard GCC or cross-compilation

---

## 🧱 File Structure (Expected)

```
viatext-core/
├── include/
│   └── viatext/
│       ├── message.hpp      # Stamp and message structure
│       ├── packet.hpp       # Encoding and decoding functions
│       └── utils.hpp        # Lightweight helpers
├── src/
│   ├── message.cpp
│   ├── packet.cpp
│   └── ...
├── examples/
│   └── test-node.cpp        # CLI tool or test harness
├── platformio.ini           # For microcontroller builds
├── Makefile                 # For Linux builds
└── README.md
```

---

## 🚧 Status

Work in progress — current focus is on:

- Message structure definition
- Serialization/deserialization methods
- Stamp format validation
- Lightweight memory handling
- Clock sync helpers and routing metadata (planned)

This core will remain **strictly minimal**, readable, and adaptable — no STL dependency sprawl, no unnecessary abstractions.

---

## 🔧 Integration

- Microcontroller builds: included via `#include <viatext/message.hpp>` and linked in `platformio.ini`
- Linux builds: compiled into CLI tools, daemons, or services
- Eventually: a static library (`libviatext.a`) may be generated for easy inclusion

---

## 🔒 Guiding Rule

> **If this core breaks, every node breaks.**  
> It must be stable, testable, and boring — in the best possible way.

---

## 🤖 Note on AI Assistance

Parts of this README, related documentation, and portions of code were drafted with help from **ChatGPT** and **GitHub Copilot**. These tools assist with scaffolding, formatting, and rapid ideation — but every decision, edit, and line of logic is reviewed and curated by a human.

AI is used here as a **development accelerator**, not an autopilot.

This project reflects the work of an engineer who uses every tool available — including AI — to build clear, functional, and human-centered systems.

ViaText is built with intent. AI is just one of many tools in the belt.
