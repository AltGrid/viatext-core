# ViaText Core Module

The following is an extremely basic concept of ViaText protocol and core function. It is enough to present a general understanding of the system.



## ViaText Core Overview / Concept

The **Core** is the central message handler within the ViaText network. It logs received messages, queues messages for outbound transmission, and provides a unified communication layer for various types of nodes.

The Core itself does not directly handle hardware or OS interactions. Instead, it is designed to be wrapped by platform-specific implementations, including:

* **ESP32 LilyGO LoRa nodes**
* **Linux Command-line Interface (`viatext-cli`)**
* **Linux Daemon Process** (for IoT applications)
* **Station Program** (interactive, chat-like terminal interface)

Each implementation leverages the Core to achieve standardized logging, message queuing, and timing logic.

> **IMPORTANT:** The Core must be written in simple, standard C++ compatible with any appropriate system: Linux, Arduino, etc.

---

## Usage Flow

A typical ViaText user scenario is as follows:

* A user on a Linux-based system interacts with ViaText through either the `viatext-cli` or the interactive Station program.
* The user prepares a message addressed to another user or node. Users have short, human-readable IDs (e.g., radio-style IDs).
* The message is initially sent from the user's ID to a connected LoRa node ID (e.g., `"Node1"`). This first-hop transmission typically occurs via serial (USB).
* The LoRa node then broadcasts the message wirelessly. Nearby LoRa nodes receive this broadcast and determine if they are the intended recipient:

  * If the node **is** the recipient, it passes the message to its internal Core, logging and processing it.
  * If the node is **not** the intended recipient, it conditionally rebroadcasts the message outward to other nodes, following ViaText's mesh rules (such as hop limits or route metadata, or other factors).

Nodes in the mesh can also be "internal nodes," meaning they run directly on Linux systems connected via serial or other local interfaces. These internal nodes can represent human users or IoT endpoints.

All node types, regardless of hardware or operating context, utilize the same Core logic. This provides uniform functionality for logging, timestamp management, and message handling throughout the entire ViaText ecosystem.

---

## Core API Methods

The Core exposes a simple, consistent API to node implementations:

* **`add_message(message)`**
  Adds an inbound message to the internal queue. Standard LoRa messages consist of 1:  message id, 2: to id, 3: from id, 4: message. It can also consist of other directives.

* **`tick(timestamp)`**
  Advances internal timing logic and triggers scheduled processing, including retries, cleanup, and message expiration checks.

* **`get_message()`**
  Retrieves the next outbound message for sending. If no message is available, returns an appropriate indicator (e.g., `null` or empty).

* **Other functions**
  Additional functions can be added, but these three (`add_message`, `tick`, and `get_message`) must handle all activity in this specific order.

Implementations wrapping the Core must regularly call `tick(timestamp)` to maintain proper timing and internal state.

### Internal variables:

- node_id (string)
- tick_count (how many ticks have passed)
- uptime (always in milliseconds)
- last_timestamp (always in milliseconds)
- inbox (list-like) (populated by add_message)
- outbox (list-like) (populated get_message)
- received_message_ids (list of message ids, avoid duplicates)

### Full Process

## Overview

A ViaText node—whether LoRa, Linux (CLI, station, or daemon)—receives its initial input via whatever interface it is listening through. Each individual node, depending on its role, will add protocol-specific directives to the incoming message. For a LoRa node, it attaches packet info to the message in the form of command-line arguments, which are then parsed and processed by the core logic.

---

## LoRa Packet Reception Example

### Initial Incoming Message

Example received payload:

```
0x4F2B000131~shrek~donkey~Shut Up
```

### Attaching LoRa Metadata

Since this is a LoRa node, it attaches packet info as arguments:

```
-m -rssi 92 -snr 4.5 -sf 7 -bw 125 -cr 4/5 -data_length 12 -data 0x4F2B000131~shrek~donkey~Shut Up
```

This Linux-style command (recognized internally in the core) contains all relevant radio metadata and message payload info.

---

## Command-Line Arguments Reference

Below is a breakdown of the command-line arguments used by the system:

