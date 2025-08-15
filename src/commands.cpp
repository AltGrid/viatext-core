/**
 * @file commands.cpp
 * @brief Low-level ViaText request builders (host side) + pretty response decode.
 *
 * This pairs with commands.hpp and command_dispatch.*:
 *  - Builders here construct the *inner payload* (verb + TLVs); serial_io wraps it in SLIP.
 *  - Numeric TLVs are LITTLE-ENDIAN; strings are UTF-8 (max 255 bytes).
 *  - Legacy verbs (GET_ID/SET_ID/PING) remain for simple flows.
 *  - Generic verbs (GET_PARAM/SET_PARAM/GET_ALL) support a full parameter model.
 *
 * Notes:
 *  - GET_ALL may yield multiple RESP_OK frames; callers must read until satisfied/timeout.
 *  - `decode_pretty()` yields a compact, human+machine friendly line; for structured data,
 *    parse TLVs directly in your caller.
 */

#include "commands.hpp"
#include <algorithm>
#include <sstream>
#include <iomanip>

namespace viatext {

// ============================================================================
// Low-level helpers
// ============================================================================
static inline std::vector<uint8_t> header(uint8_t verb, uint8_t seq){
    std::vector<uint8_t> b; b.reserve(64);
    b.push_back(verb); b.push_back(0); b.push_back(seq); b.push_back(0); // TLV len placeholder
    return b;
}
static inline void add_tlv_bytes(std::vector<uint8_t>& b, uint8_t tag, const uint8_t* p, uint8_t len){
    b.push_back(tag); b.push_back(len); if (len) b.insert(b.end(), p, p+len);
}
static inline void add_tlv_u8 (std::vector<uint8_t>& b, uint8_t tag, uint8_t v){ uint8_t x[1]={v}; add_tlv_bytes(b,tag,x,1); }
static inline void add_tlv_i8 (std::vector<uint8_t>& b, uint8_t tag, int8_t  v){ uint8_t x[1]={ static_cast<uint8_t>(v) }; add_tlv_bytes(b,tag,x,1); }
static inline void add_tlv_u16(std::vector<uint8_t>& b, uint8_t tag, uint16_t v){ uint8_t x[2]={(uint8_t)(v&0xFF),(uint8_t)(v>>8)}; add_tlv_bytes(b,tag,x,2); }
static inline void add_tlv_i16(std::vector<uint8_t>& b, uint8_t tag, int16_t  v){ add_tlv_u16(b,tag, static_cast<uint16_t>(v)); }
static inline void add_tlv_u32(std::vector<uint8_t>& b, uint8_t tag, uint32_t v){
    uint8_t x[4]={(uint8_t)(v&0xFF),(uint8_t)((v>>8)&0xFF),(uint8_t)((v>>16)&0xFF),(uint8_t)((v>>24)&0xFF)};
    add_tlv_bytes(b,tag,x,4);
}
static inline void add_tlv_str(std::vector<uint8_t>& b, uint8_t tag, const std::string& s){
    const uint8_t L = static_cast<uint8_t>(std::min<size_t>(s.size(), 255));
    add_tlv_bytes(b, tag, reinterpret_cast<const uint8_t*>(s.data()), L);
}
static inline void add_tlv_get(std::vector<uint8_t>& b, uint8_t tag){ add_tlv_bytes(b, tag, nullptr, 0); } // request tag via GET_PARAM
static inline void finalize(std::vector<uint8_t>& b){ b[3] = static_cast<uint8_t>(b.size() - 4); }

// ============================================================================
// Legacy builders (kept so current firmware keeps working)
// ============================================================================
std::vector<uint8_t> make_get_id(uint8_t seq){
    auto b = header(GET_ID, seq); finalize(b); return b;
}
std::vector<uint8_t> make_ping(uint8_t seq){
    auto b = header(PING, seq); finalize(b); return b;
}
std::vector<uint8_t> make_set_id(uint8_t seq, const std::string& id){
    auto b = header(SET_ID, seq);
    add_tlv_str(b, TAG_ID, id);
    finalize(b); return b;
}

// ============================================================================
// ------------------------- Identity / System --------------------------------
// TAG_ID (string)               <-> node persistent ID
// TAG_ALIAS (string)            <-> human alias
// TAG_FW_VERSION (string)       <-> firmware semver
// TAG_UPTIME_S (u32)            <-> seconds since boot
// TAG_BOOT_TIME (u32)           <-> Unix epoch seconds (if RTC present)
// ============================================================================
std::vector<uint8_t> make_get_alias(uint8_t seq){
    auto b = header(GET_PARAM, seq); add_tlv_get(b, TAG_ALIAS); finalize(b); return b;
}
std::vector<uint8_t> make_set_alias(uint8_t seq, const std::string& alias){
    auto b = header(SET_PARAM, seq); add_tlv_str(b, TAG_ALIAS, alias); finalize(b); return b;
}
std::vector<uint8_t> make_get_fw_version(uint8_t seq){
    auto b = header(GET_PARAM, seq); add_tlv_get(b, TAG_FW_VERSION); finalize(b); return b;
}
std::vector<uint8_t> make_get_uptime(uint8_t seq){
    auto b = header(GET_PARAM, seq); add_tlv_get(b, TAG_UPTIME_S); finalize(b); return b;
}
std::vector<uint8_t> make_get_boot_time(uint8_t seq){
    auto b = header(GET_PARAM, seq); add_tlv_get(b, TAG_BOOT_TIME); finalize(b); return b;
}

// ============================================================================
// ------------------------------ Radio --------------------------------------
// TAG_FREQ_HZ (u32)             <-> radio frequency in Hz (e.g., 915000000)
// TAG_SF (u8)                    <-> spreading factor (7..12)
// TAG_BW_HZ (u32)                <-> bandwidth in Hz (e.g., 125000)
// TAG_CR (u8)                    <-> coding rate code 5..8 => 4/5..4/8
// TAG_TX_PWR_DBM (i8)            <-> TX power in dBm
// TAG_CHAN (u8)                  <-> abstract channel index
// ============================================================================
std::vector<uint8_t> make_get_freq(uint8_t seq){
    auto b = header(GET_PARAM, seq); add_tlv_get(b, TAG_FREQ_HZ); finalize(b); return b;
}
std::vector<uint8_t> make_set_freq(uint8_t seq, uint32_t hz){
    auto b = header(SET_PARAM, seq); add_tlv_u32(b, TAG_FREQ_HZ, hz); finalize(b); return b;
}
std::vector<uint8_t> make_get_sf(uint8_t seq){
    auto b = header(GET_PARAM, seq); add_tlv_get(b, TAG_SF); finalize(b); return b;
}
std::vector<uint8_t> make_set_sf(uint8_t seq, uint8_t sf){
    auto b = header(SET_PARAM, seq); add_tlv_u8(b, TAG_SF, sf); finalize(b); return b;
}
std::vector<uint8_t> make_get_bw(uint8_t seq){
    auto b = header(GET_PARAM, seq); add_tlv_get(b, TAG_BW_HZ); finalize(b); return b;
}
std::vector<uint8_t> make_set_bw(uint8_t seq, uint32_t hz){
    auto b = header(SET_PARAM, seq); add_tlv_u32(b, TAG_BW_HZ, hz); finalize(b); return b;
}
std::vector<uint8_t> make_get_cr(uint8_t seq){
    auto b = header(GET_PARAM, seq); add_tlv_get(b, TAG_CR); finalize(b); return b;
}
std::vector<uint8_t> make_set_cr(uint8_t seq, uint8_t cr){
    auto b = header(SET_PARAM, seq); add_tlv_u8(b, TAG_CR, cr); finalize(b); return b;
}
std::vector<uint8_t> make_get_tx_pwr(uint8_t seq){
    auto b = header(GET_PARAM, seq); add_tlv_get(b, TAG_TX_PWR_DBM); finalize(b); return b;
}
std::vector<uint8_t> make_set_tx_pwr(uint8_t seq, int8_t dbm){
    auto b = header(SET_PARAM, seq); add_tlv_i8(b, TAG_TX_PWR_DBM, dbm); finalize(b); return b;
}
std::vector<uint8_t> make_get_chan(uint8_t seq){
    auto b = header(GET_PARAM, seq); add_tlv_get(b, TAG_CHAN); finalize(b); return b;
}
std::vector<uint8_t> make_set_chan(uint8_t seq, uint8_t ch){
    auto b = header(SET_PARAM, seq); add_tlv_u8(b, TAG_CHAN, ch); finalize(b); return b;
}

// ============================================================================
// ------------------------------ Behavior -----------------------------------
// TAG_MODE (u8)                  <-> 0=relay,1=direct,2=gateway (example)
// TAG_HOPS (u8)                  <-> max hops
// TAG_BEACON_SEC (u32)           <-> beacon interval seconds
// TAG_BUF_SIZE (u16)             <-> outbound queue size
// TAG_ACK_MODE (u8)              <-> 0/1
// ============================================================================
std::vector<uint8_t> make_get_mode(uint8_t seq){
    auto b = header(GET_PARAM, seq); add_tlv_get(b, TAG_MODE); finalize(b); return b;
}
std::vector<uint8_t> make_set_mode(uint8_t seq, uint8_t mode){
    auto b = header(SET_PARAM, seq); add_tlv_u8(b, TAG_MODE, mode); finalize(b); return b;
}
std::vector<uint8_t> make_get_hops(uint8_t seq){
    auto b = header(GET_PARAM, seq); add_tlv_get(b, TAG_HOPS); finalize(b); return b;
}
std::vector<uint8_t> make_set_hops(uint8_t seq, uint8_t hops){
    auto b = header(SET_PARAM, seq); add_tlv_u8(b, TAG_HOPS, hops); finalize(b); return b;
}
std::vector<uint8_t> make_get_beacon(uint8_t seq){
    auto b = header(GET_PARAM, seq); add_tlv_get(b, TAG_BEACON_SEC); finalize(b); return b;
}
std::vector<uint8_t> make_set_beacon(uint8_t seq, uint32_t secs){
    auto b = header(SET_PARAM, seq); add_tlv_u32(b, TAG_BEACON_SEC, secs); finalize(b); return b;
}
std::vector<uint8_t> make_get_buf_size(uint8_t seq){
    auto b = header(GET_PARAM, seq); add_tlv_get(b, TAG_BUF_SIZE); finalize(b); return b;
}
std::vector<uint8_t> make_set_buf_size(uint8_t seq, uint16_t n){
    auto b = header(SET_PARAM, seq); add_tlv_u16(b, TAG_BUF_SIZE, n); finalize(b); return b;
}
std::vector<uint8_t> make_get_ack_mode(uint8_t seq){
    auto b = header(GET_PARAM, seq); add_tlv_get(b, TAG_ACK_MODE); finalize(b); return b;
}
std::vector<uint8_t> make_set_ack_mode(uint8_t seq, uint8_t on){
    auto b = header(SET_PARAM, seq); add_tlv_u8(b, TAG_ACK_MODE, on?1:0); finalize(b); return b;
}

// ============================================================================
// ------------------------------ Diagnostics --------------------------------
// TAG_RSSI_DBM (i16)            <-> last RX RSSI
// TAG_SNR_DB (i8)               <-> last RX SNR
// TAG_VBAT_MV (u16)             <-> supply millivolts
// TAG_TEMP_C10 (i16)            <-> temp * 10 C
// TAG_FREE_MEM (u32)            <-> bytes
// TAG_FREE_FLASH (u32)          <-> bytes
// TAG_LOG_COUNT (u16)           <-> entries
// ============================================================================
std::vector<uint8_t> make_get_rssi(uint8_t seq){
    auto b = header(GET_PARAM, seq); add_tlv_get(b, TAG_RSSI_DBM); finalize(b); return b;
}
std::vector<uint8_t> make_get_snr(uint8_t seq){
    auto b = header(GET_PARAM, seq); add_tlv_get(b, TAG_SNR_DB); finalize(b); return b;
}
std::vector<uint8_t> make_get_vbat(uint8_t seq){
    auto b = header(GET_PARAM, seq); add_tlv_get(b, TAG_VBAT_MV); finalize(b); return b;
}
std::vector<uint8_t> make_get_temp(uint8_t seq){
    auto b = header(GET_PARAM, seq); add_tlv_get(b, TAG_TEMP_C10); finalize(b); return b;
}
std::vector<uint8_t> make_get_free_mem(uint8_t seq){
    auto b = header(GET_PARAM, seq); add_tlv_get(b, TAG_FREE_MEM); finalize(b); return b;
}
std::vector<uint8_t> make_get_free_flash(uint8_t seq){
    auto b = header(GET_PARAM, seq); add_tlv_get(b, TAG_FREE_FLASH); finalize(b); return b;
}
std::vector<uint8_t> make_get_log_count(uint8_t seq){
    auto b = header(GET_PARAM, seq); add_tlv_get(b, TAG_LOG_COUNT); finalize(b); return b;
}

// ============================================================================
// Optional: GET_ALL (node may stream multiple frames; CLI must read many)
// ============================================================================
std::vector<uint8_t> make_get_all(uint8_t seq){
    auto b = header(GET_ALL, seq); finalize(b); return b;
}

// ============================================================================
// Response decoding
// ============================================================================
struct Tlv { uint8_t tag; std::string val; }; // value is raw bytes; may include '\0'

static std::vector<Tlv> parse_tlvs(const std::vector<uint8_t>& f){
    std::vector<Tlv> tv;
    if (f.size() < 4) return tv;
    const size_t tl = f[3];
    size_t off = 4, end = std::min(f.size(), size_t(4 + tl));
    while (off + 2 <= end){
        uint8_t tag = f[off++], len = f[off++];
        if (off + len > end) break;
        tv.push_back({tag, std::string(reinterpret_cast<const char*>(&f[off]),
                                       reinterpret_cast<const char*>(&f[off + len]))});
        off += len;
    }
    return tv;
}

// Safe byte→int helpers (LE)
static inline bool as_u8 (const std::string& s, uint8_t&  out){ if (s.size()!=1) return false; out=static_cast<uint8_t>(static_cast<unsigned char>(s[0])); return true; }
static inline bool as_i8 (const std::string& s, int8_t&   out){ if (s.size()!=1) return false; out=static_cast<int8_t>(static_cast<signed char>(s[0])); return true; }
static inline bool as_u16(const std::string& s, uint16_t& out){
    if (s.size()!=2) return false;
    out = static_cast<uint16_t>( static_cast<uint16_t>(static_cast<uint8_t>(s[0])) |
                                (static_cast<uint16_t>(static_cast<uint8_t>(s[1])) << 8) );
    return true;
}
static inline bool as_i16(const std::string& s, int16_t&  out){ uint16_t u; if(!as_u16(s,u)) return false; out=static_cast<int16_t>(u); return true; }
static inline bool as_u32(const std::string& s, uint32_t& out){
    if (s.size()!=4) return false;
    out =  static_cast<uint32_t>(static_cast<uint8_t>(s[0])) |
          (static_cast<uint32_t>(static_cast<uint8_t>(s[1])) << 8) |
          (static_cast<uint32_t>(static_cast<uint8_t>(s[2])) << 16) |
          (static_cast<uint32_t>(static_cast<uint8_t>(s[3])) << 24);
    return true;
}

std::string decode_pretty(const std::vector<uint8_t>& f){
    std::ostringstream os;
    if (f.size() < 4) { os << "status=error reason=bad_frame"; return os.str(); }
    uint8_t verb  = f[0];
    // uint8_t flags = f[1]; // reserved
    uint8_t seq   = f[2];

    if (verb == RESP_OK)      os << "status=ok";
    else if (verb == RESP_ERR)os << "status=error";
    else                      os << "status=unknown";

    os << " seq=" << unsigned(seq);

    for (auto& t : parse_tlvs(f)){
        switch (t.tag) {
            // Identity/System
            case TAG_ID:         os << " id=" << t.val; break;
            case TAG_ALIAS:      os << " alias=" << t.val; break;
            case TAG_FW_VERSION: os << " fw=" << t.val; break;
            case TAG_UPTIME_S: { uint32_t v; if (as_u32(t.val,v)) os << " uptime_s=" << v; break; }
            case TAG_BOOT_TIME:{ uint32_t v; if (as_u32(t.val,v)) os << " boot_time=" << v; break; }

            // Radio
            case TAG_FREQ_HZ:   { uint32_t v; if (as_u32(t.val,v)) os << " freq_hz=" << v; break; }
            case TAG_SF:        { uint8_t  v; if (as_u8 (t.val,v)) os << " sf=" << unsigned(v); break; }
            case TAG_BW_HZ:     { uint32_t v; if (as_u32(t.val,v)) os << " bw_hz=" << v; break; }
            case TAG_CR:        { uint8_t  v; if (as_u8 (t.val,v)) os << " cr=4/" << unsigned(v); break; }
            case TAG_TX_PWR_DBM:{ int8_t   v; if (as_i8 (t.val,v)) os << " tx_pwr_dbm=" << int(v); break; }
            case TAG_CHAN:      { uint8_t  v; if (as_u8 (t.val,v)) os << " chan=" << unsigned(v); break; }

            // Behavior
            case TAG_MODE:      { uint8_t  v; if (as_u8 (t.val,v)) os << " mode=" << unsigned(v); break; }
            case TAG_HOPS:      { uint8_t  v; if (as_u8 (t.val,v)) os << " hops=" << unsigned(v); break; }
            case TAG_BEACON_SEC:{ uint32_t v; if (as_u32(t.val,v)) os << " beacon_s=" << v; break; }
            case TAG_BUF_SIZE:  { uint16_t v; if (as_u16(t.val,v)) os << " buf_size=" << v; break; }
            case TAG_ACK_MODE:  { uint8_t  v; if (as_u8 (t.val,v)) os << " ack=" << unsigned(v); break; }

            // Diagnostics
            case TAG_RSSI_DBM:  { int16_t  v; if (as_i16(t.val,v)) os << " rssi_dbm=" << v; break; }
            case TAG_SNR_DB:    { int8_t   v; if (as_i8 (t.val,v)) os << " snr_db=" << int(v); break; }
            case TAG_VBAT_MV:   { uint16_t v; if (as_u16(t.val,v)) os << " vbat_mv=" << v; break; }
            case TAG_TEMP_C10:  { int16_t  v; if (as_i16(t.val,v)) os << " temp_c=" << (v/10.0); break; }
            case TAG_FREE_MEM:  { uint32_t v; if (as_u32(t.val,v)) os << " free_mem=" << v; break; }
            case TAG_FREE_FLASH:{ uint32_t v; if (as_u32(t.val,v)) os << " free_flash=" << v; break; }
            case TAG_LOG_COUNT: { uint16_t v; if (as_u16(t.val,v)) os << " log_count=" << v; break; }

            default: {
                os << " tag" << unsigned(t.tag) << "=0x";
                std::ios_base::fmtflags f0 = os.flags();
                char fill0 = os.fill();
                for (unsigned char c : t.val)
                    os << std::hex << std::setw(2) << std::setfill('0') << (unsigned)c;
                os.flags(f0); os.fill(fill0);
                break;
            }
        }
    }
    return os.str();
}

} // namespace viatext
