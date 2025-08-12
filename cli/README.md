# ViaText CLI — one-shot wrapper around `viatext::Core`

**Purpose:** a tiny, scriptable CLI that turns flags and a text body into a ViaText message and hands it to the Core. It can also dump what went in/out and (optionally) write the outgoing payload to a serial device. Mainly for testing. But it is designed to send (not receive) messages. 

> Core stays transport-agnostic. The CLI is a Linux convenience layer: parse flags, persist a Node ID, build a `Package`, call `core.add_message() → core.tick() → core.get_message()` — and optionally ship the result over serial.

---

## TL;DR

```bash
./build/viatext-cli   --from USER   --to rnslr   --serial /dev/pts/3   --baud 115200   -m   --data "Hello from ViaText test"   --print   --new
```

**Notes**

- `--new` tells Core to allocate a fresh `MessageID` and build the stamp for you. (For now, creation starts this way.)
- `-m` marks it as a **message**. Other verbs exist (`-p` ping, `-ack`, `--set-id`).
- `--print` lets you see exactly what was passed to Core and what Core produced.
- If `--serial` is given, the **outbound** payload is also written to that device (newline appended).

---

## Quick mental model

- **Stamp format** (payload): `<hex10>~FROM~TO~DATA`  
  Example: `00AB91F220~KFURY~HCKRMN~Hello`  
  Core generates this when you use `--new`. You can also supply it yourself via `--message`.
- **Args**: key/values and flags (e.g., `-m`, `--to`, `--set-id`). The CLI keeps your keys **exact**; Core decides behavior during dispatch.
- **Core ticks once per run.** It processes at most one inbound item and may emit 0..N outbound packages. The CLI drains them all.

---

## Node identity & state

The CLI persists your node **callsign** (uppercase A–Z, 0–9, optional `-`/`_`, 1..6 chars, must start/end alnum, no consecutive symbols) under:

```
~/.config/altgrid/viatext-cli/<ID>-node-state.json
```

On each run:

- If you pass `--create-id <ID>`, it validates, uppercases, and persists immediately.
- If no ID is provided and exactly one state file exists, it reuses that ID.
- Otherwise, it generates a random 6‑char callsign and persists it.

**Override the directory** with `--state-dir <path>`.

---

## CLI flags (what the wrapper owns)

| Option | Meaning |
|---|---|
| `--print` | Show **IN** (args/payload going into Core) and **OUT** packages produced by Core. |
| `--format {pretty\|json\|raw}` | Change `--print` formatting. Default: `pretty`. |
| `--no-color` | Disable ANSI styling in pretty mode. |
| `--state-dir <path>` | Use a custom config directory. |
| `--create-id <ID>` | Create & persist a node ID (validated & uppercased). |
| `--tick-ms <u32>` | Override the tick time passed to `core.tick()`. If omitted, the CLI uses a steady-clock based value. |
| `--serial <dev>` | Write each **outbound** payload to the serial device (e.g., `/dev/ttyUSB0`, `/dev/pts/3`). |
| `--baud <rate>` | Serial baud (default 115200). |
| `--message "<stamp>"` | Send an **already-formed** stamp (bypasses `--new`). |

**Message construction helpers** (convenience when not using `--message`):

- `--id <hex10>` — raw header hex (rarely used; Core builds this for you when `--new`).
- `--from <CALLSIGN>` — sender; with `--new`, Core will prefer your node ID.
- `--to <CALLSIGN>` — recipient (required for `--new` message creation).
- `--data "<text>"` — body text.

> Anything not in this list is treated as **Core-centered** and passed through unchanged (e.g., `-m`, `--m` → normalized to `-m`, `-p`, `--set-id`, `--new`, `--ack`, etc.).

---

## Core verbs (what gets passed through)

- `-m` — standard message delivery
- `-p` — ping (Core replies with `-pong`)
- `-ack` — acknowledgment (surfaced as an event)
- `--set-id` — set the node’s ID from the message **body**
- `--new` — **creation trigger**: allocate a fresh header, build a stamp using `--to/--data` (and `--from` = your node ID)

Order is **first-match-wins** in Core dispatch.

---

## Common workflows

### 1) First run: create a node ID

```bash
./build/viatext-cli --create-id LEO --print
```

- Persists `LEO` under the state dir and shows a small header in pretty mode.
- On later runs, the CLI injects `-node-id LEO` into the args sent to Core.

### 2) Send a new message (Core builds the stamp)

```bash
./build/viatext-cli -m --new --to HCKRMN --data "Hello from ViaText test" --print
```

- Core allocates a fresh sequence, sets `FROM=<your ID>`, `TO=HCKRMN`, encodes the payload.
- The CLI prints the OUT package(s); add `--serial /dev/pts/3` to actually write the payload to a port.

### 3) Send a pre-built stamp verbatim

```bash
./build/viatext-cli --message "00AB91F220~LEO~HCKRMN~raw payload" --print
```

- No creation logic; your stamp is passed straight through to Core.

