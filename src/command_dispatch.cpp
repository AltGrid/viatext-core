/**
 * @file command_dispatch.cpp
 */
#include "command_dispatch.hpp"
#include "commands.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>

namespace viatext {

// ---------- local parsing helpers (no exceptions) ----------
static bool parse_u8 (const std::string& s, uint8_t&  out,  uint32_t lo=0,  uint32_t hi=255) {
    char* e=nullptr; long v = std::strtol(s.c_str(), &e, 0);
    if (!e || *e) return false; if (v < (long)lo || v > (long)hi) return false;
    out = (uint8_t)v; return true;
}
static bool parse_i8 (const std::string& s, int8_t&   out,  int32_t lo=-128, int32_t hi=127) {
    char* e=nullptr; long v = std::strtol(s.c_str(), &e, 0);
    if (!e || *e) return false; if (v < lo || v > hi) return false;
    out = (int8_t)v; return true;
}
static bool parse_u16(const std::string& s, uint16_t& out,  uint32_t lo=0,  uint32_t hi=65535) {
    char* e=nullptr; long v = std::strtol(s.c_str(), &e, 0);
    if (!e || *e) return false; if (v < (long)lo || v > (long)hi) return false;
    out = (uint16_t)v; return true;
}
static bool parse_u32(const std::string& s, uint32_t& out,  uint64_t lo=0,  uint64_t hi=0xFFFFFFFFull) {
    char* e=nullptr; unsigned long long v = std::strtoull(s.c_str(), &e, 0);
    if (!e || *e) return false; if (v < lo || v > hi) return false;
    out = (uint32_t)v; return true;
}

static std::string lower(std::string s) {
    for (auto& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

// ---------- mapping: (name, is_set) -> CommandKind ----------
bool name_to_kind(const std::string& raw_name, bool is_set, CommandKind& out_kind) {
    const std::string name = lower(raw_name);

    // Legacy / explicit names
    if (!is_set) {
        if (name == "id" || name == "get-id") { out_kind = CommandKind::GET_ID; return true; }
        if (name == "ping")                    { out_kind = CommandKind::PING;   return true; }
    } else {
        if (name == "id" || name == "set-id")  { out_kind = CommandKind::SET_ID; return true; }
    }

    // Identity / inventory
    if (!is_set) {
        if (name == "alias")         { out_kind = CommandKind::GET_ALIAS; return true; }
        if (name == "fw" || name=="fw_version") { out_kind = CommandKind::GET_FW_VERSION; return true; }
        if (name == "uptime" || name=="uptime_s"){ out_kind = CommandKind::GET_UPTIME_S;   return true; }
        if (name == "boot_time" || name=="boot_time_s") { out_kind = CommandKind::GET_BOOT_TIME_S; return true; }
    } else {
        if (name == "alias")         { out_kind = CommandKind::SET_ALIAS; return true; }
    }

    // Radio
    if (!is_set) {
        if (name == "freq")    { out_kind = CommandKind::GET_FREQ_HZ; return true; }
        if (name == "sf")      { out_kind = CommandKind::GET_SF;      return true; }
        if (name == "bw")      { out_kind = CommandKind::GET_BW_HZ;   return true; }
        if (name == "cr")      { out_kind = CommandKind::GET_CR_DEN;  return true; }
        if (name == "tx_pwr" || name == "pwr") { out_kind = CommandKind::GET_TX_PWR_DBM; return true; }
        if (name == "chan")    { out_kind = CommandKind::GET_CHAN;    return true; }
    } else {
        if (name == "freq")    { out_kind = CommandKind::SET_FREQ_HZ; return true; }
        if (name == "sf")      { out_kind = CommandKind::SET_SF;      return true; }
        if (name == "bw")      { out_kind = CommandKind::SET_BW_HZ;   return true; }
        if (name == "cr")      { out_kind = CommandKind::SET_CR_DEN;  return true; }
        if (name == "tx_pwr" || name == "pwr") { out_kind = CommandKind::SET_TX_PWR_DBM; return true; }
        if (name == "chan")    { out_kind = CommandKind::SET_CHAN;    return true; }
    }

    // Behavior
    if (!is_set) {
        if (name == "mode")     { out_kind = CommandKind::GET_MODE;     return true; }
        if (name == "hops")     { out_kind = CommandKind::GET_HOPS;     return true; }
        if (name == "beacon" || name=="beacon_s") { out_kind = CommandKind::GET_BEACON_S; return true; }
        if (name == "buf_size") { out_kind = CommandKind::GET_BUF_SIZE; return true; }
        if (name == "ack")      { out_kind = CommandKind::GET_ACK_MODE; return true; }
    } else {
        if (name == "mode")     { out_kind = CommandKind::SET_MODE;     return true; }
        if (name == "hops")     { out_kind = CommandKind::SET_HOPS;     return true; }
        if (name == "beacon" || name=="beacon_s") { out_kind = CommandKind::SET_BEACON_S; return true; }
        if (name == "buf_size") { out_kind = CommandKind::SET_BUF_SIZE; return true; }
        if (name == "ack")      { out_kind = CommandKind::SET_ACK_MODE; return true; }
    }

    // Diagnostics (RO)
    if (!is_set) {
        if (name == "rssi")       { out_kind = CommandKind::GET_RSSI_DBM;   return true; }
        if (name == "snr")        { out_kind = CommandKind::GET_SNR_DB;     return true; }
        if (name == "vbat")       { out_kind = CommandKind::GET_VBAT_MV;    return true; }
        if (name == "temp")       { out_kind = CommandKind::GET_TEMP_C;     return true; }
        if (name == "free_mem")   { out_kind = CommandKind::GET_FREE_MEM_B; return true; }
        if (name == "free_flash") { out_kind = CommandKind::GET_FREE_FLASH_B; return true; }
        if (name == "log_count")  { out_kind = CommandKind::GET_LOG_COUNT;  return true; }
    }

    // Bulk
    if (!is_set) {
        if (name == "all" || name == "get_all") { out_kind = CommandKind::GET_ALL; return true; }
    }

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
        // Legacy / basic
        case CommandKind::GET_ID:         out = make_get_id(seq); return true;
        case CommandKind::SET_ID:         out = make_set_id(seq, value); return true;
        case CommandKind::PING:           out = make_ping(seq); return true;

        // Identity / inventory
        case CommandKind::GET_ALIAS:      out = make_get_alias(seq); return true;
        case CommandKind::SET_ALIAS:      out = make_set_alias(seq, value); return true;
        case CommandKind::GET_FW_VERSION: out = make_get_fw_version(seq); return true;
        case CommandKind::GET_UPTIME_S:   out = make_get_uptime(seq); return true;
        case CommandKind::GET_BOOT_TIME_S:out = make_get_boot_time(seq); return true;

        // Radio
        case CommandKind::GET_FREQ_HZ:    out = make_get_freq(seq); return true;
        case CommandKind::SET_FREQ_HZ: {
            uint32_t hz; if (!parse_u32(value, hz)) { err="bad_value:freq_hz"; return false; }
            out = make_set_freq(seq, hz); return true;
        }
        case CommandKind::GET_SF:         out = make_get_sf(seq); return true;
        case CommandKind::SET_SF: {
            uint8_t sf; if (!parse_u8(value, sf, 7, 12)) { err="bad_value:sf(7..12)"; return false; }
            out = make_set_sf(seq, sf); return true;
        }
        case CommandKind::GET_BW_HZ:      out = make_get_bw(seq); return true;
        case CommandKind::SET_BW_HZ: {
            uint32_t bw; if (!parse_u32(value, bw)) { err="bad_value:bw_hz"; return false; }
            out = make_set_bw(seq, bw); return true;
        }
        case CommandKind::GET_CR_DEN:     out = make_get_cr(seq); return true;
        case CommandKind::SET_CR_DEN: {
            uint8_t cr; if (!parse_u8(value, cr, 5, 8)) { err="bad_value:cr(5..8)"; return false; }
            out = make_set_cr(seq, cr); return true;
        }
        case CommandKind::GET_TX_PWR_DBM: out = make_get_tx_pwr(seq); return true;
        case CommandKind::SET_TX_PWR_DBM: {
            int8_t dbm; if (!parse_i8(value, dbm, -20, 23)) { err="bad_value:tx_pwr_dbm(-20..23)"; return false; }
            out = make_set_tx_pwr(seq, dbm); return true;
        }
        case CommandKind::GET_CHAN:       out = make_get_chan(seq); return true;
        case CommandKind::SET_CHAN: {
            uint8_t ch; if (!parse_u8(value, ch)) { err="bad_value:chan"; return false; }
            out = make_set_chan(seq, ch); return true;
        }

        // Behavior
        case CommandKind::GET_MODE:       out = make_get_mode(seq); return true;
        case CommandKind::SET_MODE: {
            uint8_t m; if (!parse_u8(value, m)) { err="bad_value:mode"; return false; }
            out = make_set_mode(seq, m); return true;
        }
        case CommandKind::GET_HOPS:       out = make_get_hops(seq); return true;
        case CommandKind::SET_HOPS: {
            uint8_t h; if (!parse_u8(value, h)) { err="bad_value:hops"; return false; }
            out = make_set_hops(seq, h); return true;
        }
        case CommandKind::GET_BEACON_S:   out = make_get_beacon(seq); return true;
        case CommandKind::SET_BEACON_S: {
            uint32_t s; if (!parse_u32(value, s)) { err="bad_value:beacon_s"; return false; }
            out = make_set_beacon(seq, s); return true;
        }
        case CommandKind::GET_BUF_SIZE:   out = make_get_buf_size(seq); return true;
        case CommandKind::SET_BUF_SIZE: {
            uint16_t n; if (!parse_u16(value, n)) { err="bad_value:buf_size"; return false; }
            out = make_set_buf_size(seq, n); return true;
        }
        case CommandKind::GET_ACK_MODE:   out = make_get_ack_mode(seq); return true;
        case CommandKind::SET_ACK_MODE: {
            uint8_t on; if (!parse_u8(value, on, 0, 1)) { err="bad_value:ack(0|1)"; return false; }
            out = make_set_ack_mode(seq, on); return true;
        }

        // Diagnostics (RO)
        case CommandKind::GET_RSSI_DBM:   out = make_get_rssi(seq); return true;
        case CommandKind::GET_SNR_DB:     out = make_get_snr(seq); return true;
        case CommandKind::GET_VBAT_MV:    out = make_get_vbat(seq); return true;
        case CommandKind::GET_TEMP_C:     out = make_get_temp(seq); return true;
        case CommandKind::GET_FREE_MEM_B: out = make_get_free_mem(seq); return true;
        case CommandKind::GET_FREE_FLASH_B: out = make_get_free_flash(seq); return true;
        case CommandKind::GET_LOG_COUNT:  out = make_get_log_count(seq); return true;

        // Bulk
        case CommandKind::GET_ALL:        out = make_get_all(seq); return true;
    }

    err = "unhandled_command";
    return false;
}

// ---------- public helpers ----------
bool build_legacy_packet(bool get_id,
                         bool ping,
                         const std::string& set_id_value,
                         uint8_t seq,
                         std::vector<uint8_t>& out,
                         std::string& err)
{
    out.clear();
    int count = (get_id?1:0) + (ping?1:0) + (!set_id_value.empty()?1:0);
    if (count != 1) { err = "need_exactly_one_command"; return false; }

    if (get_id)           return build_packet_from_kind(CommandKind::GET_ID, seq, "", out, err);
    if (ping)             return build_packet_from_kind(CommandKind::PING,   seq, "", out, err);
    /* set-id */          return build_packet_from_kind(CommandKind::SET_ID,  seq, set_id_value, out, err);
}

bool build_param_get_packet(const std::string& name, uint8_t seq,
                            std::vector<uint8_t>& out, std::string& err)
{
    out.clear();
    CommandKind kind;
    if (!name_to_kind(name, /*is_set=*/false, kind)) { err="unknown_get"; return false; }
    return build_packet_from_kind(kind, seq, "", out, err);
}

bool build_param_set_packet(const std::string& name, const std::string& value, uint8_t seq,
                            std::vector<uint8_t>& out, std::string& err)
{
    out.clear();
    CommandKind kind;
    if (!name_to_kind(name, /*is_set=*/true, kind)) { err="unknown_set"; return false; }
    return build_packet_from_kind(kind, seq, value, out, err);
}

} // namespace viatext
