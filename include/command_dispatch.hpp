#pragma once
/**
 * @page vt-command-dispatch ViaText Command Dispatcher
 * @file command_dispatch.hpp
 * @brief Centralized resolution of CLI options → protocol packets.
 * @see docs/commands.md
 * @see docs/command_flow.md
 * @see docs/add_parameter_steps.md
 * 
 * @details
 * PURPOSE
 * -------
 * The dispatcher is the **glue layer** between high-level CLI arguments
 * and low-level packet builders in commands.hpp/cpp. It exists so that:
 *   - `main.cpp` never has to know about individual builders.
 *   - New parameters can be added by editing *only* this file + commands.cpp.
 *   - Parsing, validation, and packet selection are kept in one place.
 *
 * WHAT THIS DOES
 * --------------
 * - Defines an enum `CommandKind` that lists every supported operation:
 *   both GET_* and SET_* forms, plus legacy verbs like PING and GET_ID.
 * - Provides `name_to_kind()` which maps user-facing strings like
 *   `"freq"` or `"alias"` (and whether it’s a GET or SET) into
 *   the corresponding `CommandKind`.
 *   * This normalizes synonyms (e.g., `"fw"` and `"fw_version"`).
 *   * It enforces which parameters are read-only (no SET variant).
 * - Provides `build_packet_from_kind()` which takes a `CommandKind`
 *   (plus an optional string value for SETs) and returns the exact
 *   TLV-framed request bytes using the builders in commands.cpp.
 *   * Includes validation: `--set sf 99` will fail with `err="bad_value:sf"`.
 * - Provides wrappers:
 *   * `build_legacy_packet()` for compatibility with the old `--get-id`, `--ping`,
 *     and `--set-id` flags.
 *   * `build_param_get_packet()` / `build_param_set_packet()` for
 *     the modern `--get` / `--set` CLI style.
 *
 * HOW IT FITS INTO VIATEXT
 * ------------------------
 * - **main.cpp**: Parses CLI args, then calls into this dispatcher to
 *   produce exactly one outbound request packet. No protocol knowledge
 *   is embedded in main.cpp itself.
 * - **commands.hpp/cpp**: Houses the concrete builders like
 *   `make_get_freq()` or `make_set_alias()`. The dispatcher is the
 *   central switchboard that decides which one to call.
 * - **serial_io.hpp/cpp**: Consumes the packet produced here, frames it
 *   with SLIP, and pushes it to the node.
 *
 * PROCESS FLOW
 * ------------
 * 1. CLI args parsed in main.cpp (e.g., `--set freq 915000000`).
 * 2. main.cpp calls `build_param_set_packet("freq", "915000000", seq, out, err)`.
 * 3. Dispatcher → `name_to_kind("freq", true)` → `CommandKind::SET_FREQ_HZ`.
 * 4. Dispatcher → `build_packet_from_kind(SET_FREQ_HZ, seq, "915000000")`.
 * 5. Value is parsed, validated (must fit in 32-bit Hz), builder is called.
 * 6. `out` now holds the ready-to-send request.
 *
 * DESIGN ADVANTAGES
 * -----------------
 * - **Single point of truth**: All valid command names live in one switch.
 * - **Robust validation**: Numeric ranges checked before hitting firmware.
 * - **Extendable**: Adding new parameter = add builder + enum entry + string alias.
 * - **Keeps CLI thin**: main.cpp never explodes into a maze of if/else for each param.
 *
 * TRADE-OFFS
 * ----------
 * - Requires manual maintenance of the enum + switch (no codegen).
 * - Slight redundancy: command names exist here and in commands.cpp,
 *   but this separation ensures CLI errors are caught early and explained.
 *
 * EXAMPLE
 * -------
 *   // main.cpp
 *   std::vector<uint8_t> req;
 *   std::string err;
 *   if (!viatext::build_param_set_packet("sf", "7", 1, req, err)) {
 *       std::cerr << "status=error reason=" << err << "\n";
 *       return 2;
 *   }
 *
 *   // Dispatcher calls make_set_sf(), returns request
 *   // req = [verb=SET_PARAM, seq=1, TLV(tag=TAG_SF, len=1, val=7)]
 *
 * MAINTENANCE
 * -----------
 * - When adding a new parameter:
 *   1) Add the low-level builder(s) in commands.hpp/cpp.
 *   2) Extend CommandKind enum.
 *   3) Extend name_to_kind() to map user string to enum.
 *   4) Extend build_packet_from_kind() to call the right builder,
 *      with parsing/validation if needed.
 * @see docs/add_parameter_steps.md
 * 
 * With this pattern, the CLI and tooling scale without main.cpp changes.
 *
 * @note Errors are surfaced with stable strings like "unknown_get",
 *       "bad_value:sf(7..12)" so scripts can act accordingly.
 *
 * @see commands.hpp  For individual request builders
 * @see serial_io.hpp For serial framing and transport
 */

