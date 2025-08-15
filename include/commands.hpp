#pragma once
/**
 * @file commands.hpp
 * @brief Build ViaText request frames and pretty-decode responses (Linux CLI side).
 *
 * OVERVIEW
 * --------
 * This header exposes *low-level* request builders (make_* functions) that
 * produce the inner payload of a SLIP-framed packet using a tiny TLV schema.
 * It pairs with `command_dispatch.(hpp|cpp)` which maps CLI options to these
 * builders, so **main.cpp never needs to change** when you add commands.
 *
 * WIRE FORMAT (inner payload before SLIP)
 * ---------------------------------------
 *   byte 0: VERB
 *   byte 1: FLAGS (reserved = 0)
 *   byte 2: SEQ   (mirrored back by the node)
 *   byte 3: TLV_LEN N (0..255)
 *   then N bytes of TLVs:
 *       [ TAG (1), LEN (1), VALUE (LEN) ] repeated
 *
 * - Numeric TLVs are little-endian.
 * - String TLVs are UTF-8, max 255 bytes.
 * - Multiple TLVs can be included in one frame.
 *
 * ADDING A NEW PARAM
 * ------------------
 * 1) Pick a new TAG_* value below (keep groups logical).
 * 2) Add make_get_* / make_set_* builders here (+docs for type/range).
 * 3) Teach `command_dispatch` to route names to those builders.
 * 4) Implement the firmware side to recognize the same TAG(s).
 *
 * COMPAT
 * ------
 * - Legacy verbs (GET_ID, SET_ID, PING) are kept for simplicity.
 * - Generic verbs (GET_PARAM/SET_PARAM/GET_ALL) support richer reads/writes.
 */

#include <vector>
#include <string>
#include <cstdint>

namespace viatext {

// =============================== Verbs ===============================
enum : uint8_t {
    // Legacy / simple
    GET_ID    = 0x01,  // no TLV required
    SET_ID    = 0x02,  // TLV: TAG_ID
    PING      = 0x03,  // no TLV

    // Generic parameterized ops
    GET_PARAM = 0x10,  // include one or more TAGs with len=0 to request
    SET_PARAM = 0x11,  // include TAGs with values to write
    GET_ALL   = 0x12,  // node may stream multiple RESP_OK frames

