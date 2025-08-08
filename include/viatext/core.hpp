/**
 * @file core.hpp
 * @brief ViaText Core — Portable Event and Message Engine for Decentralized Mesh Nodes (Linux & MCU-safe)
 *
 * ---
 *
 * ## Overview
 *
 * The **ViaText Core** is the universal event/message loop engine powering all ViaText mesh nodes. It provides
 * a minimal, deterministic communication core that logs, processes, and routes messages in a way that is
 * platform-agnostic and compatible with both embedded microcontrollers (ESP32, ATmega) and Linux hosts.
 *
 * ---
 *
 * ## System Role & Philosophy
 *
 * - **Central Message Handler:** The Core logs inbound messages, queues outbound responses, handles acknowledgment and deduplication, and provides a consistent interface for mesh node logic.
 * - **Strict Separation of Concerns:** *All* hardware, radio, OS, or storage logic is handled by wrappers or external modules. The Core is strictly protocol and logic.
 * - **Portable, Deterministic, Minimal:** Uses only ETL (Embedded Template Library) containers—no STL, no heap, no exceptions, no RTTI. MCU-safe and fully testable on Linux or desktop.
 * - **Text-In/Text-Out Principle:** Everything is a string of arguments in, a string of arguments out—using a simple, Linux-style command-line convention. This enables universal compatibility across transports (serial, LoRa, files, pipes, sockets).
 *
 * ---
 *
 * ## Typical Usage Flow
 *
 * 1. **Initialization:** A wrapper (ESP32 LoRa, CLI, Daemon, etc) instantiates Core and assigns an ID (radio-style, 1–6 chars).
 * 2. **Message Ingestion:** Messages (as raw argument strings) are added to the inbox queue via `add_message()`. These could be native LoRa packets or synthesized Linux CLI commands.
 * 3. **Tick Processing:** The wrapper regularly calls `tick(timestamp)`, which:
 *    - Updates uptime and tick counters
 *    - Pops one message off the inbox and processes it (FIFO)
 *    - Dispatches to the appropriate handler based on directive (`-m`, `-p`, etc)
 *    - May queue replies or ACKs in outbox
 * 4. **Output Handling:** The wrapper pulls messages from `get_message()` (FIFO) and delivers or displays as appropriate.
 * 5. **Repeat:** The wrapper is responsible for persistence, hardware comms, mesh logic, and OS I/O.
 *
 * ---
 *
 * ## What the Core *Does Not* Do
 *
 * - **No direct hardware I/O:** Does not read serial ports, LoRa radios, or files.
 * - **No OS-specific code:** 100% standard C++/ETL; portable to any target.
 * - **No storage or history:** Persistence, message reassembly, and fragment caching is wrapper-driven.
 * - **No dynamic memory:** All buffers, lists, and queues are statically sized.
 * - **No dependencies on Linux or Arduino frameworks:** Pure ETL and standard headers.
 *
 * ---
 *
 * ## Core API (Wrapper-Facing)
 *
 * - `void add_message(const char* arg_string);`
 *     - Add a raw, argument-formatted message string to the inbox.
 * - `void tick(uint32_t millis);`
 *     - Update time state and process the next message in queue.
 * - `bool get_message(char* out, size_t max_len);`
 *     - Pop the next outbound message string (for transmission).
 *
 * Wrappers should call `tick()` regularly with a monotonically increasing timestamp (e.g., `millis()`). The Core handles
 * one message per tick for simplicity and determinism.
 *
 * ---
 *
 * ## Example Workflow
 *
 * 1. **LoRa Packet Reception (Raw Input):**
 *     ```
 *     0x4F2B000131~shrek~donkey~Shut Up
 *     ```
 * 2. **Attach Metadata (as Arguments):**
 *     ```
 *     -m -rssi 92 -snr 4.5 -sf 7 -bw 125 -cr 4/5 -data_length 12 -data 0x4F2B000131~shrek~donkey~Shut Up
 *     ```
 * 3. **Queue in Core:**
 *     ```
 *     core.add_message("-m -rssi 92 ... -data ...")
 *     ```
 * 4. **Tick (process one message):**
 *     ```
 *     core.tick(millis);
 *     ```
 * 5. **Wrapper fetches outbound:**
 *     ```
 *     char buf[256];
 *     while (core.get_message(buf, sizeof(buf))) {
 *         // Send or display...
 *     }
 *     ```
 *
 * ---
 *
 * ## Supported Command Arguments (by Convention)
 *
 * | Argument        | Description                                                         |
 * |-----------------|---------------------------------------------------------------------|
 * | `-m`            | Main message/operation flag                                         |
 * | `-rssi [val]`   | Signal strength (LoRa)                                              |
 * | `-snr [val]`    | Signal/noise ratio                                                  |
 * | `-sf [val]`     | Spreading factor                                                    |
 * | `-bw [val]`     | Bandwidth                                                           |
 * | `-cr [val]`     | Coding rate                                                         |
 * | `-data_length`  | Payload size (optional, for wire payloads)                          |
 * | `-data`         | Raw wire message string (`<id>~<from>~<to>~<payload>`)              |
 *
 * Directives are always the first argument (e.g., `-m`, `-p`, `-ack`, `-id`...).
 *
 * ---
 *
 * ## Message Parsing and Handling
 *
 * - Messages are always parsed using the internal `ArgParser`, splitting the incoming string into directive, flags, and key/value pairs.
 * - Main message type (`-m`) expects a `-data` argument containing the ViaText wire string.
 * - Message IDs are tracked to prevent duplicates; ACK/RECEIVE replies are queued as appropriate.
 * - The Core does not store full messages or fragment series—this is left to the wrapper.
 *
 * ---
 *
 * ## Internal Variables and State
 *
 * - `node_id` (ETL string): Symbolic node name/callsign (1–6 chars)
 * - `tick_count` (uint32_t): Number of ticks since init
 * - `uptime` (uint32_t): Total uptime in ms
 * - `last_timestamp` (uint32_t): For calculating elapsed time
 * - `inbox` (ETL vector<string>): FIFO queue for incoming arg strings
 * - `outbox` (ETL vector<string>): FIFO queue for outgoing arg strings
 * - `received_message_ids` (ETL vector<uint16_t>): Recent message IDs for deduplication
 *
 * ---
 *
 * ## Why This Design?
 *
 * - **Absolute portability:** Runs the same on Linux, ESP32, and ATmega.
 * - **Heap-free, deterministic operation:** Predictable resource use, zero surprises.
 * - **Human-readable and scriptable:** Text argument system is approachable for users, devs, and scripts alike.
 * - **Easy to wrap and extend:** Any environment (CLI, daemon, embedded) can be supported with minimal glue.
 * - **Philosophically durable:** Aligned with Unix, radio, and text-first computing principles.
 *
 * ---
 *
 * ## Authors
 * @author Leo
 * @author ChatGPT
 * @date 2025-08-09
 */