| Argument        | Description                                                                                  |
|-----------------|----------------------------------------------------------------------------------------------|
| `-m`            | **Message/Operation Flag**: Identifier that signals a new message or specific operation.     |
| `-rssi [value]` | **Received Signal Strength Indicator**: Signal power in dBm. Closer to 0 = stronger signal.  |
| `-snr [value]`  | **Signal-to-Noise Ratio**: Strength relative to background noise. Positive is better.        |
| `-sf [value]`   | **Spreading Factor**: Value 7–12; higher = longer range, lower data rate.                    |
| `-bw [value]`   | **Bandwidth**: Frequency range in kHz (commonly 125, 250, 500).                             |
| `-cr [value]`   | **Coding Rate**: Forward error correction rate. E.g., `4/5`, `4/6`, `4/7`, `4/8`.           |
| `-data_length [value]` | **Data Length**: Payload size in bytes.                                                |
| `-data [value]` | **Data Payload**: The actual message or content transmitted.                                 |

---

## Message Submission to Core

After assigning all relevant info, the message is submitted to the core using:

```
add_message("-m -rssi 92 -snr 4.5 -sf 7 -bw 125 -cr 4/5 -data_length 12 -data 0x4F2B000131~shrek~donkey~Shut Up")
```

Submission can occur once or for multiple messages at a time.

---

## Message Processing Cycle

Immediately after message submission, or after multiple messages have been submitted, the system begins processing:

### 1. Tick Function

The tick function is called with the current `millis` value:

```
tick(millis)
```

This updates all time and tick information in the system. Every time `tick(millis)` is called, **one message is processed** (first-in, first-out).

### 2. Process Function

After tick, the main process loop runs:

```
process()
```

This is the main decision thread.

---

## Argument Parsing and Message Object Creation

1. **Argument Parsing:**
   
   The message string is parsed using an argument parser:
   
   ```cpp
   args = ArgParser("-m -rssi 92 -snr 4.5 -sf 7 -bw 125 -cr 4/5 -data_length 12 -data 0x4F2B000131~shrek~donkey~Shut Up")
   ```
   
   The ArgParser class returns a list of argument `key -> value` pairs for internal use.

2. **Main Message Handling:**

   A series of easily modifiable if statements run in priority: 

   if(has_arg("-m")):
       process_message(args);

   if(has_arg("-p")):
       respond_to_ping(args)

   ...and so forth...there can be many of these. Since it is the main switch track, dozens or hundreds can exist, but for generic tasks for max usability. 

   This is where the system shines, because these can be LoRa or Linux specific operations that do useful things, in messaging, IoT, Logging, etc. 

   Continuing...
   
   If the argument `-m` exists, it is a basic message. The message data is retrieved via the `-data` argument:
   
   ```
   0x4F2B000131~shrek~donkey~Shut Up
   ```
   
   A Message object is created:
   
   ```cpp
   message = Message("0x4F2B000131~shrek~donkey~Shut Up")
   ```

---

## Message Object Structure

The Message object extracts and contains all relevant information from the raw message string.

### MessageID Struct

- `sequence`  – The sequence ID of the message set
- `part`      – Which message part in a series
- `total`     – Total number of message parts in the series
- `hops`      – Remaining hops allowed
- `flags`     – Flags (e.g., request acknowledgement, acknowledged, encrypted)

### Message Fields

- `from`  – Who sent the message
- `to`    – Who is the intended recipient
- `data`  – The message payload/content

The message object holds lots of information for decision making. 

With args and message parsed, we can verify if we are the intended recipient by comparing node_id to message id, using 

```
bool addressed_to_here(to)

```

if it is addressed to this node, we combine it with a recieved argument -r, and other arguments pertaining:

```
recieved = '-r -from <from> -data "0x4F2B000131~shrek~donkey~Shut Up'
```

It will also check the message_id struct to see:

Is it encrypted? (not yet implemented)

Does it require acknowledgment? Then set up the ack message. (not yet implemented)

If requires acknowledgment

and then:

```
stash_to_outbox(recieved)
stash_to_outbox(ack) (not yet implemented)
```

When the wrapper calls 

```
get_message()
```

It will get first in / first out messages, which it will respond to according to it's own manner. For example, a LoRa might save the message to history for viewing later. 