### 4) Ping another node

```bash
./build/viatext-cli -p --to HCKRMN --print --new
```

- Core emits a `-pong` package (sequence preserved).

### 5) Change the node’s ID at runtime

```bash
./build/viatext-cli --set-id "BOXCAR" --print
```

- Core sets the ID to `BOXCAR` and emits an `-id_set` event. (Persistence is the CLI’s job; it writes state each run.)

### 6) Write outgoing payload to serial

```bash
./build/viatext-cli -m --new --to HCKRMN --data "TTY test"   --serial /dev/pts/3 --baud 115200 --print
```

- The CLI appends a newline and writes the **outbound** payload to the device. There’s a short non-blocking retry loop for `EAGAIN` cases.

---

## Output formats

- `--format pretty` (default): human-friendly; shows parsed stamp fields.
- `--format json`: machine-friendly array/objects for IN/OUT.
- `--format raw`: writes payload bodies only (one per line for OUT).

Example (pretty, IN and OUT abbreviated):

```
IN  → core(add_message)
  args(in):
      -m               [flag]
      --to             HCKRMN
      --data           Hello from ViaText test
      --new            [flag]
  payload:
    [ID]    (empty)
    [FROM]  (empty)
    [TO]    (empty)
    [DATA]  (empty)

TCK → core.tick(12345678)

OUT → 1 package(s)
  #1
    args(out):
      -m               [flag]
      --to             HCKRMN
      --from           LEO
    payload:
      [ID]   00AB91F220
      [FROM] LEO
      [TO]   HCKRMN
      [DATA] Hello from ViaText test
```

---

## Behavior details worth remembering

- The CLI **always injects** `-node-id <ID>` into the Core args (last-wins).
- `--m` on the CLI is normalized to `-m` before passing to Core.
- For `--new`:
  - `--to` is **required** (non-empty).  
  - `--from` is ignored in favor of the current node ID.
  - `--data` is optional (empty allowed).
- Without `--new`, if you provide `--id/--from/--to/--data` the CLI can assemble a stamp for you — but the canonical path is to let Core build it.
- Outbox is drained completely each run; each outbound `Package` is printed (if `--print`) and optionally sent to serial.
- The serial writer appends a newline; it retries briefly if the port is busy.

---

## Exit codes (CLI layer)

- `0` — success
- `1` — serial open failed
- `2` — serial write failed
- `3` — serial timed out before all bytes were accepted
- nonzero from `CLI11` — parse error / usage

---

## Troubleshooting

- **“error: invalid id for --create-id”** — Callsigns must be 1..6 chars, A–Z/0–9, optional `-`/`_`, start/end alnum, no `--`/`__` runs, uppercase only.
- **No output packages in `--print`** — Some verbs are local-only, or your message wasn’t addressed to this node. For `-m`, **OUT** usually includes a `-r` “delivered” event only when `--to` equals your node ID.
- **`/dev/pts/N` doesn’t receive** — Confirm the correct PTY endpoint and that a reader is attached. Add `strace -e write` around the CLI if needed.
- **State confusion** — Inspect or delete `~/.config/altgrid/viatext-cli/*-node-state.json`. Recreate with `--create-id`.
- **Sanitizers** — build with `make SAN=1` to catch memory issues during development.

---

## Reference: What the CLI does internally

From `main.cpp` responsibilities (abridged):

- Parse CLI options and **separate** CLI‑centered vs Core‑centered arguments.
- Ensure a node identity exists; persist under XDG config dir.
- Build a `viatext::Package` with **exact, pass‑through** Core args (no canonicalization other than `--m → -m`).  
- Inject `-node-id <ID>` into Core args.  
- Run `core.add_message(pkg) → core.tick(ms) →` drain with `core.get_message(out)`.
- If `--print`, show readable dumps and a stamp visualizer.
- If `--serial`, write each OUT payload to the device (newline‑terminated).

**State file schema (MVP)**

```json
{"id":"<CALLSIGN>","last_time":<ms>}
```

---

## Recipes (copy/paste)

- **New message, print to console only**  
  `./build/viatext-cli -m --new --to HCKRMN --data "hi" --print`

- **Ping another node**  
  `./build/viatext-cli -p --new --to HCKRMN --print`

- **Change ID then confirm**  
  `./build/viatext-cli --set-id --data "NODEX" --print`

- **Raw stamp passthrough**  
  `./build/viatext-cli --message "00AA112233~A~B~X" --print --format raw`

- **Write OUT to serial PTY**  
  `./build/viatext-cli -m --new --to B --data "X" --serial /dev/pts/3 --baud 115200`

---

## Philosophy

Three standards guide design here:

1. **Simplicity** — small surface area, predictable behavior.
2. **Portability** — same Core under ESP32 or Linux.
3. **Autonomy** — store‑and‑forward first; infrastructure optional.

> If you forget everything else: _Use `--new` for creation, `-m` to message, `--to` for destination, `--data` for body, and `--print` when you want to see what really happened._