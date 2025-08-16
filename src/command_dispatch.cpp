// -----------------------------------------------------------------------------
// Implementation for command_dispatch.hpp
// 
// This file provides the working guts of the ViaText command dispatcher.
// - See command_dispatch.hpp for API contracts, design overview, and examples.
// - See tests/test-cli/ for runnable cases that exercise these builders.
//
// Notes for maintainers:
// - This file is implementation-only. The header explains *what*; here we show
//   *how* and *why*.
// - Focus is on local parsing, validation, and switch-based dispatch.
// - Style is defensive and transparent: no exceptions, no hidden magic.
// -----------------------------------------------------------------------------

#include "command_dispatch.hpp"  // matching header: declares dispatcher API and enums
#include "commands.hpp"          // low-level builders (make_get_id, make_set_freq, etc.)

#include <algorithm>             // std::tolower helper needs a loop-friendly include
#include <cctype>                // character classification and case conversion
#include <cstdlib>               // strtol / strtoull for safe string→number parsing

namespace viatext {

// ---------- local parsing helpers (no exceptions) ----------
// These functions convert strings from CLI input into fixed-width integers.
// - All parsing is done with strtol/strtoull (C stdlib) for predictability.
// - No exceptions are thrown; failure is always signaled by "false".
// - Each parser enforces explicit bounds so bad values never leak downstream.
// - Chosen ranges match the expected fields in ViaText protocol headers.

static bool parse_u8(const std::string& s, uint8_t& out,
                     uint32_t lo=0, uint32_t hi=255) {
    // strtol: parse string as integer, "e" marks the end pointer
    char* e = nullptr;
    long v = std::strtol(s.c_str(), &e, 0);

    // Reject if parsing failed or leftover junk chars exist
    if (!e || *e) return false;

    // Range check: must fit within lo..hi
    if (v < (long)lo || v > (long)hi) return false;

    // Safe to cast down to 8-bit
    out = (uint8_t)v;
    return true;
}

static bool parse_i8(const std::string& s, int8_t& out,
                     int32_t lo=-128, int32_t hi=127) {
    char* e = nullptr;
    long v = std::strtol(s.c_str(), &e, 0);
    if (!e || *e) return false;
    if (v < lo || v > hi) return false;

    // Signed case: cast down into int8_t after checks
    out = (int8_t)v;
    return true;
}

static bool parse_u16(const std::string& s, uint16_t& out,
                      uint32_t lo=0, uint32_t hi=65535) {
    char* e = nullptr;
    long v = std::strtol(s.c_str(), &e, 0);
    if (!e || *e) return false;
    if (v < (long)lo || v > (long)hi) return false;

    // Fits within 16-bit unsigned
    out = (uint16_t)v;
    return true;
}

static bool parse_u32(const std::string& s, uint32_t& out,
                      uint64_t lo=0, uint64_t hi=0xFFFFFFFFull) {
    char* e = nullptr;
    unsigned long long v = std::strtoull(s.c_str(), &e, 0);
    if (!e || *e) return false;
    if (v < lo || v > hi) return false;

    // Fits within 32-bit unsigned
    out = (uint32_t)v;
    return true;
}

// ---------- lowercase normalizer ----------
// Convert a string into lowercase, char by char.
// - Helps normalize CLI parameters so aliases match consistently.
// - Cast to unsigned char first so std::tolower is well-defined
//   (avoids UB on chars with high bit set).
static std::string lower(std::string s) {
    for (auto& c : s)
        c = (char)std::tolower((unsigned char)c);
    return s;
}

// ---------- mapping: (name, is_set) -> CommandKind ----------
// Translate a user-facing name plus operation bit (is_set) into a canonical enum.
// - Input "raw_name" may contain mixed case or legacy spellings; we normalize.
// - We prefer explicit branches over tables here for transparency and offline audit.
// - Lookup is O(n) and fine for this small, fixed vocabulary; clarity > micro-optimizations.
bool name_to_kind(const std::string& raw_name, bool is_set, CommandKind& out_kind) {
    const std::string name = lower(raw_name);  // normalize early so aliases match reliably

    // Legacy / explicit names
    // Ambiguity note: "id" is overloaded; GET when !is_set, SET when is_set.
    // This split ensures "id" alone routes correctly based on operation.
    if (!is_set) {
        // GET: return the node’s current ID
        if (name == "id" || name == "get-id") { out_kind = CommandKind::GET_ID; return true; }

        // GET: basic connectivity check
        if (name == "ping")                   { out_kind = CommandKind::PING;   return true; }
    } else {
        // SET: assign a new node ID
        if (name == "id" || name == "set-id") { out_kind = CommandKind::SET_ID; return true; }
    }

    // Identity / inventory
    // Note: "alias" is both GET and SET; "fw" has a synonym "fw_version".
    // "uptime" vs "uptime_s" and "boot_time" vs "boot_time_s" are accepted for ergonomics.
    if (!is_set) {
        // Request node alias string
        if (name == "alias")         { out_kind = CommandKind::GET_ALIAS; return true; }

        // Request firmware version
        if (name == "fw" || name=="fw_version") { out_kind = CommandKind::GET_FW_VERSION; return true; }

        // Request node uptime in seconds
        if (name == "uptime" || name=="uptime_s"){ out_kind = CommandKind::GET_UPTIME_S;   return true; }

        // Request node boot time in seconds since epoch
        if (name == "boot_time" || name=="boot_time_s") { out_kind = CommandKind::GET_BOOT_TIME_S; return true; }
    } else {
        // Set node alias string
        if (name == "alias")         { out_kind = CommandKind::SET_ALIAS; return true; }
    }


    // Radio
    // Aliases accepted for power: "tx_pwr" and short "pwr".
    // Ambiguity note: these names are terse by design; ranges are enforced later at build time.
    if (!is_set) {
        // GET: operating frequency in Hz (e.g. 915000000)
        if (name == "freq")    { out_kind = CommandKind::GET_FREQ_HZ; return true; }

        // GET: spreading factor (LoRa SF7..SF12, controls range vs data rate)
        if (name == "sf")      { out_kind = CommandKind::GET_SF;      return true; }

        // GET: bandwidth in Hz (e.g. 125000, 250000, 500000)
        if (name == "bw")      { out_kind = CommandKind::GET_BW_HZ;   return true; }

        // GET: coding rate denominator (LoRa CR 5..8, adds error correction overhead)
        if (name == "cr")      { out_kind = CommandKind::GET_CR_DEN;  return true; }

        // GET: transmit power in dBm (alias "tx_pwr" or short "pwr")
        if (name == "tx_pwr" || name == "pwr") { out_kind = CommandKind::GET_TX_PWR_DBM; return true; }

        // GET: logical channel index (firmware-defined channel plan)
        if (name == "chan")    { out_kind = CommandKind::GET_CHAN;    return true; }
    } else {
        // SET: operating frequency in Hz
        if (name == "freq")    { out_kind = CommandKind::SET_FREQ_HZ; return true; }

        // SET: spreading factor (7–12)
        if (name == "sf")      { out_kind = CommandKind::SET_SF;      return true; }

        // SET: bandwidth in Hz
        if (name == "bw")      { out_kind = CommandKind::SET_BW_HZ;   return true; }

        // SET: coding rate denominator (5–8)
        if (name == "cr")      { out_kind = CommandKind::SET_CR_DEN;  return true; }

        // SET: transmit power in dBm
        if (name == "tx_pwr" || name == "pwr") { out_kind = CommandKind::SET_TX_PWR_DBM; return true; }

        // SET: logical channel index
        if (name == "chan")    { out_kind = CommandKind::SET_CHAN;    return true; }
    }


    // Behavior
    // Symmetric GET/SET pairs; "beacon" and "beacon_s" both mean seconds.
    // Ambiguity note: "mode" is an opaque numeric enum at this layer; decoding is firmware-side.
    if (!is_set) {
        // GET: operating mode (numeric, meaning defined in firmware)
        if (name == "mode")     { out_kind = CommandKind::GET_MODE;     return true; }

        // GET: maximum hop count (TTL) for relayed packets
        if (name == "hops")     { out_kind = CommandKind::GET_HOPS;     return true; }

        // GET: beacon interval in seconds (alias: "beacon_s")
        if (name == "beacon" || name=="beacon_s") { out_kind = CommandKind::GET_BEACON_S; return true; }

        // GET: buffer size for queued messages
        if (name == "buf_size") { out_kind = CommandKind::GET_BUF_SIZE; return true; }

        // GET: acknowledgment mode (0 = off, 1 = on)
        if (name == "ack")      { out_kind = CommandKind::GET_ACK_MODE; return true; }
    } else {
        // SET: operating mode
        if (name == "mode")     { out_kind = CommandKind::SET_MODE;     return true; }

        // SET: maximum hop count (TTL)
        if (name == "hops")     { out_kind = CommandKind::SET_HOPS;     return true; }

        // SET: beacon interval in seconds
        if (name == "beacon" || name=="beacon_s") { out_kind = CommandKind::SET_BEACON_S; return true; }

        // SET: buffer size for queued messages
        if (name == "buf_size") { out_kind = CommandKind::SET_BUF_SIZE; return true; }

        // SET: acknowledgment mode
        if (name == "ack")      { out_kind = CommandKind::SET_ACK_MODE; return true; }
    }

    // Diagnostics (RO)
    // Read-only probes. If "is_set" is true for these names, we fall through and return false.
    // This early rejection prevents "SET rssi" style invalid ops from reaching the builder stage.
    if (!is_set) {
        // GET: last received signal strength (RSSI, in dBm)
        if (name == "rssi")       { out_kind = CommandKind::GET_RSSI_DBM;   return true; }

        // GET: last signal-to-noise ratio (in dB)
        if (name == "snr")        { out_kind = CommandKind::GET_SNR_DB;     return true; }

        // GET: supply voltage (in millivolts)
        if (name == "vbat")       { out_kind = CommandKind::GET_VBAT_MV;    return true; }

        // GET: onboard temperature (in °C)
        if (name == "temp")       { out_kind = CommandKind::GET_TEMP_C;     return true; }

        // GET: free memory (in bytes)
        if (name == "free_mem")   { out_kind = CommandKind::GET_FREE_MEM_B; return true; }

        // GET: free flash storage (in bytes)
        if (name == "free_flash") { out_kind = CommandKind::GET_FREE_FLASH_B; return true; }

        // GET: number of stored log entries
        if (name == "log_count")  { out_kind = CommandKind::GET_LOG_COUNT;  return true; }
    }

    // Bulk
    // "all" is a convenience alias for a snapshot read of common fields.
    if (!is_set) {
        // GET: one-shot snapshot of all parameters
        if (name == "all" || name == "get_all") { out_kind = CommandKind::GET_ALL; return true; }
    }


    // No match found.
    // Maintenance note: add new names in the appropriate section above.
    // Prefer explicit alias pairs (GET/SET) to keep failure modes obvious.
    return false;
}

// ---------- switch-based builder ----------
bool build_packet_from_kind(CommandKind kind,
                            uint8_t seq,
                            const std::string& value,
                            std::vector<uint8_t>& out,
                            std::string& err)
{
    out.clear();

    switch (kind) {

        // ===== Legacy / basic =====

        case CommandKind::GET_ID: {
            // Build a GET request for the node's unique ID string.
            out = make_get_id(seq);
            return true;
        }

        case CommandKind::SET_ID: {
            // Build a SET request to assign the node's unique ID (uses `value` as new ID).
            out = make_set_id(seq, value);
            return true;
        }

        case CommandKind::PING: {
            // Build a PING request (round-trip sanity check).
            out = make_ping(seq);
            return true;
        }

        // ===== Identity / inventory =====

        case CommandKind::GET_ALIAS: {
            // Read the node's human-readable alias.
            out = make_get_alias(seq);
            return true;
        }

        case CommandKind::SET_ALIAS: {
            // Set the node's human-readable alias (uses `value`).
            out = make_set_alias(seq, value);
            return true;
        }

        case CommandKind::GET_FW_VERSION: {
            // Read firmware version string.
            out = make_get_fw_version(seq);
            return true;
        }

        case CommandKind::GET_UPTIME_S: {
            // Read uptime in seconds since last boot.
            out = make_get_uptime(seq);
            return true;
        }

        case CommandKind::GET_BOOT_TIME_S: {
            // Read absolute boot time (epoch seconds).
            out = make_get_boot_time(seq);
            return true;
        }

        // ===== Radio =====

        case CommandKind::GET_FREQ_HZ: {
            // Read RF center frequency in Hz.
            out = make_get_freq(seq);
            return true;
        }

        case CommandKind::SET_FREQ_HZ: {
            // Set RF center frequency in Hz; parse and validate numeric `value`.
            uint32_t hz;
            if (!parse_u32(value, hz)) { err = "bad_value:freq_hz"; return false; }
            out = make_set_freq(seq, hz);
            return true;
        }

        case CommandKind::GET_SF: {
            // Read LoRa spreading factor (SF7..SF12).
            out = make_get_sf(seq);
            return true;
        }

        case CommandKind::SET_SF: {
            // Set LoRa spreading factor (must be 7..12).
            uint8_t sf;
            if (!parse_u8(value, sf, 7, 12)) { err = "bad_value:sf(7..12)"; return false; }
            out = make_set_sf(seq, sf);
            return true;
        }

        case CommandKind::GET_BW_HZ: {
            // Read LoRa bandwidth in Hz (e.g., 125000/250000/500000).
            out = make_get_bw(seq);
            return true;
        }

        case CommandKind::SET_BW_HZ: {
            // Set LoRa bandwidth in Hz; parse and validate numeric `value`.
            uint32_t bw;
            if (!parse_u32(value, bw)) { err = "bad_value:bw_hz"; return false; }
            out = make_set_bw(seq, bw);
            return true;
        }

        case CommandKind::GET_CR_DEN: {
            // Read LoRa coding rate denominator (5..8).
            out = make_get_cr(seq);
            return true;
        }

        case CommandKind::SET_CR_DEN: {
            // Set LoRa coding rate denominator (must be 5..8).
            uint8_t cr;
            if (!parse_u8(value, cr, 5, 8)) { err = "bad_value:cr(5..8)"; return false; }
            out = make_set_cr(seq, cr);
            return true;
        }

        case CommandKind::GET_TX_PWR_DBM: {
            // Read TX power in dBm.
            out = make_get_tx_pwr(seq);
            return true;
        }

        case CommandKind::SET_TX_PWR_DBM: {
            // Set TX power in dBm (must be -20..23).
            int8_t dbm;
            if (!parse_i8(value, dbm, -20, 23)) { err = "bad_value:tx_pwr_dbm(-20..23)"; return false; }
            out = make_set_tx_pwr(seq, dbm);
            return true;
        }

        case CommandKind::GET_CHAN: {
            // Read logical channel index.
            out = make_get_chan(seq);
            return true;
        }

        case CommandKind::SET_CHAN: {
            // Set logical channel index; parse and validate numeric `value`.
            uint8_t ch;
            if (!parse_u8(value, ch)) { err = "bad_value:chan"; return false; }
            out = make_set_chan(seq, ch);
            return true;
        }

        // ===== Behavior =====

        case CommandKind::GET_MODE: {
            // Read operating mode (opaque numeric enum; defined in firmware).
            out = make_get_mode(seq);
            return true;
        }

        case CommandKind::SET_MODE: {
            // Set operating mode (opaque numeric enum); parse `value`.
            uint8_t m;
            if (!parse_u8(value, m)) { err = "bad_value:mode"; return false; }
            out = make_set_mode(seq, m);
            return true;
        }

        case CommandKind::GET_HOPS: {
            // Read maximum hop count (TTL) for relayed packets.
            out = make_get_hops(seq);
            return true;
        }

        case CommandKind::SET_HOPS: {
            // Set maximum hop count (TTL); parse `value`.
            uint8_t h;
            if (!parse_u8(value, h)) { err = "bad_value:hops"; return false; }
            out = make_set_hops(seq, h);
            return true;
        }

        case CommandKind::GET_BEACON_S: {
            // Read beacon interval in seconds.
            out = make_get_beacon(seq);
            return true;
        }

        case CommandKind::SET_BEACON_S: {
            // Set beacon interval in seconds; parse `value`.
            uint32_t s;
            if (!parse_u32(value, s)) { err = "bad_value:beacon_s"; return false; }
            out = make_set_beacon(seq, s);
            return true;
        }

        case CommandKind::GET_BUF_SIZE: {
            // Read buffer size for queued messages (bytes/entries; firmware-defined).
            out = make_get_buf_size(seq);
            return true;
        }

        case CommandKind::SET_BUF_SIZE: {
            // Set buffer size; parse `value`.
            uint16_t n;
            if (!parse_u16(value, n)) { err = "bad_value:buf_size"; return false; }
            out = make_set_buf_size(seq, n);
            return true;
        }

        case CommandKind::GET_ACK_MODE: {
            // Read acknowledgment mode (0=off, 1=on).
            out = make_get_ack_mode(seq);
            return true;
        }

        case CommandKind::SET_ACK_MODE: {
            // Set acknowledgment mode (must be 0 or 1).
            uint8_t on;
            if (!parse_u8(value, on, 0, 1)) { err = "bad_value:ack(0|1)"; return false; }
            out = make_set_ack_mode(seq, on);
            return true;
        }

        // ===== Diagnostics (RO) =====

        case CommandKind::GET_RSSI_DBM: {
            // Read last received signal strength (RSSI, dBm).
            out = make_get_rssi(seq);
            return true;
        }

        case CommandKind::GET_SNR_DB: {
            // Read last signal-to-noise ratio (dB).
            out = make_get_snr(seq);
            return true;
        }

        case CommandKind::GET_VBAT_MV: {
            // Read supply voltage (millivolts).
            out = make_get_vbat(seq);
            return true;
        }

        case CommandKind::GET_TEMP_C: {
            // Read onboard temperature (°C).
            out = make_get_temp(seq);
            return true;
        }

        case CommandKind::GET_FREE_MEM_B: {
            // Read free RAM (bytes).
            out = make_get_free_mem(seq);
            return true;
        }

        case CommandKind::GET_FREE_FLASH_B: {
            // Read free flash storage (bytes).
            out = make_get_free_flash(seq);
            return true;
        }

        case CommandKind::GET_LOG_COUNT: {
            // Read count of stored log entries.
            out = make_get_log_count(seq);
            return true;
        }

        // ===== Bulk =====

        case CommandKind::GET_ALL: {
            // Request a one-shot snapshot of common parameters.
            out = make_get_all(seq);
            return true;
        }
    }

    // If we ever add a new enum and forget to handle it here:
    err = "unhandled_command";
    return false;
}


// ---------- public helpers ----------
// Build a legacy packet using the old CLI flag style (--get-id, --ping, --set-id).
// Exactly one of these operations must be selected; otherwise we fail early.
// This keeps backward compatibility for scripts that haven't migrated to the
// modern parameter-based interface.
bool build_legacy_packet(bool get_id,
                         bool ping,
                         const std::string& set_id_value,
                         uint8_t seq,
                         std::vector<uint8_t>& out,
                         std::string& err)
{
    out.clear(); // always start with an empty output buffer

    // Count how many commands were requested.
    // Only one is allowed: GET_ID, PING, or SET_ID.
    int count = (get_id ? 1 : 0) +
                (ping ? 1 : 0) +
                (!set_id_value.empty() ? 1 : 0);

    // Reject invalid usage (zero or multiple commands set).
    if (count != 1) {
        err = "need_exactly_one_command";
        return false;
    }

    // Dispatch to the correct builder:
    if (get_id) {
        // Legacy GET_ID request
        return build_packet_from_kind(CommandKind::GET_ID, seq, "", out, err);
    }

    if (ping) {
        // Legacy PING request
        return build_packet_from_kind(CommandKind::PING, seq, "", out, err);
    }

    // Otherwise, it must be a SET_ID request (uses provided set_id_value).
    return build_packet_from_kind(CommandKind::SET_ID, seq, set_id_value, out, err);
}


// ---------- public helpers ----------
// Build a GET_* packet from a user-facing parameter name.
//
// Flow:
//  1) Start with a clean output buffer.
//  2) Resolve the human-facing `name` into a canonical CommandKind (GET variant).
//  3) If unknown, fail with a stable error string ("unknown_get").
//  4) Otherwise, build the packet (GET ignores `value`).
bool build_param_get_packet(const std::string& name, uint8_t seq,
                            std::vector<uint8_t>& out, std::string& err)
{
    out.clear();              // 1) ensure no stale bytes are left in `out`
    CommandKind kind;         //    resolved command kind will be stored here

    // 2) map name → CommandKind (GET). If not recognized:
    if (!name_to_kind(name, /*is_set=*/false, kind)) {
        err = "unknown_get";  // 3) stable error for scripts / callers
        return false;
    }

    // 4) dispatch to the low-level builder; GET variants pass an empty `value`
    return build_packet_from_kind(kind, seq, "", out, err);
}


// Build a SET_* packet from a user-facing parameter name and string value.
//
// Flow:
//  1) Start with a clean output buffer.
//  2) Resolve the human-facing `name` into a canonical CommandKind (SET variant).
//  3) If unknown, fail with a stable error string ("unknown_set").
//  4) Otherwise, build the packet; the builder will parse/validate `value`
//     and return a specific error (e.g., "bad_value:sf(7..12)") on failure.
bool build_param_set_packet(const std::string& name, const std::string& value, uint8_t seq,
                            std::vector<uint8_t>& out, std::string& err)
{
    out.clear();              // 1) ensure no stale bytes are left in `out`
    CommandKind kind;         //    resolved command kind will be stored here

    // 2) map name → CommandKind (SET). If not recognized:
    if (!name_to_kind(name, /*is_set=*/true, kind)) {
        err = "unknown_set";  // 3) stable error for scripts / callers
        return false;
    }

    // 4) dispatch to the low-level builder; SET variants include `value`
    return build_packet_from_kind(kind, seq, value, out, err);
}


} // namespace viatext
