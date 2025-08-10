# ViaText Core Module

## Overview

ViaText Core is the message engine used by all ViaText nodes.  
It provides a uniform, transport-agnostic way to handle inbound and outbound messages — whether those bytes arrive from LoRa radios, a serial port, or a Linux CLI tool.

The Core:

- Stores received messages in an **inbox** for processing.
- Manages an **outbox** for outbound delivery.
- Tracks timing, retries, and expiration.
- Keeps duplicate protection via recent message ID history.

It does **not** talk directly to radios, sockets, or filesystems.  
Instead, it is wrapped by platform-specific programs, such as:

- ESP32 LoRa Nodes (e.g., LilyGO LoRa32)
- Linux CLI tool (`viatext-cli`)
- Linux Station Program (interactive shell)
- Daemon services for IoT or always-on systems

All of these wrappers feed the same core logic, ensuring identical behavior no matter where it runs.

---

## How It Works

### Ingress-Agnostic Design

Wrappers take care of receiving raw input and turning it into a **Package**:

- `payload`: up to 255 bytes of in-system text.
- `args`: metadata as fixed-capacity key/value pairs (e.g., `-rssi 92`, `-sf 7`, `-m`).

Keys are preserved exactly as given. Values may be empty for flags.  
No heap allocations — everything is stored in fixed-size ETL containers.

The Core works only with `Package` objects. It does not parse raw strings.

---

### Typical Flow

1. **Wrapper receives data**  
   From LoRa, serial, stdin, etc.

2. **Wrapper parses metadata**  
   Populates a `Package` with `payload` + `args`.

3. **Wrapper calls**  
   ```cpp
   core.add_message(pkg);
   ```

4. **Core processes on tick**  
   ```cpp
   core.tick(millis);
   ```

5. **Wrapper fetches next outbound**  
   ```cpp
   if (core.get_message(pkg)) { send(pkg); }
   ```

---

### Core API

- **`add_message(Package)`**  
  Enqueues a new message for processing.

- **`tick(timestamp_ms)`**  
  Advances internal timers and processes one queued message per call.

- **`get_message(Package&)`**  
  Retrieves the next outbound message, FIFO order.

Wrappers are expected to call `tick()` regularly to keep the system moving.

---

### Internal Tracking

The Core keeps:

- node_id – this node’s callsign/identifier.
- tick_count – how many ticks have passed.
- uptime – milliseconds since start.
- last_timestamp – last tick timestamp.
- inbox – FIFO queue for inbound work.
- outbox – FIFO queue for outbound work.
- recent_ids – short history of MessageIDs to avoid duplicates.

---

## Example: LoRa Ingress

**Raw payload from radio:**
```
0x4F2B000131~SHREK~DONKEY~Shut Up
```

**Metadata from wrapper, generally:**
```
-m -rssi 92 -snr 4.5 -sf 7 -bw 125 -cr 4/5 -data_length 12
```
(Will often be found in variables such as packet, not a string like this. 
But each node type will handle arguments it's own way.)

**Wrapper produces:**
```cpp
viatext::Package p;
p.payload = "0x4F2B000131~SHREK~DONKEY~Shut Up";
p.args.set_flag("-m");
p.args.set("-rssi", "92");
p.args.set("-snr", "4.5");
p.args.set("-sf",  "7");
p.args.set("-bw",  "125");
p.args.set("-cr",  "4/5");
p.args.set("-data_length", "12");

core.add_message(p);
```

---

## Message and MessageID

The `Message` object can be built from a `Package.payload` when the core needs to interpret the ViaText frame.

### MessageID (5 bytes)
```
[ Sequence (16) ][ Part (8) ][ Total (8) ][ Hops (4) | Flags (4) ]
```
- Compact: always 5 bytes.
- Supports fragmentation, hop limits, ACK/encryption flags.
- Duplicate detection via sequence/part.

---

## ViaText Callsigns

- Charset: A–Z, 0–9, `-`, `_`
- Length: 1–6 characters
- Must start/end with alphanumeric.
- No consecutive symbols.
- Example: `N0D3A1`, `B-2`, `RX_1`

---

## Core Philosophy

### Simplicity
- Everything is human-readable where possible.
- All state can be inspected with standard Linux tools.
- One person should be able to understand and modify the code without vendor docs.

### Portability
- Runs on ESP32, Raspberry Pi, Linux desktops/servers.
- No GUI required — terminal and serial friendly.
- Minimal dependencies; fixed-size storage for MCU safety.

### Autonomy
- No phone or internet required.
- Operates in mesh or stand-alone.
- Messages carry their own minimal headers; no schema servers.

---

## Feature Tracker

- [x] Fixed-capacity argument storage (`ArgList`)
- [x] Text payload (`Text255`)
- [x] Core FIFO inbox/outbox
- [x] MessageID parsing and packing
- [ ] ACK handling
- [ ] Encryption layer
- [ ] Store-and-forward Post Office mode
- [ ] RPS/LPS time/orientation sync
