/**
 * @page vt-commands ViaText Commands Layer
 * @file commands.hpp
 * @brief Request builders and response decoding — the Linux-centric control window into a ViaText node.
 * @see docs/commands.md for detailed explanation on command use.
 * @details
 * PURPOSE
 * -------
 * The commands layer is the *contract* between a Linux host and a ViaText microcontroller/LoRa node.
 * It defines how requests are constructed, how replies are interpreted, and how human operators
 * (through the CLI) can manipulate the node as if it were just another Linux process.
 *
 * At its core, this file lets the host:
 *   - Build **requests** (verb + TLVs) to query or configure a node.
 *   - Issue those requests over a raw serial link, framed by SLIP (see slip.hpp).
 *   - Decode **responses** into compact, shell-friendly lines or structured TLVs.
 *
 * Without this layer, the serial I/O channel is just a byte pipe. With it, ViaText gains a
 * stable, extensible protocol where radios, IDs, diagnostics, and behavior can be managed
 * consistently from Linux tools.
 *
 * RELATIONSHIP TO OTHER FILES
 * ---------------------------
 * - **serial_io.hpp/cpp**: Handles the physical TTY; writes/reads SLIP frames.
 *   Commands.hpp builds the *inside* of those frames (verbs + TLVs).
 * - **slip.hpp**: Provides framing and boundaries. Commands.hpp never worries about framing;
 *   it just fills a vector of bytes that gets wrapped in SLIP.
 * - **node_registry.hpp/cpp**: Uses `make_get_id()` and `decode_pretty()` to probe devices
 *   and populate the system-wide registry of online nodes.
 * - **CLI / Station programs**: The user interface layer that calls these builders to perform
 *   actions like `--set-alias`, `--ping`, or `--get-freq`.
 *
 * SYSTEM DESIGN
 * -------------
 * - **Verbs**: High-level operations like GET_PARAM, SET_PARAM, GET_ALL.
 *   Legacy verbs (PING, GET_ID, SET_ID) remain to support fast checks.
 * - **TLVs**: Type-Length-Value records appended to requests and responses. Each TLV has:
 *     * tag (1 byte): identifies the parameter (e.g., TAG_FREQ_HZ).
 *     * len (1 byte): size of the following data.
 *     * value (n bytes): UTF-8 string or little-endian integer.
 * - **Response decoding**: Nodes reply with RESP_OK/RESP_ERR plus TLVs.
 *   decode_pretty() renders a one-line `key=value` summary useful for logging and CLI output.
 *
 * LINUX-CENTRIC VIEW
 * ------------------
 * This is deliberately written for Linux operators. Everything is:
 *   - **Human-readable**: requests are just bytes; responses are easy to dump as text.
 *   - **Scriptable**: outputs like `status=ok id=N3 freq_hz=915000000` can be grepped, piped,
 *     or parsed by shell scripts.
 *   - **Portable**: no dependencies beyond the STL; works on Debian, Fedora, Arch, Alpine.
 *
 * CORE NATURE
 * -----------
 * This layer is the **window**: from Linux into the microcontroller.
 *   - When you run `viatext-cli --ping`, the CLI calls `make_ping()`.
 *   - serial_io writes it to the device, read_frame returns the reply.
 *   - decode_pretty() prints `status=ok seq=1`.
 *
 * The user never sees TLV bytes or escape codes. They see a clean, minimal interface
 * where Linux commands map directly to embedded parameters. This isolates complexity:
 *   - Microcontroller firmware can evolve parameters freely as long as tags remain stable.
 *   - Linux tools can issue queries/sets without knowing firmware internals.
 *
 * TRADE-OFFS
 * ----------
 * - Compactness over richness: no JSON, no Protobuf, no schema registry. Just TLVs.
 * - Explicitness over convenience: every builder function is hand-written, so it’s obvious
 *   which tags and verbs are supported.
 * - Resilience over speed: GET_ALL may stream many frames; host must loop until timeout.
 *
 * FIELD UTILITY
 * -------------
 * - Debugging: A single CLI call can dump a node’s ID, alias, firmware version, radio params,
 *   and diagnostics, then exit. Perfect for field checks.
 * - Configuration: Scripts can set aliases, frequencies, or power levels quickly and repeatably.
 * - Diagnostics: RSSI, SNR, battery voltage, free memory, and temperature are accessible
 *   via small, consistent queries.
 *
 * EXAMPLE FLOW
 * ------------
 *   Request: make_get_freq(5) → bytes [verb=GET_PARAM, seq=5, TLV(tag=FREQ_HZ)]
 *   Framed: serial_io::write_frame(fd, request)
 *   Node: replies with RESP_OK seq=5 TLV(tag=FREQ_HZ, val=915000000)
 *   Host: decode_pretty() → "status=ok seq=5 freq_hz=915000000"
 *
 * This cycle repeats for every parameter or command, forming the backbone of all higher-level
 * ViaText tools. Without it, the mesh has no entry point from Linux.
 *
 * MAINTENANCE
 * -----------
 * - Keep builders explicit and small; don’t collapse into generic meta-builders.
 * - Add new tags cautiously; once defined, tags are part of the wire contract.
 * - Ensure decode_pretty() always prints something stable and parseable by scripts.
 *
 * @author Leo
 * @author ChatGPT
 */

