# ViaText CLI — Valid Commands

This document describes the **supported CLI commands** for ViaText.  
Exactly **ONE** command must be provided per run (except `--scan`).  
If none or many are given, the CLI exits with:

```
status=error reason=need_exactly_one_command
```

---

## Legacy Commands (no TLVs unless noted)
```bash
viatext-cli --get-id [--node <id> | --dev <path>] [--timeout <ms>] [--baud <n>] [--boot-delay <ms>]
viatext-cli --ping   [--node <id> | --dev <path>] [--timeout <ms>] [--baud <n>] [--boot-delay <ms>]
viatext-cli --set-id <new_id> [--node <id> | --dev <path>] [--timeout <ms>] [--baud <n>] [--boot-delay <ms>]
```

---

## Generic GET (single parameter by name)
```bash
viatext-cli --get <name> [--node <id> | --dev <path>] [--timeout <ms>] [--baud <n>] [--boot-delay <ms>]
```

**Valid parameter names:**

- Identity / System  
  `id | alias | fw | uptime | boot_time`

- Radio  
  `freq | sf | bw | cr | tx_pwr | chan`

- Behavior  
  `mode | hops | beacon | buf_size | ack`

- Diagnostics  
  `rssi | snr | vbat | temp | free_mem | free_flash | log_count`

- Bulk  
  `all` (may stream multiple frames; CLI reads one and exits)

---

## Generic SET (single parameter by name)
```bash
viatext-cli --set <name> <value> [--node <id> | --dev <path>] [--timeout <ms>] [--baud <n>] [--boot-delay <ms>]
```

**Valid parameters and value types:**

| Name      | Type / Units                   | Notes |
|-----------|--------------------------------|-------|
| `alias`   | `<string>` (UTF-8, ≤255 bytes) | |
| `freq`    | `<u32 Hz>` (e.g., 915000000)   | |
| `sf`      | `<u8 7..12>`                   | spreading factor |
| `bw`      | `<u32 Hz>` (e.g., 125000)      | bandwidth |
| `cr`      | `<u8 5..8>`                    | coding rate (represents 4/5..4/8) |
| `tx_pwr`  | `<i8 dBm>`                     | radio-safe range per firmware |
| `chan`    | `<u8>`                         | abstract channel index |
| `mode`    | `<u8>` (0=relay,1=direct,2=gateway) | firmware-defined |
| `hops`    | `<u8>`                         | max hops |
| `beacon`  | `<u32 seconds>`                | beacon interval |
| `buf_size`| `<u16>`                        | outbound queue size |
| `ack`     | `<u8 0|1>`                     | ack mode |

> ⚠️ Setting persistent **ID** is done via the legacy flag:  
> `--set-id <new_id>`

---

## Discovery / Symlinks
```bash
viatext-cli --scan [--aliases]
```

- Prints:  
  ```
  id=<id> dev=<path> online=<0|1>
  ```

- Saves registry to:  
  `$HOME/.config/altgrid/viatext/nodes.json`

- With `--aliases`: also creates runtime symlinks:  
  ```
  $XDG_RUNTIME_DIR/viatext/viatext-node-<id>
  # fallback: /run/user/<uid>/viatext/viatext-node-<id>
  ```

---

## Targeting / Device Selection
- `--node <id>`  
  Resolve device by node ID. Resolution order:  
  1. existing runtime alias symlink  
  2. live scan of devices to find an online match  

  On failure:  
  ```
  status=error reason=node_not_found id=<id>
  ```

- `--dev <path>`  
  Use explicit device path (e.g., `/dev/serial/by-id/...`). Overrides `--node`.

---

## I/O Tuning
These apply to any command that talks to a device:

- `--timeout <ms>` — Read timeout (default **1500**)  
- `--baud <n>` — Baud rate (default **115200**)  
- `--boot-delay <ms>` — Delay after open to absorb USB CDC reset (default **400**)  

---

## Defaults & Behavior
- If neither `--node` nor `--dev` is given, the CLI scans:
  - If exactly one online node → auto-select that device  
  - If multiple are online →  
    ```
    status=error reason=multiple_nodes_connected
    ```
  - If none online →  
    ```
    status=error reason=no_nodes_online
    ```

- `--aliases` is only effective with `--scan` (program exits after the scan branch).

---

## Examples
```bash
viatext-cli --scan --aliases
viatext-cli --get freq --node HckrMn
viatext-cli --set sf 7 --node KngFry
viatext-cli --get all --dev /dev/serial/by-id/usb-ACME_Node123
viatext-cli --set alias field-gateway --node N3
viatext-cli --ping --node N3 --timeout 2000
viatext-cli --set-id vt-01 --dev /dev/ttyACM0
```