#ifndef VIATEXT_CORE_HPP
#define VIATEXT_CORE_HPP

#include "viatext/message.hpp"
#include "viatext/arg_parser.hpp"
#include "etl/string.h"
#include "etl/vector.h"
#include <stdint.h>
#include <stddef.h>

namespace viatext {

// ------- CONFIGURABLE CONSTANTS --------

/**
 * @brief Maximum number of inbound messages the core can queue for processing.
 * @details Tune for memory usage and expected message bursts.
 * Typical range: 8–32. Default is 16.
 *
 * A larger value allows more incoming messages to be buffered before dropping.
 * A smaller value conserves RAM on constrained MCUs.
 */
constexpr size_t INBOX_SIZE = 16;

/**
 * @brief Maximum number of outbound messages the core can queue for transmission.
 * @details Tune for how quickly your wrapper drains the outbox.
 * Typical range: 8–32. Default is 16.
 *
 * A larger value allows more replies/ACKs to queue before transmission.
 * Lower values save memory if your node sends responses infrequently.
 */
constexpr size_t OUTBOX_SIZE = 16;

/**
 * @brief Number of recent message IDs remembered for duplicate/replay protection.
 * @details This ring buffer prevents the core from processing the same message twice.
 * Default is 32; increase for busy or multi-hop networks.
 *
 * A larger value offers better protection in busy mesh environments.
 * Lower for simple, low-traffic, or single-hop networks to conserve memory.
 */
constexpr size_t RECEIVED_IDS_SIZE = 32; // Store N most recent message IDs


// ------- MAIN CLASS --------

/**
 * @class Core
 * @brief Main protocol loop and event/message state for the ViaText system.
 *
 * Simple, static, MCU/AI-friendly. No heap, no STL.
 */
class Core {
public:
    // --- Types ---
    /** 
     * @brief Alias for string buffers used in argument parsing and message queues.
     * 64 characters provides enough room for typical ViaText arguments or short messages,
     * while keeping memory usage within microcontroller constraints.
     */
    using string_t = etl::string<64>; // For argument buffers, flexible size