#include <string>
#include <vector>
#include <cstdint>

namespace viatext {

/**
 * @enum CommandKind
 * @brief Canonical set of supported commands for the ViaText dispatcher.
 *
 * This enum defines the **full menu of operations** that the CLI and dispatcher
 * can translate into TLV-framed protocol packets. Each entry represents a
 * concrete, validated request that can be issued against a ViaText node.
 *
 * - GET_* commands are **read-only** queries. They ignore the `value` string.
 * - SET_* commands are **configuration changes**. They require and validate a `value`.
 * - Diagnostics commands are **read-only probes**, intended for monitoring only.
 * - Legacy commands exist for compatibility with the older CLI flags.
 *
 * DESIGN NOTES
 * ------------
 * - Having a **central enum** means there is one authoritative list of
 *   everything the node can be asked to do. The CLI never builds packets
 *   directly; it resolves into one of these enums, which in turn dispatches
 *   to a builder function.
 * - This avoids "stringly-typed" control logic (e.g. lots of if/else on
 *   `"freq"` or `"alias"`) and enforces compile-time discipline.
 * - By using explicit GET_* and SET_* variants, we avoid ambiguous semantics
 *   and make it clear which parameters are mutable.
 *
 * FIELD UTILITY
 * -------------
 * In practice, this enum is your **command index card**. If you are in a
 * basement, bunker, or backpack deployment and need to know what a ViaText
 * node can do, this is the list. Every valid operation is here.
 *
 * @note To add new parameters: extend this enum, add string mapping in
 *       `name_to_kind()`, and implement the builder in commands.cpp.
 */
enum class CommandKind {

    /** Legacy compatibility: request the node’s unique ID string. */
    GET_ID,

    /** Legacy compatibility: set the node’s ID string manually. */
    SET_ID,

    /** Legacy compatibility: ping the node (round-trip sanity check). */
    PING,

    // Identity / inventory

    /** Get the human-readable alias assigned to the node. */
    GET_ALIAS,

    /** Set the human-readable alias for this node. */
    SET_ALIAS,

    /** Get the firmware version string. */
    GET_FW_VERSION,

    /** Get the uptime in seconds since last boot. */
    GET_UPTIME_S,

    /** Get the absolute boot time in seconds since epoch. */
    GET_BOOT_TIME_S,

    // Radio

    /** Get the radio frequency in Hz. */
    GET_FREQ_HZ,

    /** Set the radio frequency in Hz (requires regulatory compliance). */
    SET_FREQ_HZ,

    /** Get the LoRa spreading factor (SF). */
    GET_SF,

    /** Set the LoRa spreading factor (SF). */
    SET_SF,

    /** Get the radio bandwidth in Hz. */
    GET_BW_HZ,

    /** Set the radio bandwidth in Hz. */
    SET_BW_HZ,

    /** Get the coding rate denominator (e.g., 5, 6, 7, 8). */
    GET_CR_DEN,

    /** Set the coding rate denominator. */
    SET_CR_DEN,

    /** Get the transmit power in dBm. */
    GET_TX_PWR_DBM,

    /** Set the transmit power in dBm. */
    SET_TX_PWR_DBM,

    /** Get the logical channel number. */
    GET_CHAN,

    /** Set the logical channel number. */
    SET_CHAN,

    // Behavior

    /** Get the node’s operating mode (e.g., idle, relay). */
    GET_MODE,

    /** Set the node’s operating mode. */
    SET_MODE,

    /** Get the maximum hop count (TTL) for forwarded packets. */
    GET_HOPS,

    /** Set the maximum hop count (TTL) for forwarded packets. */
    SET_HOPS,

    /** Get the beacon interval in seconds. */
    GET_BEACON_S,

    /** Set the beacon interval in seconds. */
    SET_BEACON_S,

    /** Get the buffer size for queued messages. */
    GET_BUF_SIZE,

    /** Set the buffer size for queued messages. */
    SET_BUF_SIZE,

    /** Get the acknowledgment (ACK) mode setting. */
    GET_ACK_MODE,

