# ViaText CLI — Command-Line Wrapper for the ViaText Core

This is the Linux CLI layer for the ViaText mesh messaging system. It wraps the core engine and lets users send, receive, and trace mesh messages via simple command-line invocations.

---

##  Quick Start

### 1. Inject or generate your Node ID (persistent)

```bash
# Set and persist a new node ID
viatext-cli -set-id "NODE1"

# Or, let the CLI generate one automatically
viatext-cli -print
```

Your `node-state.json` will be stored in `$XDG_CONFIG_HOME/altgrid/viatext-cli/`.

### 2. Send a message

```bash
viatext-cli -m "Hello, other node!" -node-id "NODE2"
```

`-m` forwards the payload into the core. After running, outbound packages are silently enqueued.

### 3. Ping another node

```bash
viatext-cli -p -node-id "NODE3" --print
```

`--print` enables human-readable debug output showing inbound args and the resulting outbound args/payload.

---

##  Example Commands

```bash
# Send to another node
viatext-cli -m "System Check OK" -node-id "relay1"

# Send and print internal flow
viatext-cli -m "Status: operational" -node-id "central" --print

# Monitor incoming messages (simulate as needed)
viatext-cli -m "Testing 123" -node-id "edge" --print

# Ping + trace output
viatext-cli -p -node-id "robot1" --print --no-color

# Custom payload stamp format
viatext-cli --message "4F2B000131~NODEA~NODEB~Hi" --print

# Use full components instead of raw stamp
viatext-cli -id "4F2B000131" --from "NODEA" --to "NODEB" --data "Hello" -node-id "NODEC" --print

# Run under Address/UBSAN for debugging
make SAN=1 cli && ./build/viatext-cli -m "Test SAN" -node-id "debugnode" --print
```

---

##  Notes

- **CLI‑only flags** (`--print`, `--no-color`, `--format`, `--state-dir`, `--create-id`, etc.) are **not forwarded** to the core.
- **Core‑centric flags** (`-m`, `-p`, `-ack`, `-node-id`, `--set-id`, `--to`, `--from`, etc.) are forwarded exactly—no key normalization or loss.
- The CLI wrapper uses [CLI11](https://cliutils.github.io/CLI11/) for command-line parsing and `nlohmann/json` for state persistence.
- Pretty-printing conventions adapt to TTY output by default (`--no-color` disables ANSI escapes). Use `--format json` or `--format raw` for scripting integration.

---

##  Why This Matters

Following good CLI design (inspired by [clig.dev](https://clig.dev/)) ensures:

- **Human-first usability**, with clear flags and minimal surprises.
- **Script-friendly behavior**, especially when `--format json` or `--format raw` is used.
- **Discoverable state and identity**, so you always know which node you’re sending from.

---

Feel free to contribute example recipes or edge-case scenarios.
