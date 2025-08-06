/**
 * @file core.hpp
 * @brief ViaTextCore: Main logic engine for the ViaText node protocol (Linux & ESP32-compatible)
 *
 * @details
 *   The ViaTextCore class implements the primary logic and message handling for a single node
 *   in the ViaText communication mesh. It maintains node state, processes inbound and outbound
 *   messages (as argument strings), and dispatches them to specialized handlers according to
 *   the ViaText protocol.
 *
 *   **Key features:**
 *   - Easy to read and modify for new C++ users (and AI tools)
 *   - Simple event loop ("tick") design—processes one message per tick
 *   - Uses std::queue and std::set for inbox, outbox, and deduplication (no complex dependencies)
 *   - Protocol dispatch is handled via simple if/else chain for clarity
 *   - Fully cross-platform: compiles on Linux, ESP32, and Arduino environments
 *
 *   **Usage:**
 *   - Create a ViaTextCore instance, optionally specifying a node ID
 *   - Use add_message() to queue inbound argument strings (from CLI, serial, LoRa, etc)
 *   - Call tick() in your main loop with the current system time (ms)
 *   - Use get_message() to retrieve the next outbound message string for delivery
 *
 * @author Leo
 * @author ChatGPT
 * @date 2025-08-05
 */
#pragma once
#include <string>
#include <queue>
#include <unordered_set>
#include "arg_parser.hpp"
#include "message.hpp"

namespace viatext {
/**
 * @class ViaTextCore
 * @brief Main logic core for a ViaText node—handles message dispatch, routing, and state.
 *
 * The ViaTextCore class encapsulates all node-level logic for the ViaText protocol.
 * It is designed for both Linux and microcontroller targets (ESP32 LoRa), with clear,
 * simple code to maximize approachability and portability.
 */
class ViaTextCore {
public:
    /**
     * @brief Default constructor. Initializes the node with optional ID (callsign).
     * @param optional_node_id The initial node ID. If not given, set with set_node_id().
     *
     * All state (tick, uptime, queues, deduplication set) is reset on creation.
     */
    explicit ViaTextCore(const std::string& optional_node_id = "");

    /**
     * @brief Add an inbound message (argument string) to the inbox.
     * @param arg_string The incoming argument string (from CLI, LoRa, serial, etc).
     *
     * This string is queued for processing during the next process() call.
     */
    void add_message(const std::string& arg_string);

    /**
     * @brief Advance the core state by one tick. Processes one message if available.
     * @param current_timestamp The current system time in milliseconds.
     *
     * Updates uptime and tick counters, and calls process() to handle one message.
     * Should be called regularly (e.g., once per loop or on timer interrupt).
     */
    void tick(uint64_t current_timestamp);

    /**
     * @brief Main processing function: handles one message from the inbox (if present).
     *
     * Parses the next queued message, dispatches to the appropriate handler
     * (handle_message, handle_ping, etc) according to protocol.
     */
    std::string get_message();

    /**
     * @brief Set the node ID (callsign) for this node.
     * @param new_id The string to assign as this node's ID.
     *
     * This will be used for all outgoing and routing logic.
     */
    void set_node_id(const std::string& new_id);

    /**
     * @brief Get the current tick count (since startup).
     * @return Number of times tick() has been called.
     */
    uint64_t get_tick_count() const { return tick_count; }
    
        /**
     * @brief Get system uptime (milliseconds since node startup).
     * @return Uptime in milliseconds.
     */
    uint64_t get_uptime() const { return uptime; }

