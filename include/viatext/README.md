# ViaText Core Module

## ViaText Core Overview / Concept

The **Core** is the central message handler within the ViaText network. It logs received messages, queues messages for outbound transmission, and provides a unified communication layer for various types of nodes.

The Core itself does not directly handle hardware or OS interactions. Instead, it is designed to be wrapped by platform-specific implementations, including:

* **ESP32 LilyGO LoRa nodes**
* **Linux Command-line Interface (`viatext-cli`)**
* **Linux Daemon Process** (for IoT applications)
* **Station Program** (interactive, chat-like terminal interface)

Each implementation leverages the Core to achieve standardized logging, message queuing, and timing logic.

> **IMPORTANT:** The Core must be written in simple, standard C++ compatible with any system: Linux, Arduino, etc.

---

## Usage Flow

A typical ViaText user scenario is as follows:

* A user on a Linux-based system interacts with ViaText through either the `viatext-cli` or the interactive Station program.
* The user prepares a message addressed to another user or node. Users have short, human-readable IDs (e.g., radio-style IDs).
* The message is initially sent from the user's ID to a connected LoRa node ID (e.g., `"Node1"`). This first-hop transmission typically occurs via serial (USB).
* The LoRa node then broadcasts the message wirelessly. Nearby LoRa nodes receive this broadcast and determine if they are the intended recipient:

  * If the node **is** the recipient, it passes the message to its internal Core, logging and processing it.
  * If the node is **not** the intended recipient, it rebroadcasts the message outward to other nodes, following ViaText's mesh rules (such as hop limits or route metadata).

Nodes in the mesh can also be "internal nodes," meaning they run directly on Linux systems connected via serial or other local interfaces. These internal nodes can represent human users or IoT endpoints.

All node types, regardless of hardware or operating context, utilize the same Core logic. This provides uniform functionality for logging, timestamp management, and message handling throughout the entire ViaText ecosystem.

---

## Core API Methods

The Core exposes a simple, consistent API to node implementations:

* **`add_message(message)`**
  Adds an inbound message to the internal queue. Messages include sender, recipient, routing metadata, and the message payload itself. It can also consist of other directives.

* **`tick(timestamp)`**
  Advances internal timing logic and triggers scheduled processing, including retries, cleanup, and message expiration checks.

* **`get_message()`**
  Retrieves the next outbound message for sending. If no message is available, returns an appropriate indicator (e.g., `null` or empty).

* **Other functions**
  Additional functions can be added, but these three (`add_message`, `tick`, and `get_message`) must handle all activity in this specific order.

Implementations wrapping the Core must regularly call `tick(timestamp)` to maintain proper timing and internal state.

---

## Internal Logic

### LoRa Message Structure

The format for a LoRa message is as follows:

```
<message_id>|<hops>|<from>|<to>|<message>
```

This structure contains five parts.

For LoRa communication, the packet is assembled with its data and sent to the Core via `add_message(message)`.

Example:

```
-m -rssi 92 -snr 4.5 -sf 7 -bw 125 -cr 4/5 -data_length 12 -data "23|3|shrek|donkey|Hello World"
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


### Message Class

Creates a message from a standard 5 part message:

- message_id
- hops
- from
- to
- message
- <other relevant data>
- <other relevant data>

Uses the parser class to retrieve the ordered data. 