    /** 
     * @brief Type used for message sequence IDs (deduplication).
     * 16 bits is sufficient for message sequencing and duplicate detection.
     */
    using id_t = uint16_t; // MessageID.sequence for deduplication

    // --- State Variables ---

    /**
     * @brief The unique callsign or node ID for this ViaText instance.
     * Must conform to ViaText callsign rules (A-Z, 0-9, -, _; up to 6 chars).
     */
    etl::string<6> node_id;                          

    /**
     * @brief Monotonic tick counter, incremented each time tick() is called.
     * Useful for profiling, debugging, and scheduled maintenance tasks.
     */
    uint32_t tick_count;                             

    /**
     * @brief Accumulated uptime of the core (in milliseconds).
     * Updated by tick(), this tracks the total running time since initialization.
     */
    uint32_t uptime;                                 

    /**
     * @brief Timestamp (in milliseconds) of the last tick() invocation.
     * Used to compute time deltas and prevent clock drift.
     */
    uint32_t last_timestamp;                         

    /**
     * @brief Inbound message queue (raw argument strings).
     * Each string is a single command or message awaiting processing.
     * Bounded to INBOX_SIZE (typically 16) for MCU safety.
     */
    etl::vector<string_t, INBOX_SIZE> inbox;         

    /**
     * @brief Outbound message queue (raw argument strings).
     * Each string is a command or message ready for external transmission.
     * Bounded to OUTBOX_SIZE (typically 16).
     */
    etl::vector<string_t, OUTBOX_SIZE> outbox;       

    /**
     * @brief Ring buffer of recently seen message sequence IDs for deduplication.
     * Prevents reprocessing of duplicate or replayed messages.
     * Stores up to RECEIVED_IDS_SIZE (typically 32) most recent IDs.
     */
    etl::vector<id_t, RECEIVED_IDS_SIZE> received_message_ids; 


    /**
     * @brief Core constructor.
     *
     * Initializes the ViaText Core state.
     *
     * @param optional_node_id Optional null-terminated C-string for this node's unique ID/callsign.
     *                        If omitted, defaults to an empty string. 
     *                        Must follow ViaText callsign rules (A-Z, 0-9, -, _; up to 6 chars).
     */
    Core(const char* optional_node_id = "");

    // --- API Methods ---

    /**
     * @brief Add an inbound message (raw argument string) to the core's processing queue.
     *
     * This function accepts a raw command-line style message string, such as those
     * received from LoRa packets, CLI wrappers, or other node interfaces. The string
     * is queued for later processing by the main event loop.
     *
     * - If the inbox queue has available space, the message is appended.
     * - If the inbox is full (INBOX_SIZE reached), the message is dropped and false is returned.
     * - No parsing is performed at this stage—this function only queues the string.
     *
     * Example usage:
     * @code
     *   core.add_message("-m -rssi 92 -data 0x4F2B000131~shrek~donkey~Shut Up");
     * @endcode
     *
     * @param arg_string Null-terminated CLI/LoRa message string to enqueue.
     * @return true if the message was successfully queued; false if the inbox is full.
     */
    bool add_message(const char* arg_string);

