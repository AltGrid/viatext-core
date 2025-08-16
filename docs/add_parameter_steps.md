# Adding a New ViaText Parameter (Host Side)

> **Purpose & design**  
> The ViaText host stack separates *what the user asks for* from *how bytes go over serial*.  
> - **commands.hpp/cpp** define the **wire contract** (verbs + TLVs) and human‑readable decoding.  
> - **command_dispatch.hpp/cpp** maps CLI names to those builders and performs **validation**.  
> - **main.cpp** stays thin: it parses flags and asks the dispatcher to build one request.  
>
> This layout keeps `main.cpp` stable, makes parameters easy to extend, and ensures bad inputs are rejected before touching firmware.  
>
> @see `docs/commands.md` (valid CLI names & types)  
> @see `docs/command_flow.md` (host‑side request/response pipeline)

---

## Quick checklist (TL;DR)

1. **Pick a tag & type** in `commands.hpp` (reuse an existing TAG_* or add a new one).
2. **Add builders** in `commands.cpp`:
   - `make_get_<param>(seq)` using `GET_PARAM` + `add_tlv_get(TAG_*)`
   - `make_set_<param>(seq, value)` using `SET_PARAM` + `add_tlv_*` (if the value is writable)
3. **Expose names** in the dispatcher (`command_dispatch.hpp/cpp`):
   - Extend `enum class CommandKind` with `GET_*` / `SET_*`
   - Map CLI strings in `name_to_kind()` (include synonyms if needed)
   - In `build_packet_from_kind()`, call your new builder(s) and **validate inputs**
4. **Make it discoverable to users**:
   - Update the `--get` help string in `main.cpp` to include the new name
   - If it’s settable, ensure `--set` usage matches the type/range
   - Update `docs/commands.md`
5. **Pretty output (optional but recommended)**:
   - In `commands.cpp::decode_pretty()`, add a `case TAG_*` to print a stable `key=value`
6. **Test**: run a GET and, if applicable, a SET against a node; confirm error messages on bad input.

---

## Step‑by‑step guide

### 1) Define (or reuse) the TLV tag in `commands.hpp`
- If your parameter already has a tag, skip this step.
- Otherwise, add it in the correct section with the right wire type comment:
  ```cpp
  // Example (Diagnostics)
  enum : uint8_t {
      TAG_RSSI_DBM = 0x30, // i16: last RX RSSI dBm
      // ...
      TAG_MY_PARAM = 0x37  // u16: description
  };
  ```

**Tip:** Keep comments precise about signedness and units (e.g., `u32 Hz`, `i8 dBm`, `u16 mV`). These comments are the canonical reference for host validation and `decode_pretty()` formatting.

---

### 2) Add request builders in `commands.cpp`
Create minimal, explicit builders mirroring existing ones.

- **Getter** (requesting a value):
  ```cpp
  std::vector<uint8_t> make_get_my_param(uint8_t seq){
      auto b = header(GET_PARAM, seq);
      add_tlv_get(b, TAG_MY_PARAM);
      finalize(b);
      return b;
  }
  ```

- **Setter** (writing a value) — if the parameter is writable:
  ```cpp
  std::vector<uint8_t> make_set_my_param(uint8_t seq, uint16_t v){
      auto b = header(SET_PARAM, seq);
      add_tlv_u16(b, TAG_MY_PARAM, v);
      finalize(b);
      return b;
  }
  ```

Choose the `add_tlv_*` function that matches the on‑wire type (`u8/i8/u16/i16/u32/str`).

---

### 3) Register names and validation in the **dispatcher**

Edit `command_dispatch.hpp/cpp`:

1. **Enum entries** (canonical operations):
   ```cpp
   enum class CommandKind {
       // ...
       GET_MY_PARAM,
       SET_MY_PARAM, // omit if read‑only
       // ...
   };
   ```

2. **CLI name mapping** in `name_to_kind()`:
   ```cpp
   if (!is_set) {
       if (name == "my_param" || name == "myparam") { out_kind = CommandKind::GET_MY_PARAM; return true; }
   } else {
       if (name == "my_param" || name == "myparam") { out_kind = CommandKind::SET_MY_PARAM; return true; }
   }
   ```