The main pattern is this:

Text IN, Text OUT. 

How the wrappers use the processed information will differ in each case. 

### Internal methods / functions:

* **`process()`**
  Called by tick(timestamp). After time and initial calculations are updated, `process` is called. Process administrates the entire process that happens up to logging the next get_message. 

---

## Internal Logic, Recap

### LoRa Message Structure

The format for a LoRa message is as follows:

```
<message_id>~<from>~<to>~<message>
```

This structure contains five parts.

For LoRa communication, the packet is assembled with its data and sent to the Core via `add_message(message)`.

Example:

```
-m -rssi 92 -snr 4.5 -sf 7 -bw 125 -cr 4/5 -data_length 12 -data 0x4F2B000131~shrek~donkey~Shut Up
```

This example shows the message with purposeful data.

For internal messaging, the system must allow other message types and metadata.

### Arbitrary Command System

ViaText uses a Linux-like command system so Linux users, processes, and hardware can agnostically interact with the engine. **In essence, it behaves somewhat like a standard-in/standard-out machine.**

Arbitrary data can be sent within messages. If the Core doesn't recognize a specific command or data structure, it will drop the message. Recognized commands trigger the appropriate internal actions.

## Connected Modules

### Parser Class

The **Parser class** handles all message and command parsing within the ViaText Core. Its purpose is to offer a **minimal, portable interface** for interpreting commands and message arguments across all node types.

> **Why not use CLI11, getopt, or standard argument parsers?**  
> Because **ViaText Core must compile cleanly on microcontrollers** like ESP32 and ATmega, without STL-heavy or heap-allocating dependencies. A complex parser would break cross-platform compatibility and violate our core principles: simplicity, portability, and autonomy.

---

#### Command Format Rule

All arguments must follow a strict **`-key [value]`** structure:

- A **key** is always prefixed by a single dash (`-`)
- A **value** is optional
- Keys with **no value** are treated as **flags**
- Order matters: keys must precede their value

---

#### Design Rationale

This command structure ensures:

- **Maximum compatibility** across all platforms (Linux, ESP32, Arduino, etc.)
- **Human readability** for CLI and serial-based interaction
- **Deterministic parsing**, minimizing bugs from ambiguous or complex formats
- **No external dependencies**, enabling full offline operation and microcontroller use

---

This format forms the foundation for all internal command and message control inside the ViaText ecosystem — whether used in serial interfaces, LoRa payloads, or stdin-like Linux daemons.

The parser class is also resonsible for converting a message delimited by "~" into an array of appropriate size (note, not limited to size 5, in case other values must be appended).

### Message Class

Creates a message object from a standard 5 part message string:

- message_id
- sequence number (see ViaText Message ID Format)
- part (see ViaText Message ID Format)
- max parts
- hops
- encrypted
- acknowledge
- <yet to determine>
- <yet to determine>
- from
- to
- message
- <other relevant data>
- <other relevant data>

Setters and getters for all variables. Useful for when iterating through messages. 

Uses the parser class to retrieve the ordered data. 

## ViaText Message ID Format

ViaText uses a **compact 6-byte message header**, combining a 32-bit message ID with a shared 8-bit field for hops and flags. This structure supports message fragmentation, delivery state, and routing control in a tight, platform-agnostic format optimized for constrained networks like LoRa.

---

### Structure

Message headers are now packed into a **6-byte structure** (48 bits total), embedding all essential tracking and routing metadata directly into a compact, uniform block.

| Field        | Bits | Size (Bytes) | Description                          | Max Value |
|--------------|------|--------------|--------------------------------------|-----------|
| Sequence     | 16   | 2            | Unique message identifier            | 65535     |
| Part Number  | 8    | 1            | Index of this part (0-based)         | 255       |
| Total Parts  | 8    | 1            | Total number of parts in the message | 255       |
| Hops         | 4    | — (within 1) | Hop count or TTL (max 15 hops)       | 15        |
| Flags        | 4    | — (within 1) | Acknowledge and Encryption flags     | —         |