    /**
     * @brief Main tick function: updates internal timekeeping and processes one inbound message.
     *
     * This function should be called periodically (e.g., from a main loop or timer interrupt)
     * with the current time in milliseconds. It performs the following actions:
     *
     * - Updates internal uptime and tick counters based on the time delta since the last call.
     * - Stores the new timestamp for subsequent ticks.
     * - Processes a single message from the inbox queue, if available.
     *
     * This routine is essential for driving the event loop and ensuring timely handling of
     * inbound messages, retries, and any future scheduled tasks. 
     *
     * **Must be called regularly** by the wrapper/platform to keep the core alive.
     *
     * @param current_timestamp Current system or platform time in milliseconds (e.g., from millis()).
     */
    void tick(uint32_t current_timestamp);

    /**
     * @brief Retrieve the next outbound message for transmission.
     *
     * This function provides a wrapper or platform implementation with the next available
     * message from the outbox queue, if any. The message is copied into the provided output buffer.
     * 
     * Typical usage:
     * - The wrapper calls this method after each tick to check for new messages ready to send
     *   over LoRa, serial, or other transports.
     * - If a message is available, it is copied to out_buf and removed from the queue.
     * - If no message is available, returns false and out_buf is unchanged.
     *
     * @param[out] out_buf  Destination buffer (char array) to receive the message text.
     * @param[in]  max_len  Maximum size of out_buf, including null terminator.
     * @return true if a message was written to out_buf; false if outbox is empty.
     */
    bool get_message(char* out_buf, size_t max_len);

    // --- Utility ---

    /**
     * @brief Returns the number of pending inbound messages in the inbox queue.
     *
     * Useful for monitoring the load, debugging, or wrapper logic that needs to check
     * if there are unprocessed inbound messages.
     *
     * @return The number of messages currently waiting in the inbox queue.
     */
    size_t inbox_count() const { return inbox.size(); }

    /**
     * @brief Returns the number of pending outbound messages in the outbox queue.
     *
     * Can be used by wrappers or monitoring tools to check if there are unsent
     * messages queued for transmission.
     *
     * @return The number of messages currently waiting in the outbox queue.
     */
    size_t outbox_count() const { return outbox.size(); }

    /**
     * @brief Sets this node's identifier (callsign).
     *
     * Updates the node_id string with the provided value. The node_id is used
     * to determine message ownership and correct routing decisions.
     * Input should be a valid ViaText callsign (max 6 chars, see rules).
     *
     * @param new_id Null-terminated string representing the new node callsign.
     */
    void set_node_id(const char* new_id);


private:
    // --- Internal Helpers ---

    /**
     * @brief Processes one message from the inbox, if any are present.
     *
     * This is the core state machine: retrieves the oldest inbound message,
     * parses it, identifies the directive (e.g. -m, -p), and dispatches
     * to the appropriate handler. Ensures one-at-a-time, FIFO processing.
     */
    void process();

    /**
     * @brief Handles a standard -m (message) directive.
     *
     * Deduplicates via message ID, acknowledges if required, and routes delivery.
     * If the message is addressed to this node, queues a received confirmation and
     * optionally an ACK. Otherwise, wrapper/upper layer handles forwarding.
     *
     * @param parser Parsed ArgParser instance for the incoming message.
     */
    void handle_message(const ArgParser& parser);

    /**
     * @brief Handles a -p (ping) directive.
     *
     * Queues a -pong reply addressed from this node. Used for reachability or
     * simple mesh health checks.
     *
     * @param parser Parsed ArgParser instance for the ping request.
     */
    void handle_ping(const ArgParser& parser);

