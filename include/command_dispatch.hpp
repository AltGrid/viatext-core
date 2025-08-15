#pragma once
/**
 * @file command_dispatch.hpp
 * @brief Centralized command selection + request building (enum + switch).
 *
 * HOW TO ADD / REMOVE COMMANDS
 * ----------------------------
 * 1) Add a new builder in commands.hpp/cpp (e.g., make_get_foo(), make_set_foo()).
 * 2) Add an enum entry in CommandKind below (GET_FOO / SET_FOO).
 * 3) Add string aliases in name_to_kind() so "--get foo" / "--set foo VAL" map to your enum.
 * 4) Handle the new enum in build_packet_from_kind()'s switch() to call the right builder.
 *
 * With this approach, main.cpp never needs to change when new commands are added:
 * just update this dispatcher + the low-level builders.
 */

#include <string>
#include <vector>
#include <cstdint>

namespace viatext {

/// High-level kinds the dispatcher can build packets for.
/// GET_* kinds ignore `value`; SET_* kinds validate and consume `value`.
enum class CommandKind {
    // Legacy / basic
    GET_ID,
    SET_ID,
    PING,

    // Identity / inventory
    GET_ALIAS,
    SET_ALIAS,
    GET_FW_VERSION,
    GET_UPTIME_S,
    GET_BOOT_TIME_S,

    // Radio
    GET_FREQ_HZ,
    SET_FREQ_HZ,
    GET_SF,
    SET_SF,
    GET_BW_HZ,
    SET_BW_HZ,
    GET_CR_DEN,
    SET_CR_DEN,
    GET_TX_PWR_DBM,
    SET_TX_PWR_DBM,
    GET_CHAN,
    SET_CHAN,

    // Behavior
    GET_MODE,
    SET_MODE,
    GET_HOPS,
    SET_HOPS,
    GET_BEACON_S,
    SET_BEACON_S,
    GET_BUF_SIZE,
    SET_BUF_SIZE,
    GET_ACK_MODE,
    SET_ACK_MODE,

    // Diagnostics (RO)
    GET_RSSI_DBM,
    GET_SNR_DB,
    GET_VBAT_MV,
    GET_TEMP_C,
    GET_FREE_MEM_B,
    GET_FREE_FLASH_B,
    GET_LOG_COUNT,

    // Bulk
    GET_ALL
};

/// Build a packet from a canonical CommandKind. Returns true on success; false with `err` on failure.
/// - `value` is required only for SET_* commands (ignored otherwise).
bool build_packet_from_kind(CommandKind kind,
                            uint8_t seq,
                            const std::string& value,
                            std::vector<uint8_t>& out,
                            std::string& err);

/// Resolve a canonical name (e.g., "id", "freq", "sf") and operation (is_set) to a CommandKind.
/// Returns true on success; false if unknown.
bool name_to_kind(const std::string& name_normalized_lower,
                  bool is_set,
                  CommandKind& out_kind);

/// Legacy helpers (optional):
/// Build from the classic flags (exactly one of get-id / ping / set-id).
bool build_legacy_packet(bool get_id,
                         bool ping,
                         const std::string& set_id_value, // empty if not used
                         uint8_t seq,
                         std::vector<uint8_t>& out,
                         std::string& err);

/// Generic param helpers used by main.cpp:
bool build_param_get_packet(const std::string& name, uint8_t seq,
                            std::vector<uint8_t>& out, std::string& err);

bool build_param_set_packet(const std::string& name, const std::string& value, uint8_t seq,
                            std::vector<uint8_t>& out, std::string& err);

} // namespace viatext
