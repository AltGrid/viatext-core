# ViaText Command Flow (Host Side)

This document describes the **round-trip flow** of a ViaText command on the host side: from when the user types a CLI command, through packet construction and serial transport, and back to a decoded response. The behavior of the microcontroller/LoRa device is not covered here.

```
+-----------------+        +---------------------+        +----------------------+        +----------------------+
| User / Shell    | -----> | CLI (main.cpp)      | -----> | Dispatcher           | -----> | Commands (builders)  |
| viatext-cli ... |        | CLI11 parse args    |        | name_to_kind()       |        | make_* functions     |
+-----------------+        +---------------------+        +----------------------+        +----------------------+
                                                                                                     |
                                                                                                     v
                                                                                         [verb + TLVs request bytes]
                                                                                                     |
                                                                                                     v
                                                                                         +--------------------------+
                                                                                         | Serial I/O + SLIP        |
                                                                                         | open_serial()/write_frame|
                                                                                         +--------------------------+
                                                                                                     |
                                                                                         (serial link to device)
                                                                                                     |
                                                                                         +--------------------------+
                                                                                         | Serial I/O + SLIP        |
                                                                                         | read_frame()             |
                                                                                         +--------------------------+
                                                                                                     |
                                                                                                     v
                                                                                         +--------------------------+
                                                                                         | Decoding / Output        |
                                                                                         | decode_pretty() -> stdout|
                                                                                         +--------------------------+
```

---

## 1) User Input (CLI11 in `main.cpp`)

User invokes exactly one command (or `--scan`). Examples:
```bash
viatext-cli --get freq --node N3
viatext-cli --set sf 7 --dev /dev/serial/by-id/usb-ACME_Node123
viatext-cli --ping --node N3 --timeout 2000
```
`main.cpp` parses with CLI11 into:
- Legacy flags: `--get-id`, `--ping`, `--set-id <new_id>`
- Modern flags: `--get <name>`, `--set <name> <value>`
- Targeting: `--node <id>`, `--dev <path>`
- I/O tuning: `--timeout <ms>`, `--baud <n>`, `--boot-delay <ms>`

If no single command is selected, the program prints:
```
status=error reason=need_exactly_one_command
```

---

## 2) Target Resolution (`main.cpp` + `node_registry`)

Goal: determine the device path to open.

- If `--node <id>` is provided:
  1) Build expected runtime alias path: `$XDG_RUNTIME_DIR/viatext/viatext-node-<id>` (fallback `/run/user/<uid>/viatext/...`).
  2) If the alias exists, use it.
  3) Otherwise perform a live scan:
     - `discover_nodes()` probes candidates and identifies online nodes via `make_get_id()`.
     - `save_registry()` writes `$HOME/.config/altgrid/viatext/nodes.json`.
     - `create_symlinks()` can create runtime aliases when invoked under `--scan --aliases`.
- If `--dev <path>` is provided, use it directly (overrides `--node`).
- If neither is provided, a quick scan runs and either auto-selects the single online device or exits with:
  - `status=error reason=multiple_nodes_connected` or
  - `status=error reason=no_nodes_online`

---

## 3) Build the Request Packet (`command_dispatch` + `commands`)

Main calls exactly one of:
- `build_param_get_packet(name, seq, req, err)`
- `build_param_set_packet(name, value, seq, req, err)`
- `build_legacy_packet(get_id, ping, set_id, seq, req, err)`

Inside the dispatcher:
1) `name_to_kind(name, is_set)` maps user-facing names (e.g., `"freq"`) to a `CommandKind` (e.g., `SET_FREQ_HZ`).
2) `build_packet_from_kind(kind, seq, value, req, err)`
   - Validates and parses values (ranges and types).
   - Invokes the matching builder in `commands.hpp/cpp` (`make_set_freq()`, `make_get_sf()`, etc.).
3) Output is a raw **verb + TLV** request in `req` (unframed bytes).

On bad input, `err` is set to a stable string like `bad_value:sf(7..12)` and the CLI prints:
```
status=error reason=<err>
```

---

## 4) Frame and Send (`serial_io` + `slip`)

- `open_serial(dev, baud, boot_delay_ms)` opens the TTY, sets raw mode, and waits briefly to absorb USB CDC resets and boot chatter.
- `write_frame(fd, req)` SLIP-encodes the request (`slip::encode()`) and attempts a single write of the full frame.

If the write cannot send the full frame, the CLI prints:
```
status=error reason=write_failed
```

---

## 5) Receive Response (`serial_io` + `slip`)

- `read_frame(fd, resp, timeout_ms)` polls for input and feeds bytes into a `slip::decoder` until a complete frame is recovered.
- On success, `resp` holds the raw response bytes (verb + TLVs).
- On timeout or poll error, the CLI prints:
```
status=error reason=timeout
```

---

## 6) Decode and Print (`commands.cpp`)

- `decode_pretty(resp)` converts the verb and TLVs to a compact line such as:
```
status=ok seq=1 id=N3 freq_hz=915000000 sf=7 ...
```
- The line is written to stdout. This format is designed for both humans and shell tools (grep, awk).

---

## 7) Close

- `close_serial(fd)` closes the port.
- Exit status:
  - `0` on success
  - `1`..`6` for open/write/timeout/targeting/command selection errors as emitted in `main.cpp`.

---

## Function Map (Quick Reference)

| Stage                        | Functions                                                                 |
|-----------------------------|----------------------------------------------------------------------------|
| Parse CLI                   | CLI11 in `main.cpp`                                                        |
| Discover / Alias            | `discover_nodes()`, `save_registry()`, `create_symlinks()`                 |
| Dispatch selection          | `name_to_kind()`, `build_packet_from_kind()`                               |
| Build request bytes         | `make_get_*()`, `make_set_*()` (in `commands.hpp/cpp`)                     |
| Serial open / frame / send  | `open_serial()`, `write_frame()`, `slip::encode()`                         |
| Receive / deframe           | `read_frame()`, `slip::decoder::feed()`                                    |
| Decode / output             | `decode_pretty()`                                                          |

---

## Example Round Trip

Command:
```bash
viatext-cli --set sf 7 --node N3
```

Flow:
1) CLI parses options in `main.cpp`.
2) Target resolved via alias or `discover_nodes()`.
3) Dispatcher: `name_to_kind("sf", is_set=true)` → `SET_SF` → `build_packet_from_kind()` → `make_set_sf()`.
4) `open_serial()`; `write_frame()` sends SLIP-framed request.
5) `read_frame()` receives a SLIP-framed response.
6) `decode_pretty()` prints:
```
status=ok seq=1 sf=7
```
7) `close_serial()`; process exits 0.