#include <vector>
#include <string>
#include <cstdint>

namespace viatext {
// =============================== Verbs ===============================
/**
 * @name Protocol Verbs
 * @brief One-byte opcodes that define what a frame is asking the node to do.
 *
 * These are placed at the start of every outbound request or inbound response.
 * Think of them as the "verb" in a sentence. Most user-facing operations use
 * the parameterized forms (GET_PARAM/SET_PARAM/GET_ALL). Legacy verbs exist
 * for quick checks and backwards compatibility.
 *
 * How it’s used on the wire:
 *   - Requests start with one of: GET_ID, SET_ID, PING, GET_PARAM, SET_PARAM, GET_ALL.
 *   - Responses start with RESP_OK or RESP_ERR, followed by TLVs with details.
 *
 * Notes for beginners:
 *   - TLV means Type-Length-Value: a compact way to ship small fields.
 *   - "No TLV required" means the verb alone is enough; the node knows what to do.
 */
enum : uint8_t {
    GET_ID    = 0x01,  /**< Legacy: ask the node for its ID string (no TLV). */
    SET_ID    = 0x02,  /**< Legacy: set the node ID; include TLV TAG_ID with the new string. */
    PING      = 0x03,  /**< Legacy: round-trip sanity check (no TLV). */

    GET_PARAM = 0x10,  /**< Parameter read: include one or more TAGs with len=0 to request their values. */
    SET_PARAM = 0x11,  /**< Parameter write: include TAGs with value bytes to set new values. */
    GET_ALL   = 0x12,  /**< Snapshot read: node may stream multiple RESP_OK frames with many TLVs. */

