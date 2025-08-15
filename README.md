# ViaText CLI Core (LoRa/SLIP)

## Overview
This is the minimal Linux-facing CLI core for ViaText.  
It sends and receives framed messages over serial using **SLIP** encoding,  
targeting devices such as **TTGO LoRa32 v2** running Arduino firmware.

The goal:
- **Valid command in → Valid output out**
- Output is *both* human-readable and script-friendly
- Linux-style, pipeline-safe
- Simple enough to embed in shell scripts or other processes

---

## Features
- **Basic Commands**
  - `--set-id <ID>` → set node identity
  - `--get-id` → query current node identity
  - `--ping` → request alive/latency test
- **Framing**
  - Binary frame format with verb, flags, sequence, TLV payload length
  - SLIP-encoded for serial transport
- **Decode**
  - CLI prints compact `key=value` strings for easy parsing

---

## Build
Linux (Makefile):
```bash
make
```

PlatformIO (Arduino firmware target):
```bash
platformio run --environment ttgo-lora32-v2
```

---

## Run (Linux CLI)
```bash
./viatext-cli --dev /dev/ttyACM0 --set-id vt-01
./viatext-cli --dev /dev/ttyACM0 --get-id
./viatext-cli --dev /dev/ttyACM0 --ping
```

Output format example:
```
status=ok seq=7 id=vt-01
```

---

## Frame Structure
```
+-------+-------+-------+-------+-------------------+
| verb  |flags  | seq   | len   | TLV payload...    |
+-------+-------+-------+-------+-------------------+
   1B     1B      1B      1B     variable
```

**TLV format inside payload:**
```
+-------+-------+--------------------+
| tag   | len   | value (len bytes)  |
+-------+-------+--------------------+
```

- SLIP wraps the entire frame for serial transport
- All integers are unsigned, little-endian

---

## File Layout
```
include/       # headers (commands.hpp, serial_io.hpp, slip.hpp)
src/           # implementation (.cpp)
third_party/   # CLI11.hpp (argument parsing)
Makefile       # Linux build
```

---

## Notes
- Designed for **Linux-first** workflows, but transport-agnostic at core
- Easy to integrate into larger ViaText mesh logic
- Minimal dependencies (CLI11 + SLIP)