    /**
     * @brief Get the current node ID.
     * @return The node's callsign as a string.
     */
    std::string get_node_id() const { return node_id; }

protected:
// --- Handlers for each message type ---

/**
 * @brief Main process loop; handles one message per call, dispatching by flag.
 *
 * This function dequeues the next inbound message (if present), parses it for arguments,
 * and calls the appropriate handler function according to the first flag or command detected.
 * 
 * @details
 *    The main protocol dispatch is handled here. Recognized flags:
 *      - "-m"        : Standard ViaText message (calls handle_message)
 *      - "-p"        : Ping request (calls handle_ping)
 *      - "-mr"       : Mesh report or mesh status request (calls handle_mesh_report)
 *      - "-ack"      : Acknowledge receipt of a message (calls handle_acknowledge)
 *      - "-save"     : Save message to storage (calls handle_save; stub)
 *      - "-load"     : Load message from storage (calls handle_load; stub)
 *      - "-get_time" : Request current node time (calls handle_get_time; stub)
 *      - "-set_time" : Set current node/system time (calls handle_set_time; stub)
 *      - "-id"       : Set node ID (direct call to set_node_id)
 *
 *    Any other flags/commands are ignored or logged (can be extended).
 */
void process();

/**
 * @brief Handler for standard message receipt ("-m").
 * @param parser Parsed arguments for this message.
 *
 * Handles inbound ViaText messages, including deduplication and delivery/acknowledgment.
 * Called only for messages where the first argument is "-m".
 */
void handle_message(const ArgParser& parser);

/**
 * @brief Handler for ping request messages ("-p").
 * @param parser Parsed arguments for this message.
 *
 * Replies with a "-pong" response. Called for messages where the first argument is "-p".
 */
void handle_ping(const ArgParser& parser);

/**
 * @brief Handler for mesh report or neighbor status request ("-mr").
 * @param parser Parsed arguments for this message.
 *
 * Replies with mesh/neighborhood status. Called for messages where the first argument is "-mr".
 */
void handle_mesh_report(const ArgParser& parser);

/**
 * @brief Handler for acknowledgment messages ("-ack").
 * @param parser Parsed arguments for this message.
 *
 * Handles acknowledgment (ACK) logic. Called for messages where the first argument is "-ack".
 */
void handle_acknowledge(const ArgParser& parser);

/**
 * @brief Handler for save request messages ("-save").
 * @param parser Parsed arguments for this message.
 *
 * Stub for save-to-storage (to be implemented in wrapper).
 * Called for messages where the first argument is "-save".
 */
void handle_save(const ArgParser&) {}      // Stub

/**
 * @brief Handler for load request messages ("-load").
 * @param parser Parsed arguments for this message.
 *
 * Stub for load-from-storage (to be implemented in wrapper).
 * Called for messages where the first argument is "-load".
 */
void handle_load(const ArgParser&) {}      // Stub

/**
 * @brief Handler for get-time requests ("-get_time").
 * @param parser Parsed arguments for this message.
 *
 * Stub for retrieving system time (to be implemented in wrapper).
 * Called for messages where the first argument is "-get_time".
 */
void handle_get_time(const ArgParser&) {}  // Stub

/**
 * @brief Handler for set-time requests ("-set_time").
 * @param parser Parsed arguments for this message.
 *
 * Stub for setting system time (to be implemented in wrapper).
 * Called for messages where the first argument is "-set_time".
 */
void handle_set_time(const ArgParser&) {}  // Stub

// --- Utilities ---

/**
 * @brief Build argument string for an acknowledgment reply ("-ack").
 * @param message The message being acknowledged.
 * @return Argument string for ACK message.
 */
std::string build_ack_args(const Message& message) const;

/**
 * @brief Build argument string for a received/delivered message ("-r").
 * @param message The message just delivered.
 * @return Argument string for receipt/logging.
 */
std::string build_received_args(const Message& message) const;

/**
 * @brief Get the number of mesh neighbors (stub; always returns 0).
 * @return Number of neighbors (real implementation: override/extend).
 */
int get_neighbor_count() const { return 0; }

// --- State ---

std::string node_id;                          ///< Node callsign (ID)
uint64_t tick_count;                          ///< Number of ticks since startup
uint64_t uptime;                              ///< Uptime in milliseconds
uint64_t last_timestamp;                      ///< Last tick timestamp (ms)
std::queue<std::string> inbox;                ///< Queue of inbound message argument strings
std::queue<std::string> outbox;               ///< Queue of outbound message argument strings
std::unordered_set<uint16_t> received_message_ids; ///< Set of received message IDs for deduplication


};

} // namespace viatext