    /**
     * @brief Handles a -mr (mesh report) directive.
     *
     * Queues a mesh report reply containing node status. This is a placeholder;
     * wrappers or derived classes can extend with actual neighbor stats.
     *
     * @param parser Parsed ArgParser instance for the mesh report request.
     */
    void handle_mesh_report(const ArgParser& parser);

    /**
     * @brief Handles a -ack (acknowledgement) directive.
     *
     * Updates internal state or signals to the application that an ACK has been received.
     * Wrapper or user application can extend this to update logs or trigger UI events.
     *
     * @param parser Parsed ArgParser instance for the acknowledgement.
     */
    void handle_acknowledge(const ArgParser& parser);

    // --- Customizable Handlers (stubs, can be expanded) ---

    /**
     * @brief Handles a -save directive (stub).
     *
     * Placeholder for persistent storage, e.g., saving messages to SD or flash.
     * Not implemented in the core, intended for platform-specific wrappers.
     */
    void handle_save(const ArgParser& parser);

    /**
     * @brief Handles a -load directive (stub).
     *
     * Placeholder for loading messages or state from persistent storage.
     * Not implemented in the core, intended for platform-specific wrappers.
     */
    void handle_load(const ArgParser& parser);

    /**
     * @brief Handles a -get_time directive (stub).
     *
     * Placeholder for responding with system time. Not implemented in the core.
     * Wrappers can implement real time sync or response.
     */
    void handle_get_time(const ArgParser& parser);

    /**
     * @brief Handles a -set_time directive (stub).
     *
     * Placeholder for setting system time. Not implemented in the core.
     * Wrappers can implement real time update logic.
     */
    void handle_set_time(const ArgParser& parser);

    // --- Builder Utilities ---

    /**
     * @brief Builds a raw arg string for ACK reply, suitable for queuing in outbox.
     *
     * Formats a message with the -ack directive, including sender, recipient, and
     * sequence ID. Used to acknowledge receipt of a specific message.
     *
     * @param message The original Message object being acknowledged.
     * @param out_buf Output buffer for the formatted arg string.
     * @param max_len Maximum allowed length for the output (including null terminator).
     */
    void build_ack_args(const Message& message, char* out_buf, size_t max_len);

    /**
     * @brief Builds a raw arg string for a received reply, suitable for queuing in outbox.
     *
     * Formats a message with the -r directive, indicating receipt of the original payload.
     * Used for status, UI feedback, or audit trails.
     *
     * @param message The original Message object that was delivered.
     * @param out_buf Output buffer for the formatted arg string.
     * @param max_len Maximum allowed length for the output (including null terminator).
     */
    void build_received_args(const Message& message, char* out_buf, size_t max_len);

    /**
     * @brief Returns the number of mesh neighbors (always zero in this stub).
     *
     * Placeholder for real mesh network implementations. Can be overridden or
     * extended by platform-specific wrappers to provide actual neighbor stats.
     *
     * @return Number of known neighbors (default: 0).
     */
    uint8_t get_neighbor_count() const { return 0; }

    /**
     * @brief Checks if a message ID (sequence) has already been processed.
     *
     * Used for deduplication of incoming messages; prevents re-processing
     * duplicates or replayed packets.
     *
     * @param sequence The message sequence ID to check.
     * @return True if this message ID is already in the deduplication cache.
     */
    bool has_seen_message(id_t sequence) const;

    /**
     * @brief Adds a message ID (sequence) to the deduplication cache.
     *
     * Maintains a sliding window of recently seen message IDs for fast lookup.
     * If the cache is full, the oldest entry is removed.
     *
     * @param sequence The message sequence ID to remember.
     */
    void remember_message_id(id_t sequence);
    
    // --- Copy/Move semantics disabled (MCU safe, single instance) ---
    Core(const Core&) = delete;
    Core& operator=(const Core&) = delete;
};

} // namespace viatext

#endif // VIATEXT_CORE_HPP