    // Responses
    RESP_OK   = 0x90,
    RESP_ERR  = 0x91
};

// ============================== TLV Tags =============================
// Identity / System
enum : uint8_t {
    TAG_ID          = 0x01, // string: node id (<=31 recommended)
    TAG_ALIAS       = 0x02, // string: friendly name
    TAG_FW_VERSION  = 0x03, // string: semver
    TAG_UPTIME_S    = 0x04, // u32: seconds since boot
    TAG_BOOT_TIME   = 0x05  // u32: unix epoch seconds
};

// Radio (SX1276/78-ish)
enum : uint8_t {
    TAG_FREQ_HZ     = 0x10, // u32: RF frequency in Hz
    TAG_SF          = 0x11, // u8 : spreading factor (7..12)
    TAG_BW_HZ       = 0x12, // u32: bandwidth Hz (e.g. 125000)
    TAG_CR          = 0x13, // u8 : coding rate code (5..8 => 4/5..4/8)
    TAG_TX_PWR_DBM  = 0x14, // i8 : TX power dBm (radio-safe range)
    TAG_CHAN        = 0x15  // u8 : abstract channel index (impl-defined)
};

// Behavior / Routing
enum : uint8_t {
    TAG_MODE        = 0x20, // u8 : 0=relay, 1=direct, 2=gateway (example)
    TAG_HOPS        = 0x21, // u8 : max hops
    TAG_BEACON_SEC  = 0x22, // u32: beacon interval seconds
    TAG_BUF_SIZE    = 0x23, // u16: outbound queue size
    TAG_ACK_MODE    = 0x24  // u8 : 0/1
};

// Diagnostics (read-only)
enum : uint8_t {
    TAG_RSSI_DBM    = 0x30, // i16: last RX RSSI dBm
    TAG_SNR_DB      = 0x31, // i8 : last RX SNR dB
    TAG_VBAT_MV     = 0x32, // u16: supply/battery mV
    TAG_TEMP_C10    = 0x33, // i16: temperature 0.1°C units
    TAG_FREE_MEM    = 0x34, // u32: heap/free mem bytes
    TAG_FREE_FLASH  = 0x35, // u32: free flash bytes
    TAG_LOG_COUNT   = 0x36  // u16: log entries count
};

// ============================ Legacy Builders =========================
/** GET_ID (no TLV) */
std::vector<uint8_t> make_get_id(uint8_t seq);
/** PING (no TLV) */
std::vector<uint8_t> make_ping(uint8_t seq);
/** SET_ID — TLV: TAG_ID (string, <=31 recommended) */
std::vector<uint8_t> make_set_id(uint8_t seq, const std::string& id);

// ======================== Identity / System Builders ==================
/** GET_PARAM: TAG_ALIAS */
std::vector<uint8_t> make_get_alias(uint8_t seq);
/** SET_PARAM: TAG_ALIAS (string) */
std::vector<uint8_t> make_set_alias(uint8_t seq, const std::string& alias);

/** GET_PARAM: TAG_FW_VERSION (string) */
std::vector<uint8_t> make_get_fw_version(uint8_t seq);
/** GET_PARAM: TAG_UPTIME_S (u32) */
std::vector<uint8_t> make_get_uptime(uint8_t seq);
/** GET_PARAM: TAG_BOOT_TIME (u32 epoch) */
std::vector<uint8_t> make_get_boot_time(uint8_t seq);

// ============================ Radio Builders ==========================
/** GET_PARAM: TAG_FREQ_HZ (u32) */
std::vector<uint8_t> make_get_freq(uint8_t seq);
/** SET_PARAM: TAG_FREQ_HZ (u32 Hz) */
std::vector<uint8_t> make_set_freq(uint8_t seq, uint32_t hz);

/** GET_PARAM: TAG_SF (u8 7..12) */
std::vector<uint8_t> make_get_sf(uint8_t seq);
/** SET_PARAM: TAG_SF (u8 7..12) */
std::vector<uint8_t> make_set_sf(uint8_t seq, uint8_t sf);

/** GET_PARAM: TAG_BW_HZ (u32) */
std::vector<uint8_t> make_get_bw(uint8_t seq);
/** SET_PARAM: TAG_BW_HZ (u32 Hz, e.g. 125000) */
std::vector<uint8_t> make_set_bw(uint8_t seq, uint32_t hz);

/** GET_PARAM: TAG_CR (u8 5..8 => 4/5..4/8) */
std::vector<uint8_t> make_get_cr(uint8_t seq);
/** SET_PARAM: TAG_CR (u8 5..8 => 4/5..4/8) */
std::vector<uint8_t> make_set_cr(uint8_t seq, uint8_t cr);

/** GET_PARAM: TAG_TX_PWR_DBM (i8) */
std::vector<uint8_t> make_get_tx_pwr(uint8_t seq);
/** SET_PARAM: TAG_TX_PWR_DBM (i8 dBm, radio safe range) */
std::vector<uint8_t> make_set_tx_pwr(uint8_t seq, int8_t dbm);

/** GET_PARAM: TAG_CHAN (u8) */
std::vector<uint8_t> make_get_chan(uint8_t seq);
/** SET_PARAM: TAG_CHAN (u8) */
std::vector<uint8_t> make_set_chan(uint8_t seq, uint8_t ch);

// =========================== Behavior Builders ========================
/** GET_PARAM: TAG_MODE (u8) */
std::vector<uint8_t> make_get_mode(uint8_t seq);
/** SET_PARAM: TAG_MODE (u8) */
std::vector<uint8_t> make_set_mode(uint8_t seq, uint8_t mode);

/** GET_PARAM: TAG_HOPS (u8) */
std::vector<uint8_t> make_get_hops(uint8_t seq);
/** SET_PARAM: TAG_HOPS (u8) */
std::vector<uint8_t> make_set_hops(uint8_t seq, uint8_t hops);

/** GET_PARAM: TAG_BEACON_SEC (u32) */
std::vector<uint8_t> make_get_beacon(uint8_t seq);
/** SET_PARAM: TAG_BEACON_SEC (u32 seconds) */
std::vector<uint8_t> make_set_beacon(uint8_t seq, uint32_t secs);

/** GET_PARAM: TAG_BUF_SIZE (u16) */
std::vector<uint8_t> make_get_buf_size(uint8_t seq);
/** SET_PARAM: TAG_BUF_SIZE (u16) */
std::vector<uint8_t> make_set_buf_size(uint8_t seq, uint16_t n);

/** GET_PARAM: TAG_ACK_MODE (u8 0/1) */
std::vector<uint8_t> make_get_ack_mode(uint8_t seq);
/** SET_PARAM: TAG_ACK_MODE (u8 0/1) */
std::vector<uint8_t> make_set_ack_mode(uint8_t seq, uint8_t on);

// ======================= Diagnostics (read-only) ======================
/** GET_PARAM: TAG_RSSI_DBM (i16) */
std::vector<uint8_t> make_get_rssi(uint8_t seq);
/** GET_PARAM: TAG_SNR_DB (i8) */
std::vector<uint8_t> make_get_snr(uint8_t seq);
/** GET_PARAM: TAG_VBAT_MV (u16 mV) */
std::vector<uint8_t> make_get_vbat(uint8_t seq);
/** GET_PARAM: TAG_TEMP_C10 (i16 x10°C) */
std::vector<uint8_t> make_get_temp(uint8_t seq);
/** GET_PARAM: TAG_FREE_MEM (u32 bytes) */
std::vector<uint8_t> make_get_free_mem(uint8_t seq);
/** GET_PARAM: TAG_FREE_FLASH (u32 bytes) */
std::vector<uint8_t> make_get_free_flash(uint8_t seq);
/** GET_PARAM: TAG_LOG_COUNT (u16) */
std::vector<uint8_t> make_get_log_count(uint8_t seq);

// ============================= Bulk Read ==============================
/** GET_ALL — node may return multiple RESP_OK frames containing many TLVs. */
std::vector<uint8_t> make_get_all(uint8_t seq);

// =========================== Response Decode ==========================
/**
 * @brief Decode one RESP_* frame into a compact, machine-friendly line.
 * Example: "status=ok seq=1 id=vt-01 freq_hz=915000000 sf=7 ..."
 *
 * NOTE: This is intentionally lossy (human+machine friendly). If you need
 * structured values, decode TLVs in your caller directly.
 */
std::string decode_pretty(const std::vector<uint8_t>& frame);

} // namespace viatext
