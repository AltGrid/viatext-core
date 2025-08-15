/**
 * @file commands.hpp
 * @brief Build ViaText request frames and pretty-decode responses.
 *
 * Frame format (host -> node and node -> host):
 *   byte 0: verb
 *   byte 1: flags (reserved = 0)
 *   byte 2: seq  (mirrored back by node)
 *   byte 3: TLV total length N (0..255)
 *   then N bytes of TLVs:
 *       [ tag (1), len (1), value (len) ] repeated
 *
 * Endianness for numeric TLVs: little-endian.
 */
#pragma once
#include <vector>
#include <string>
#include <cstdint>

namespace viatext {

// ---------------- Verbs (existing + generic) ----------------
enum : uint8_t {
    GET_ID    = 0x01,  // legacy: query id (no TLV needed)
    SET_ID    = 0x02,  // legacy: set id (TLV TAG_ID)
    PING      = 0x03,

    GET_PARAM = 0x10,  // generic: include one or more TAG_* with len=0 to request
    SET_PARAM = 0x11,  // generic: include TAG_* with value to set
    GET_ALL   = 0x12,  // optional: node may stream multiple RESP_OK frames
    RESP_OK   = 0x90,
    RESP_ERR  = 0x91
};

// ---------------- TLV Tags ----------------
// Identity / System
enum : uint8_t {
    TAG_ID          = 0x01, // string: node id (short, <=31)
    TAG_ALIAS       = 0x02, // string: human name
    TAG_FW_VERSION  = 0x03, // string: semver
    TAG_UPTIME_S    = 0x04, // u32: seconds
    TAG_BOOT_TIME   = 0x05  // u32: unix epoch seconds
};

// Radio (SX1276/78)
enum : uint8_t {
    TAG_FREQ_HZ     = 0x10, // u32: frequency in Hz
    TAG_SF          = 0x11, // u8 : spreading factor (7..12)
    TAG_BW_HZ       = 0x12, // u32: bandwidth in Hz (e.g. 125000)
    TAG_CR          = 0x13, // u8 : coding rate code (5..8 => 4/5..4/8)
    TAG_TX_PWR_DBM  = 0x14, // i8 : TX power in dBm
    TAG_CHAN        = 0x15  // u8 : abstract channel index (implementation-defined)
};

// Behavior
enum : uint8_t {
    TAG_MODE        = 0x20, // u8 : 0=relay,1=direct,2=gateway (example)
    TAG_HOPS        = 0x21, // u8 : max hops
    TAG_BEACON_SEC  = 0x22, // u32: beacon interval seconds
    TAG_BUF_SIZE    = 0x23, // u16: outbound queue size
    TAG_ACK_MODE    = 0x24  // u8 : 0/1
};

// Diagnostics (read-only)
enum : uint8_t {
    TAG_RSSI_DBM    = 0x30, // i16: last RX RSSI dBm
    TAG_SNR_DB      = 0x31, // i8 : last RX SNR dB
    TAG_VBAT_MV     = 0x32, // u16: battery/usb millivolts
    TAG_TEMP_C10    = 0x33, // i16: temperature x10 C
    TAG_FREE_MEM    = 0x34, // u32: bytes
    TAG_FREE_FLASH  = 0x35, // u32: bytes
    TAG_LOG_COUNT   = 0x36  // u16: entries
};

// ---------------- Builders (legacy, kept for compatibility) ----------------
std::vector<uint8_t> make_get_id(uint8_t seq);
std::vector<uint8_t> make_ping(uint8_t seq);
std::vector<uint8_t> make_set_id(uint8_t seq, const std::string& id);

// ---------------- Builders: Identity/System ----------------
// get_id is legacy above
std::vector<uint8_t> make_get_alias(uint8_t seq);
std::vector<uint8_t> make_set_alias(uint8_t seq, const std::string& alias);

std::vector<uint8_t> make_get_fw_version(uint8_t seq);   // string
std::vector<uint8_t> make_get_uptime(uint8_t seq);       // u32 seconds
std::vector<uint8_t> make_get_boot_time(uint8_t seq);    // u32 epoch

// ---------------- Builders: Radio ----------------
std::vector<uint8_t> make_get_freq(uint8_t seq);
std::vector<uint8_t> make_set_freq(uint8_t seq, uint32_t hz);

std::vector<uint8_t> make_get_sf(uint8_t seq);
std::vector<uint8_t> make_set_sf(uint8_t seq, uint8_t sf); // 7..12

std::vector<uint8_t> make_get_bw(uint8_t seq);
std::vector<uint8_t> make_set_bw(uint8_t seq, uint32_t hz);

std::vector<uint8_t> make_get_cr(uint8_t seq);
std::vector<uint8_t> make_set_cr(uint8_t seq, uint8_t cr); // 5..8

std::vector<uint8_t> make_get_tx_pwr(uint8_t seq);
std::vector<uint8_t> make_set_tx_pwr(uint8_t seq, int8_t dbm);

std::vector<uint8_t> make_get_chan(uint8_t seq);
std::vector<uint8_t> make_set_chan(uint8_t seq, uint8_t ch);

// ---------------- Builders: Behavior ----------------
std::vector<uint8_t> make_get_mode(uint8_t seq);                // u8
std::vector<uint8_t> make_set_mode(uint8_t seq, uint8_t mode);  // enum

std::vector<uint8_t> make_get_hops(uint8_t seq);
std::vector<uint8_t> make_set_hops(uint8_t seq, uint8_t hops);

std::vector<uint8_t> make_get_beacon(uint8_t seq);
std::vector<uint8_t> make_set_beacon(uint8_t seq, uint32_t secs);

std::vector<uint8_t> make_get_buf_size(uint8_t seq);
std::vector<uint8_t> make_set_buf_size(uint8_t seq, uint16_t n);

std::vector<uint8_t> make_get_ack_mode(uint8_t seq);
std::vector<uint8_t> make_set_ack_mode(uint8_t seq, uint8_t on);

// ---------------- Builders: Diagnostics (read-only) ----------------
std::vector<uint8_t> make_get_rssi(uint8_t seq);       // i16
std::vector<uint8_t> make_get_snr(uint8_t seq);        // i8
std::vector<uint8_t> make_get_vbat(uint8_t seq);       // u16 mV
std::vector<uint8_t> make_get_temp(uint8_t seq);       // i16 x10C
std::vector<uint8_t> make_get_free_mem(uint8_t seq);   // u32
std::vector<uint8_t> make_get_free_flash(uint8_t seq); // u32
std::vector<uint8_t> make_get_log_count(uint8_t seq);  // u16

// ---------------- Optional: GET_ALL (node may stream multiple frames) ------
std::vector<uint8_t> make_get_all(uint8_t seq);

// ---------------- Response decoding ----------------
// Decodes RESP_* frames to a single human-readable line,
// e.g. "status=ok seq=1 id=vt-01 freq_hz=915000000 sf=7 ..."
std::string decode_pretty(const std::vector<uint8_t>& frame);

} // namespace viatext