    RESP_OK   = 0x90,  /**< Response: the request succeeded; TLVs carry results. */
    RESP_ERR  = 0x91   /**< Response: the request failed; TLVs may include error info. */
};


// ============================== TLV Tags =============================
/**
 * @name TLV Tags: Identity / System
 * @brief Tags that label identity and system-level fields.
 *
 * Each tag appears inside a TLV record after a verb. For GET_PARAM requests,
 * you send the tag with length=0 to ask for it. For SET_PARAM, you send the
 * tag with a length and the new value bytes.
 */
enum : uint8_t {
    TAG_ID          = 0x01, /**< string: node id (ASCII/UTF-8; <= 31 chars recommended). */
    TAG_ALIAS       = 0x02, /**< string: human-friendly name for the node. */
    TAG_FW_VERSION  = 0x03, /**< string: firmware version (e.g., "1.2.0"). */
    TAG_UPTIME_S    = 0x04, /**< u32: seconds since last boot. */
    TAG_BOOT_TIME   = 0x05  /**< u32: Unix epoch seconds when the node booted. */
};


/**
 * @name TLV Tags: Radio (SX1276/78-ish)
 * @brief Tags that control the LoRa PHY parameters.
 *
 * These map to common LoRa settings. Values are validated on the host before
 * they reach the radio. Always follow your local RF regulations.
 */
enum : uint8_t {
    TAG_FREQ_HZ     = 0x10, /**< u32: RF frequency in Hz (e.g., 915000000). */
    TAG_SF          = 0x11, /**< u8 : spreading factor (7..12). Higher = farther but slower. */
    TAG_BW_HZ       = 0x12, /**< u32: bandwidth in Hz (e.g., 125000, 250000, 500000). */
    TAG_CR          = 0x13, /**< u8 : coding rate denominator (5..8 => 4/5..4/8). */
    TAG_TX_PWR_DBM  = 0x14, /**< i8 : transmit power in dBm (radio-safe range). */
    TAG_CHAN        = 0x15  /**< u8 : logical channel index (implementation-defined plan). */
};


/**
 * @name TLV Tags: Behavior / Routing
 * @brief Tags that affect node behavior and mesh routing policies.
 *
 * Use these to set how the node forwards packets, how often it beacons,
 * and how large its buffers are.
 */
enum : uint8_t {
    TAG_MODE        = 0x20, /**< u8 : operating mode (example: 0=relay, 1=direct, 2=gateway). */
    TAG_HOPS        = 0x21, /**< u8 : maximum hop count (TTL) for relayed packets. */
    TAG_BEACON_SEC  = 0x22, /**< u32: beacon interval in seconds. */
    TAG_BUF_SIZE    = 0x23, /**< u16: outbound queue size (implementation-defined units). */
    TAG_ACK_MODE    = 0x24  /**< u8 : acknowledgment mode (0=off, 1=on). */
};


/**
 * @name TLV Tags: Diagnostics (read-only)
 * @brief Tags that report health and environment metrics from the node.
 *
 * These are probe-only; they are meant for monitoring and troubleshooting.
 * Use GET_PARAM with len=0 to request them. Nodes will not accept writes.
 */
enum : uint8_t {
    TAG_RSSI_DBM    = 0x30, /**< i16: last received RSSI in dBm. */
    TAG_SNR_DB      = 0x31, /**< i8 : last received SNR in dB. */
    TAG_VBAT_MV     = 0x32, /**< u16: supply/battery voltage in millivolts. */
    TAG_TEMP_C10    = 0x33, /**< i16: temperature in 0.1 C units (e.g., 253 -> 25.3 C). */
    TAG_FREE_MEM    = 0x34, /**< u32: free heap memory in bytes. */
    TAG_FREE_FLASH  = 0x35, /**< u32: free flash storage in bytes. */
    TAG_LOG_COUNT   = 0x36  /**< u16: number of stored log entries. */
};


// ========================= Convenience Builders =========================
/**
 * @brief Build a GET_ID request frame (no TLVs).
 *
 * Purpose:
 *   Quick way to learn who you are talking to. Useful for discovery scripts,
 *   sanity checks, and inventory tools. On success the node will reply with
 *   RESP_OK and a TLV carrying TAG_ID.
 *
 * Wire shape:
 *   [verb=GET_ID, seq]
 *   No TLVs are attached to the request.
 *
 * Typical usage (pseudo-shell):
 *   req = make_get_id(1)
 *   write_frame(fd, req)
 *   resp = read_frame(fd)
 *   // decode_pretty(resp) -> "status=ok seq=1 id=N3"
 *
 * @param seq   Host-chosen sequence number used to correlate reply frames.
 * @return      Byte vector containing the serialized request.
 *
 * @see make_set_id
 * @see decode_pretty
 */
std::vector<uint8_t> make_get_id(uint8_t seq);


/**
 * @brief Build a PING request frame (no TLVs).
 *
 * Purpose:
 *   Minimal round-trip check. Confirms link, framing, and that the node is alive.
 *   The node should answer with RESP_OK including the echoed sequence.
 *
 * Wire shape:
 *   [verb=PING, seq]
 *
 * Field note:
 *   Use this first when standing up a new serial link; if PING fails, higher-level
 *   parameter traffic will not be reliable.
 *
 * @param seq   Host-chosen sequence number used to correlate reply frames.
 * @return      Byte vector containing the serialized request.
 */
std::vector<uint8_t> make_ping(uint8_t seq);


/**
 * @brief Build a SET_ID request frame with TAG_ID (string).
 *
 * Purpose:
 *   Assign or change the node’s identifier string. This is the human/stable
 *   name used by tooling and logs (e.g., "vt-01", "relay-west"). Keep it short
 *   for radio efficiency; 31 characters or fewer is recommended.
 *
 * Wire shape:
 *   [verb=SET_ID, seq, TLV(tag=TAG_ID, len=N, val=<id bytes>)]
 *
 * Safety:
 *   Changing IDs can confuse scripts that cache names; plan a short maintenance
 *   window if this runs in production meshes.
 *
 * @param seq   Host-chosen sequence number used to correlate reply frames.
 * @param id    New node ID as UTF-8/ASCII (recommended <= 31 chars).
 * @return      Byte vector containing the serialized request.
 *
 * @see make_get_id
 * @see decode_pretty
 */
std::vector<uint8_t> make_set_id(uint8_t seq, const std::string& id);


// ======================== Identity / System Builders ==================

/**
 * @brief Build a packet asking the node for its human-readable alias.
 *
 * Each node can advertise a short "alias" string (friendly name). 
 * This request retrieves the current alias from the target node.
 *
 * @param seq Sequence number to track the request/response pair.
 * @return Encoded packet ready for transmission.
 */
std::vector<uint8_t> make_get_alias(uint8_t seq);

/**
 * @brief Build a packet to set the node's human-readable alias.
 *
 * Sends a new alias string to the target node, replacing the existing one. 
 * Useful for giving devices easy-to-recognize names in the mesh.
 *
 * @param seq Sequence number to track the request/response pair.
 * @param alias New alias string to assign (e.g., "HckrMn").
 * @return Encoded packet ready for transmission.
 */
std::vector<uint8_t> make_set_alias(uint8_t seq, const std::string& alias);

/**
 * @brief Build a packet asking the node for its firmware version.
 *
 * Returns a string that identifies the firmware currently running 
 * on the node (e.g., "v1.2.3"). Helps with diagnostics and ensuring 
 * nodes are on compatible software.
 *
 * @param seq Sequence number to track the request/response pair.
 * @return Encoded packet ready for transmission.
 */
std::vector<uint8_t> make_get_fw_version(uint8_t seq);

/**
 * @brief Build a packet asking the node for its uptime in seconds.
 *
 * Uptime is the total number of seconds since the node was last powered on 
 * or reset. This is useful for monitoring stability and runtime behavior.
 *
 * @param seq Sequence number to track the request/response pair.
 * @return Encoded packet ready for transmission.
 */
std::vector<uint8_t> make_get_uptime(uint8_t seq);

/**
 * @brief Build a packet asking the node for its boot time (epoch seconds).
 *
 * Boot time is reported as a UNIX timestamp (seconds since Jan 1, 1970 UTC). 
 * This allows correlating the node’s startup event with real-world time.
 *
 * @param seq Sequence number to track the request/response pair.
 * @return Encoded packet ready for transmission.
 */
std::vector<uint8_t> make_get_boot_time(uint8_t seq);

// ============================ Radio Builders ==========================

/**
 * @brief Build a packet asking the node for its RF center frequency.
 *
 * The node will respond with its current operating frequency in Hz.
 * Typical values depend on regional bandplans (e.g., 868e6, 915e6).
 *
 * @param seq Sequence number for request/response correlation.
 * @return Encoded packet ready for transmission.
 */
std::vector<uint8_t> make_get_freq(uint8_t seq);

/**
 * @brief Build a packet to set the RF center frequency.
 *
 * Updates the node’s transmit/receive frequency. Host-side validation
 * is recommended to ensure the value complies with regional regulations.
 *
 * @param seq Sequence number for request/response correlation.
 * @param hz  Frequency in Hz (e.g., 915000000).
 * @return Encoded packet ready for transmission.
 */
std::vector<uint8_t> make_set_freq(uint8_t seq, uint32_t hz);

/**
 * @brief Build a packet asking the node for its LoRa spreading factor.
 *
 * The spreading factor (SF7–SF12) controls range vs. data rate. Higher
 * values increase sensitivity and range, but reduce throughput.
 *
 * @param seq Sequence number for request/response correlation.
 * @return Encoded packet ready for transmission.
 */
std::vector<uint8_t> make_get_sf(uint8_t seq);

/**
 * @brief Build a packet to set the LoRa spreading factor.
 *
 * @param seq Sequence number for request/response correlation.
 * @param sf  Spreading factor, must be in the range 7–12.
 * @return Encoded packet ready for transmission.
 */
std::vector<uint8_t> make_set_sf(uint8_t seq, uint8_t sf);

/**
 * @brief Build a packet asking the node for its LoRa bandwidth.
 *
 * Bandwidth is reported in Hz (e.g., 125000, 250000, 500000).
 *
 * @param seq Sequence number for request/response correlation.
 * @return Encoded packet ready for transmission.
 */
std::vector<uint8_t> make_get_bw(uint8_t seq);

/**
 * @brief Build a packet to set the LoRa bandwidth.
 *
 * Larger bandwidth increases data rate but reduces range. Common values:
 * 125000 Hz, 250000 Hz, 500000 Hz.
 *
 * @param seq Sequence number for request/response correlation.
 * @param hz  Bandwidth in Hz.
 * @return Encoded packet ready for transmission.
 */
std::vector<uint8_t> make_set_bw(uint8_t seq, uint32_t hz);

/**
 * @brief Build a packet asking the node for its coding rate.
 *
 * Coding rate is reported as denominator (5–8), corresponding to LoRa
 * rates 4/5 through 4/8.
 *
 * @param seq Sequence number for request/response correlation.
 * @return Encoded packet ready for transmission.
 */
std::vector<uint8_t> make_get_cr(uint8_t seq);

/**
 * @brief Build a packet to set the coding rate.
 *
 * @param seq Sequence number for request/response correlation.
 * @param cr  Coding rate denominator (5–8 => 4/5..4/8).
 * @return Encoded packet ready for transmission.
 */
std::vector<uint8_t> make_set_cr(uint8_t seq, uint8_t cr);

/**
 * @brief Build a packet asking the node for its TX power.
 *
 * Returns the configured transmit power in dBm.
 *
 * @param seq Sequence number for request/response correlation.
 * @return Encoded packet ready for transmission.
 */
std::vector<uint8_t> make_get_tx_pwr(uint8_t seq);

/**
 * @brief Build a packet to set the TX power.
 *
 * TX power must be in the radio’s safe range (typically -20 to +23 dBm).
 * Higher values increase range but draw more current.
 *
 * @param seq Sequence number for request/response correlation.
 * @param dbm Desired TX power in dBm.
 * @return Encoded packet ready for transmission.
 */
std::vector<uint8_t> make_set_tx_pwr(uint8_t seq, int8_t dbm);

/**
 * @brief Build a packet asking the node for its channel index.
 *
 * Channels are abstract indexes; the exact mapping is implementation-defined.
 *
 * @param seq Sequence number for request/response correlation.
 * @return Encoded packet ready for transmission.
 */
std::vector<uint8_t> make_get_chan(uint8_t seq);

/**
 * @brief Build a packet to set the channel index.
 *
 * Changes the logical channel used by the node. The meaning of channel
 * numbers depends on firmware configuration.
 *
 * @param seq Sequence number for request/response correlation.
 * @param ch  Channel index (u8).
 * @return Encoded packet ready for transmission.
 */
std::vector<uint8_t> make_set_chan(uint8_t seq, uint8_t ch);

// =========================== Behavior Builders ========================

/**
 * @brief Build a packet asking the node for its operating mode.
 *
 * The "mode" field is firmware-defined. Typical examples:
 *   - 0 = relay (forward packets for others)
 *   - 1 = direct (end-node only, no relaying)
 *   - 2 = gateway (uplinks to a host or service)
 *
 * @param seq Sequence number for request/response correlation.
 * @return Encoded packet ready for transmission.
 */
std::vector<uint8_t> make_get_mode(uint8_t seq);

/**
 * @brief Build a packet to set the node's operating mode.
 *
 * Changes how the node participates in the mesh. Only use supported values,
 * as unknown modes may be ignored or rejected by the firmware.
 *
 * @param seq  Sequence number for request/response correlation.
 * @param mode New mode value (e.g., 0=relay, 1=direct, 2=gateway).
 * @return Encoded packet ready for transmission.
 */
std::vector<uint8_t> make_set_mode(uint8_t seq, uint8_t mode);


/**
 * @brief Build a packet asking for the node's maximum hop count (TTL).
 *
 * Hop count limits how far a packet can be forwarded through the mesh.
 *
 * @param seq Sequence number for request/response correlation.
 * @return Encoded packet ready for transmission.
 */
std::vector<uint8_t> make_get_hops(uint8_t seq);

/**
 * @brief Build a packet to set the node's maximum hop count (TTL).
 *
 * A higher value allows packets to travel further in the mesh,
 * but increases network traffic. Lower values reduce chatter but
 * shorten reach.
 *
 * @param seq  Sequence number for request/response correlation.
 * @param hops Maximum number of hops allowed (u8).
 * @return Encoded packet ready for transmission.
 */
std::vector<uint8_t> make_set_hops(uint8_t seq, uint8_t hops);


/**
 * @brief Build a packet asking for the node's beacon interval.
 *
 * Beaconing allows nodes to periodically announce their presence.
 * The interval is reported in seconds.
 *
 * @param seq Sequence number for request/response correlation.
 * @return Encoded packet ready for transmission.
 */
std::vector<uint8_t> make_get_beacon(uint8_t seq);

/**
 * @brief Build a packet to set the node's beacon interval.
 *
 * Setting a shorter beacon interval makes the node more visible
 * but consumes more airtime and power. A longer interval is more
 * efficient but slower to detect.
 *
 * @param seq  Sequence number for request/response correlation.
 * @param secs Beacon interval in seconds (u32).
 * @return Encoded packet ready for transmission.
 */
std::vector<uint8_t> make_set_beacon(uint8_t seq, uint32_t secs);


/**
 * @brief Build a packet asking for the node's buffer size.
 *
 * Buffer size refers to the capacity of the outbound message queue.
 *
 * @param seq Sequence number for request/response correlation.
 * @return Encoded packet ready for transmission.
 */
std::vector<uint8_t> make_get_buf_size(uint8_t seq);

/**
 * @brief Build a packet to set the node's buffer size.
 *
 * Larger buffers hold more messages but consume more RAM.
 *
 * @param seq Sequence number for request/response correlation.
 * @param n   Desired buffer size (u16).
 * @return Encoded packet ready for transmission.
 */
std::vector<uint8_t> make_set_buf_size(uint8_t seq, uint16_t n);


/**
 * @brief Build a packet asking whether acknowledgment mode is enabled.
 *
 * ACK mode determines whether messages require explicit confirmation
 * (0 = off, 1 = on).
 *
 * @param seq Sequence number for request/response correlation.
 * @return Encoded packet ready for transmission.
 */
std::vector<uint8_t> make_get_ack_mode(uint8_t seq);

/**
 * @brief Build a packet to enable or disable acknowledgment mode.
 *
 * ACK mode improves reliability (messages get confirmed), but increases
 * traffic overhead. Disable if you need minimal airtime usage.
 *
 * @param seq Sequence number for request/response correlation.
 * @param on  0 = disable ACKs, 1 = enable ACKs.
 * @return Encoded packet ready for transmission.
 */
std::vector<uint8_t> make_set_ack_mode(uint8_t seq, uint8_t on);


// ======================= Diagnostics (read-only) ======================

/**
 * @brief Build a packet asking the node for its last received RSSI value.
 *
 * RSSI (Received Signal Strength Indicator) is reported in dBm and indicates
 * how strong the last received signal was. More negative values mean weaker signals
 * (e.g., -40 dBm = strong, -120 dBm = weak).
 *
 * @param seq Sequence number for request/response correlation.
 * @return Encoded packet ready for transmission.
 */
std::vector<uint8_t> make_get_rssi(uint8_t seq);

/**
 * @brief Build a packet asking the node for its last received SNR value.
 *
 * SNR (Signal-to-Noise Ratio) is reported in dB and reflects link quality.
 * Higher SNR means cleaner reception. Values near or below 0 dB may cause errors.
 *
 * @param seq Sequence number for request/response correlation.
 * @return Encoded packet ready for transmission.
 */
std::vector<uint8_t> make_get_snr(uint8_t seq);

/**
 * @brief Build a packet asking the node for its supply/battery voltage.
 *
 * Voltage is reported in millivolts (mV). Monitoring VBAT helps assess
 * battery health or power supply stability in the field.
 *
 * @param seq Sequence number for request/response correlation.
 * @return Encoded packet ready for transmission.
 */
std::vector<uint8_t> make_get_vbat(uint8_t seq);

/**
 * @brief Build a packet asking the node for its temperature.
 *
 * Temperature is reported in tenths of a degree Celsius. For example,
 * a value of 253 corresponds to 25.3 °C. Useful for diagnosing overheating
 * or environmental conditions.
 *
 * @param seq Sequence number for request/response correlation.
 * @return Encoded packet ready for transmission.
 */
std::vector<uint8_t> make_get_temp(uint8_t seq);

/**
 * @brief Build a packet asking the node for available free RAM.
 *
 * Reported in bytes. This shows how much heap memory is left for runtime
 * operations. Low values may indicate memory leaks or overload.
 *
 * @param seq Sequence number for request/response correlation.
 * @return Encoded packet ready for transmission.
 */
std::vector<uint8_t> make_get_free_mem(uint8_t seq);

/**
 * @brief Build a packet asking the node for available flash storage.
 *
 * Reported in bytes. Useful to know how much log/config space remains.
 * Can help determine when to clear logs or rotate storage.
 *
 * @param seq Sequence number for request/response correlation.
 * @return Encoded packet ready for transmission.
 */
std::vector<uint8_t> make_get_free_flash(uint8_t seq);

/**
 * @brief Build a packet asking the node for its log entry count.
 *
 * Returns the number of diagnostic or system log entries stored locally.
 * This can indicate whether the node has recent issues to review.
 *
 * @param seq Sequence number for request/response correlation.
 * @return Encoded packet ready for transmission.
 */
std::vector<uint8_t> make_get_log_count(uint8_t seq);

// ============================= Bulk Read ==============================

/**
 * @brief Construct a GET_ALL request frame.
 *
 * This tells the node: "dump everything you’ve got" — ID, alias, frequency,
 * spreading factor, power, etc. Because the response can be too big for a
 * single packet, the node may send *multiple* RESP_OK frames back, each
 * containing a slice of TLVs (tag–length–value fields).
 *
 * Caller must be prepared to:
 *   - receive more than one frame,
 *   - accumulate TLVs until the whole picture is assembled.
 *
 * @param seq Sequence number to tag the request.
 * @return Encoded byte vector representing the GET_ALL request.
 */
std::vector<uint8_t> make_get_all(uint8_t seq);


// =========================== Response Decode ==========================

/**
 * @brief Convert a raw RESP_* frame into a compact, single-line summary.
 *
 * Example output:
 *   "status=ok seq=1 id=vt-01 freq_hz=915000000 sf=7 ..."
 *
 * Purpose:
 *   - Makes raw binary frames readable in logs or shell output.
 *   - Produces a line that is both human-readable and grep-friendly.
 *
 * Important notes:
 *   - This is *lossy*: TLVs are flattened into key=value pairs, and
 *     formatting may discard strict typing or detailed structure.
 *   - If you need precise structured values (e.g., exact integer types),
 *     you should decode the TLVs directly in your caller.
 *
 * @param frame Raw byte vector containing a single RESP_* frame.
 * @return A summarized string (human+machine friendly).
 */
std::string decode_pretty(const std::vector<uint8_t>& frame);


} // namespace viatext