- **Total size**: `48 bits` = `6 bytes` = just **5% of a 120-byte LoRa packet**
- Combines message fragmentation, state, and routing metadata in a single compact unit

---

**"Sequence"** refers to the overarching message ID — which may span several packets if fragmentation is used. For example, message #28 might consist of 5 parts: 1 of 5, 2 of 5, ..., 5 of 5. Each part shares the same sequence ID.

This structure allows the receiver to quickly determine:

- **Which part of which message** is being received
- **Who sent it** (via external from-ID metadata)
- **Whether it was already received (via bit flags)**
- **If it’s encrypted or a reply (via bit flags)**
- **How many hops it has taken**

---

### Bit Layout

```
[ Sequence (16 bits) ][ Part (8 bits) ][ Total (8 bits) ][ Hops (4 bits) ][ Flags (4 bits) ]
         0xFFFF             0xFF             0xFF           0bHHHH            0bFFFF
```

Packed as 6 bytes total.

Example:
```
Sequence:   0x001C   (28)
Part:       0x05     (5 of ...)
Total:      0x07     (total 7 parts)
Hops:       0x0A     (10 hops)
Flags:      0b1100   (e.g. ACK + Encrypted + two reserved bits)

→ Raw: [0x00, 0x1C, 0x05, 0x07, 0b1010 << 4 | 0b1100] = 6 bytes
```

---

### Message Flags

Flags are embedded into the **4 least significant bits** of the 6th byte.

| Bit | Flag        | Meaning                            |
|-----|-------------|------------------------------------|
| 3–2 | Reserved    | Unused for now (set to 0)          |
| 1   | Acknowledge | This message is an ACK reply       |
| 0   | Encrypted   | Payload is encrypted               |

The **upper 4 bits** of the same byte represent the **hop count** (0–15).

---

### Why It Matters

- **Compact**: All key routing and fragmentation data fit into 6 bytes
- **Fragmentation-ready**: Supports multi-part payloads over constrained transports
- **Retry-safe**: Stable sequence/part/total tuple enables deduplication
- **Routing-aware**: Hop tracking enables TTL-style rebroadcast logic
- **Minimalist**: Eliminates need for multiple parsing stages

Perfectly tuned for low-power, store-and-forward systems like ViaText — where bandwidth efficiency, simplicity, and reliability are mission-critical.



## ViaText User ID Format

### ViaText Callsign Rules (Compact, Radio-Inspired)

ViaText nodes use **compact callsigns** to identify individual devices in a mesh. These are short, human-readable strings that fit cleanly into LoRa packets without bloating payloads.

---

### Allowed Characters

- **A–Z** (uppercase only)
- **0–9**
- **Hyphen (-)**
- **Underscore (_)**

> Total charset size: 38 characters

---

### Length Constraints

- **Minimum:** 1 character
- **Maximum:** 6 characters
- Shorter callsigns are valid and encouraged for symbolic or role-based nodes (e.g. `RX1`, `Z_3`, `TX-2`)

---

### Restrictions

- Must **start and end** with an alphanumeric character (A–Z or 0–9)
- **No consecutive symbols**: `--`, `__`, `-_`, `_1-`, etc.
- **No lowercase** letters (input should be normalized to uppercase)

---

### Valid Examples

| Callsign | Notes                   |
|----------|-------------------------|
| `N0D3A1` | Ham-style call          |
| `ALP_7`  | Underscore as separator |
| `B-2`    | Short, symbolic         |
| `Z99X`   | High entropy, short     |
| `T1_MK`  | Compact and readable    |

---

### Invalid Examples

| Callsign  | Reason                           |
|-----------|----------------------------------|
| `-NODE1`  | Starts with symbol               |
| `NODE--5` | Consecutive hyphens              |
| `node99`  | Lowercase (must normalize)       |
| `R@DIO5`  | Invalid symbol `@`               |
| `LONGNAME`| More than 6 characters           |

---

### Address Space

With 38 valid characters and up to 6-character callsigns:

```
38^6 = 3,010,936,384 unique IDs
```

That’s over **3 billion unique callsigns**, each using only **6 ASCII bytes** in the packet.

Ideal for compact, resilient identity encoding in low-bandwidth environments like LoRa.