3. **Build + validate** in `build_packet_from_kind()`:
   ```cpp
   case CommandKind::GET_MY_PARAM:
       out = make_get_my_param(seq);
       return true;

   case CommandKind::SET_MY_PARAM: {
       uint16_t val;
       if (!parse_u16(value, val, /*lo=*/0, /*hi=*/65535)) { err = "bad_value:my_param"; return false; }
       out = make_set_my_param(seq, val);
       return true;
   }
   ```

> Use the provided `parse_u8/u16/u32/i8` helpers. Clamp ranges to what firmware expects.  
> Return clear errors like `bad_value:my_param` or `bad_value:my_param(0..10)`.

---

### 4) Update `main.cpp` help text (discoverability)

The `--get` option advertises valid names; append yours to the list string so `--help` is accurate. Example snippet to extend (search for the `--get` description):

```
"Get param: id|alias|...|my_param|..."
```

No other `main.cpp` changes are needed — it already calls the dispatcher helpers:

- GET path: `build_param_get_packet(name, seq, out, err)`  
- SET path: `build_param_set_packet(name, value, seq, out, err)`

Remember: **exactly one command** must be chosen per run; that logic is already enforced.

---

### 5) Pretty response output (optional but recommended)

Add a print case to `decode_pretty()` so your parameter shows up as a stable `key=value` pair. Pick a concise, grep‑friendly key (usually the CLI name).

```cpp
case TAG_MY_PARAM: {
    uint16_t v; if (as_u16(t.val, v)) os << " my_param=" << v;
    break;
}
```

If the value has units, print them in the key or value name consistently with the rest of the file (e.g., `freq_hz=...`, `vbat_mv=...`, `tx_pwr_dbm=...`).

---

## Patterns to follow

- **Names**: keep CLI, decode key, and docs aligned (`my_param` everywhere).  
- **Range checks**: fail early in the dispatcher with `status=error reason=bad_value:...`.  
- **Types**: match TLV wire types precisely (host <-> firmware).  
- **Small builders**: mirror existing `make_get_*` / `make_set_*` functions for clarity.  
- **Read‑only params**: define only `GET_*`; don’t add a `SET_*` enum or mapping.  
- **Bulk**: if the value is also included in `GET_ALL`, no extra host work is needed; the node decides which TLVs to emit.

---

## Example: add `buf_hi_water` (u16, writable)

1. `commands.hpp`: add `TAG_BUF_HI_WATER = 0x38 // u16: outbound buffer high‑water mark`  
2. `commands.cpp`: add `make_get_buf_hi_water()` + `make_set_buf_hi_water()` using `add_tlv_u16`  
3. `command_dispatch.hpp/cpp`:
   - Enum: `GET_BUF_HI_WATER`, `SET_BUF_HI_WATER`
   - Names: map `"buf_hi_water"` (and `"bufhi"`, if you want a short alias)
   - Build: parse with `parse_u16(value, v, 0, 65535)`; call new builders
4. `commands.cpp::decode_pretty()`: print `buf_hi_water=<n>`
5. `main.cpp`: extend `--get` description string
6. `docs/commands.md`: add to the GET/SET tables

**Smoke test**:
```bash
viatext-cli --get buf_hi_water --node N3
viatext-cli --set buf_hi_water 512 --node N3
viatext-cli --set buf_hi_water -1   # expect: status=error reason=bad_value:buf_hi_water
```

---

## Error strings (for scripts)

- Unknown name on GET: `status=error reason=unknown_get`  
- Unknown name on SET: `status=error reason=unknown_set`  
- Bad value/range: `status=error reason=bad_value:<name>` (may include a hint like `(7..12)`)  
- Multiple or zero commands: `status=error reason=need_exactly_one_command`  
- Target resolution: `status=error reason=node_not_found id=<id>` / `multiple_nodes_connected` / `no_nodes_online`  
- I/O: `open_failed`, `write_failed`, `timeout`

---

## When *not* to add a parameter

- If the value belongs to a **batch** read (e.g., logs) rather than a scalar setting, prefer a separate verb or a `GET_ALL` inclusion on the device side.  
- If it’s transient debug info that will churn frequently, consider keeping it off the host docs and out of `decode_pretty()` to avoid breaking scripts.

---

### Done? Final sanity pass

- [ ] Builders compile, names resolve, and validation returns clear errors  
- [ ] Help text and `docs/commands.md` updated  
- [ ] GET prints a stable key in `decode_pretty()`  
- [ ] Host round‑trip tested with a real device

