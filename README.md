
# ViaText Core

*The brain of the machine. The switchboard in the dark.*

ViaText Core is the shared logic layer behind all ViaText nodes—whether embedded in a microcontroller, running inside a Linux daemon, or quietly routing packets between virtual agents in a multi-process system.

It is written in **pure C++**, with a strict commitment to simplicity, portability, and autonomy. Every decision is made to ensure ViaText Core remains testable, stable, and universal across platforms.

> **If this core breaks, every node breaks.**  
> It must be stable, testable, and boring—in the best possible way.

---

## Umbrella & Context

For the full ViaText system, see:  
[github.com/altgrid/viatext](https://github.com/altgrid/viatext)

---

## Overview

ViaText Core is a minimalist, portable, and highly documented C++ library and CLI for decentralized, message-driven mesh communication systems.  
It is designed to be the **core logic engine** for ViaText nodes on Linux and embedded platforms such as ESP32 (Arduino), with an explicit focus on testability, autonomy, and simplicity.

---

## Project Goals

- **Minimalist design:** Only the essentials; no unnecessary dependencies or features.
- **Portability:** Clean C++17; core code is OS-agnostic. Linux-focused CLI for prototyping.
- **Testability:** CLI-first design for rapid development and robust automated/manual testing.
- **Extensibility:** Message-driven protocol for flexibility and hackability.
- **Transparency:** Operations are explicit, traceable, and fully documented for maintainers, users, and AI review.

---

## Features

- **Message-based event system:** All logic is message-driven (including commands, status, errors), using JSON for extensibility.
- **Tick-based main loop:** Core logic advances with explicit time (tick), always controlled by the wrapper or host system.
- **Strict separation:** No direct IO, filesystem, or system calls in the core library; only the CLI wrapper interacts with Linux features.
- **Robust queue management:** Incoming and outgoing messages have size and length limits for safety.
- **Structured status and error handling:** Node health, last error, and event notifications are clearly surfaced.
- **CLI test harness:** Command-line tool (CLI11) enables shell scripting, fuzzing, and integration testing.
- **Linux-native persistence:** CLI tracks node state, time, and IDs in `~/.config/altgrid/viatext-cli/`, using the XDG standard.

---

## Integration & Build Targets

- **Microcontroller / PlatformIO:**  
  Add `#include <viatext/message.hpp>` and link via `platformio.ini`.
- **Linux / Makefile:**  
  CLI tool or daemon.  
  Static library (`libviatext-core.a`) can be generated for other tools/services.
- **Other Environments:**  
  Compatible with Tails, Raspberry Pi, or any POSIX system via GCC/cross-compilation.
- **Not hardware-specific:**  
  This core is strictly protocol and logic—transport is always handled by the wrapper.

---

## Directory Structure

```
viatext-core/
├── cli/              # CLI entry point and binaries
├── include/
│   └── viatext/      # Public C++ headers (core.hpp, message.hpp, etc.)
├── src/              # Library implementation
├── tests/            # Unit/integration tests
├── third_party/      # External dependencies (CLI11, JSON)
├── Makefile
├── README.md
└── LICENSE
```

---

## Quickstart

### Building the CLI

1. Clone the repository.
2. Install dependencies (header-only): CLI11 and nlohmann/json (provided in `third_party/`).
3. Run:
   ```
   make all
   ```

### Example Usage

```sh
./cli/viatext-cli --id leo -m "hello mesh"
./cli/viatext-cli --id leo -d -m "reboot"
./cli/viatext-cli --id leo --status
./cli/viatext-cli --id leo --set-id
```

State files are written to `~/.config/altgrid/viatext-cli/<id>-node-state.json`.

---

## Design Philosophy

- **Everything is a message:** Commands, data, events, and errors all use the same JSON protocol—no hidden function calls.
- **Time is always explicit:** The core does nothing unless advanced by `tick(now)`, with `now` supplied by the wrapper (ms since boot/epoch).
- **No unnecessary complexity:** The core never performs IO or system calls—only the CLI/test wrapper does.
- **Hackable and scriptable:** Designed for exploration, scripting, and future adaptation. Add new features as message types, not new functions.
- **AI-assist friendly:**  
  Comments and documentation are detailed for maintainers and AI assistants.  
  Parts of this project and docs are AI-accelerated, but always human-reviewed.

---

## Contributing & Extending

- Add new features as new message types (define new `"type"` fields in your JSON messages).
- Never add Linux-only headers or code to the core library; keep platform code in wrappers.
- Comment all decisions for maintainers, users, and AI review.
- Tests go in the `tests/` directory.
- Document protocol changes and design choices in the README or adjacent docs.

---

## Technical Notes

- **Message Format:**  
  All messages are JSON with a `"type"` field, e.g.:
  ```json
  {"type":"viatext","from":"AA1122","to":"BB2233","stamp":"123456","payload":"hello"}
  ```
  Directives and events use `"type":"directive"`, etc.

- **Ticking and Time:**  
  `tick(now)` must be called by the wrapper with current time (ms).  
  Allows sync across platforms (Linux, Arduino, etc).

- **Persistence:**  
  Only the CLI provides persistence. All files are written to `~/.config/altgrid/viatext-cli/`.  
  Core library never interacts with the filesystem.

---

## Frequently Asked Questions

**Q: Can I use this on Windows or Mac?**  
A: The core is portable C++, but the CLI and persistence logic are Linux-first. Windows/Mac wrappers can be built using the same message-driven interface.

**Q: How do I extend message handling?**  
A: Add new `"type"` branches in `handle_message()` and update your CLI or wrapper logic to generate/test them.

**Q: Is this production-ready?**  
A: ViaText Core is in active, documented development. It is suitable for prototyping, hacking, research, and integration into more robust systems.

---

## Acknowledgments

- [CLI11](https://github.com/CLIUtils/CLI11) for argument parsing
- [nlohmann/json](https://github.com/nlohmann/json) for JSON serialization

---

## License

This project is open source under the MIT License.  
See [LICENSE](LICENSE) for details.


![alt text](viatext.png)