    /** Set the acknowledgment (ACK) mode setting. */
    SET_ACK_MODE,

    // Diagnostics (RO)

    /** Get the last received signal strength (RSSI) in dBm. */
    GET_RSSI_DBM,

    /** Get the last received signal-to-noise ratio (SNR) in dB. */
    GET_SNR_DB,

    /** Get the node’s supply voltage in millivolts. */
    GET_VBAT_MV,

    /** Get the node’s onboard temperature in Celsius. */
    GET_TEMP_C,

    /** Get the amount of free RAM in bytes. */
    GET_FREE_MEM_B,

    /** Get the amount of free flash storage in bytes. */
    GET_FREE_FLASH_B,

    /** Get the count of log entries stored. */
    GET_LOG_COUNT,

    // Bulk

    /** Request a snapshot of all parameters in one packet. */
    GET_ALL
};


/**
 * @brief Build a protocol packet from a canonical CommandKind.
 *
 * Translates a high-level command (from CommandKind) into the exact
 * TLV-framed byte sequence accepted by a ViaText node. This centralizes
 * validation and encoding so that the CLI and higher layers never need
 * to know about low-level packet structure.
 *
 * USAGE
 * -----
 * - For **GET_* commands**: pass the kind and a sequence number.
 *   The `value` parameter is ignored.
 * - For **SET_* commands**: pass the kind, a sequence number, and a string
 *   `value`. This will be parsed and validated against acceptable ranges.
 *   If invalid (non-numeric or out-of-range), the function fails and
 *   `err` describes the issue.
 *
 * PARAMETERS
 * ----------
 * @param kind   Command to issue (from CommandKind enum).
 * @param seq    Sequence number (caller-managed, used to correlate replies).
 * @param value  String payload for SET_* commands (ignored for GET_*).
 * @param out    Vector to be filled with the encoded packet bytes.
 * @param err    On failure, contains a stable error string such as
 *               "bad_value:sf(7..12)" or "unhandled_command".
 *
 * RETURNS
 * -------
 * @retval true   Packet successfully built and stored in `out`.
 * @retval false  Failure; see `err` for the reason.
 *
 * FIELD UTILITY
 * -------------
 * - Ensures invalid or out-of-range settings never reach firmware.
 * - Provides one guaranteed place for packet correctness.
 * - Saves airtime and battery by preventing malformed commands from
 *   being transmitted in resource-constrained or off-grid deployments.
 *
 * DESIGN NOTES
 * ------------
 * - Switch-based dispatch makes command handling explicit and auditable.
 * - All parsing is local and exception-free (safe for embedded use).
 * - Error strings are short, stable, and script-friendly.
 */
bool build_packet_from_kind(CommandKind kind,
                            uint8_t seq,
                            const std::string& value,
                            std::vector<uint8_t>& out,
                            std::string& err);


/**
 * @brief Resolve a user-facing name and operation into a CommandKind.
 *
 * Maps normalized parameter names (e.g., "id", "freq", "sf") plus the
 * intended operation (GET vs SET) into a canonical CommandKind enum.
 * This is where human-facing strings are translated into machine-facing
 * instructions.
 *
 * USAGE
 * -----
 * - Input names are expected in lowercase; callers typically run them
 *   through a normalizer before calling this function.
 * - The `is_set` flag determines whether the SET_* or GET_* variant
 *   should be returned when both exist.
 * - Aliases and legacy names are supported (e.g., "fw" → GET_FW_VERSION,
 *   "set-id" → SET_ID).
 *
 * PARAMETERS
 * ----------
 * @param name_normalized_lower  The normalized, lowercase parameter name
 *                               (e.g., "freq", "alias", "uptime").
 * @param is_set                 True to request a SET_* operation,
 *                               false for GET_*.
 * @param out_kind               On success, set to the resolved CommandKind.
 *
 * RETURNS
 * -------
 * @retval true   Successfully resolved to a known CommandKind.
 * @retval false  Name was not recognized; `out_kind` is undefined.
 *
 * FIELD UTILITY
 * -------------
 * - Provides a single, authoritative mapping between user input and
 *   protocol semantics.
 * - Normalizes synonyms and legacy names so the CLI can remain forgiving
 *   while the core protocol stays strict.
 * - Ensures that unsupported parameters are rejected early, saving
 *   airtime and avoiding invalid packets.
 *
 * DESIGN NOTES
 * ------------
 * - Aliases are hard-coded here for transparency and offline readability.
 * - Lookup is O(n) through chained if-statements, chosen for simplicity
 *   and small binary size over more complex data structures.
 * - Errors are not silent: unknown inputs cleanly return false so higher
 *   layers can explain the failure.
 */
bool name_to_kind(const std::string& name_normalized_lower,
                  bool is_set,
                  CommandKind& out_kind);

/**
 * @brief Build a legacy packet from classic flags (exactly one must be set).
 *
 * Compatibility shim for the original CLI style that exposed three separate
 * switches: get-id, ping, and set-id. This helper enforces that callers
 * request exactly one operation, then delegates to build_packet_from_kind.
 *
 * PARAMETERS
 * ----------
 * @param get_id       True to issue a GET_ID request.
 * @param ping         True to issue a PING request.
 * @param set_id_value Non-empty string to issue a SET_ID request; empty means unused.
 * @param seq          Sequence number for the request.
 * @param out          Filled with the encoded packet on success.
 * @param err          On failure, contains a stable error string, e.g.
 *                     "need_exactly_one_command".
 *
 * RETURNS
 * -------
 * @retval true   Exactly one legacy command was selected and encoded.
 * @retval false  Zero or multiple commands selected; or downstream encoding failed.
 *
 * FIELD UTILITY
 * -------------
 * - Keeps old scripts working while the modern param system takes over.
 * - Useful when porting tooling incrementally without touching protocol code.
 *
 * DESIGN NOTES
 * ------------
 * - Counts selected operations and rejects anything but exactly one.
 * - Delegates to build_packet_from_kind for actual packet construction.
 */
bool build_legacy_packet(bool get_id,
                         bool ping,
                         const std::string& set_id_value, // empty if not used
                         uint8_t seq,
                         std::vector<uint8_t>& out,
                         std::string& err);

/**
 * @brief Build a GET_* packet from a user-facing parameter name.
 *
 * Resolves a normalized parameter name (e.g., "freq", "alias", "rssi")
 * to its CommandKind (GET variant) and constructs the request packet.
 * This is the thin, user-friendly entry point used by main.cpp.
 *
 * PARAMETERS
 * ----------
 * @param name  Normalized, lowercase parameter name.
 * @param seq   Sequence number for the request.
 * @param out   Filled with the encoded packet on success.
 * @param err   On failure, contains a stable error string, e.g. "unknown_get".
 *
 * RETURNS
 * -------
 * @retval true   Name resolved and packet constructed.
 * @retval false  Name not recognized (unknown_get) or downstream failure.
 *
 * FIELD UTILITY
 * -------------
 * - Keeps CLI parsing simple: call once, get a correct packet or a clear error.
 * - Rejects unsupported queries early, saving airtime and battery.
 *
 * DESIGN NOTES
 * ------------
 * - Uses name_to_kind(name, false, out_kind) for resolution.
 * - Delegates encoding to build_packet_from_kind.
 */
bool build_param_get_packet(const std::string& name, uint8_t seq,
                            std::vector<uint8_t>& out, std::string& err);

/**
 * @brief Build a SET_* packet from a user-facing parameter name and value.
 *
 * Resolves a normalized parameter name to its CommandKind (SET variant),
 * validates the provided string value against the expected type/range,
 * and constructs the request packet. Validation errors are surfaced as
 * short, stable strings (e.g., "bad_value:sf(7..12)").
 *
 * PARAMETERS
 * ----------
 * @param name   Normalized, lowercase parameter name.
 * @param value  String value to set (parsed and validated per parameter).
 * @param seq    Sequence number for the request.
 * @param out    Filled with the encoded packet on success.
 * @param err    On failure, contains a stable error string, e.g. "unknown_set"
 *               or a specific validation message like "bad_value:tx_pwr_dbm(-20..23)".
 *
 * RETURNS
 * -------
 * @retval true   Name resolved, value valid, and packet constructed.
 * @retval false  Name not recognized (unknown_set) or value invalid.
 *
 * FIELD UTILITY
 * -------------
 * - Centralizes range/type checks so mistakes do not reach firmware or radio.
 * - Script-friendly errors make recovery deterministic in low-connectivity contexts.
 *
 * DESIGN NOTES
 * ------------
 * - Uses name_to_kind(name, true, out_kind) for resolution.
 * - Delegates parsing/encoding to build_packet_from_kind, which performs
 *   strict local, exception-free validation.
 */
bool build_param_set_packet(const std::string& name, const std::string& value, uint8_t seq,
                            std::vector<uint8_t>& out, std::string& err);


} // namespace viatext
