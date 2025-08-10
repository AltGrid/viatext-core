/**
 * @file core.hpp
 * @brief ViaText Core — the minimal node brain (tick -> process loop, sane I/O, basic handlers).
 *
 * @details
 * ## Field Brief
 * In a world where networks are flaky and batteries matter, **Core** is the
 * smallest viable “node brain.” It doesn’t know radios, sockets, or filesystems.
 * It only knows **packages in**, **messages processed**, **packages out**.
 * Wrappers handle transport. Core enforces discipline.
 *
 * This header defines the **ViaText Core** orchestrator: a transport‑agnostic,
 * single‑threaded loop with fixed‑capacity queues, minimal state, and a short
 * list of built‑in handlers (deliver, ping/pong, ack surface, set‑id). It’s
 * deliberately boring — on purpose. Boring survives.
 *
 * ---
 *
 * @par Why This Exists 
 * - **Simplicity:** You can audit it in one sitting. No hidden threads, no
 *   heap surprises on MCUs. Behavior is predictable under stress.
 * - **Portability:** Same brain runs on ESP32, Raspberry Pi, or a headless
 *   Linux box in a backpack. Wrappers swap the transport; Core doesn’t care.
 * - **Autonomy:** Works offline, off‑grid, and off the record. Store‑and‑forward
 *   is a first‑class pattern. If the cloud dies, this doesn’t.
 *
 * ---
 *
 * @par What This File Provides
 * - `viatext::Core` — the orchestrator class that:
 *   - Owns the node identity (callsign).
 *   - Accepts inbound `Package` objects from any wrapper via `add_message()`.
 *   - On each `tick(now_ms)`, processes **at most one** inbound item.
 *   - Emits outbound `Package` objects retrievable with `get_message()`.
 * - Sensible defaults for capacity, TTL, and fragment policy (MVP).
 *
 * ---
 *
 * @par What Depends On This
 * - **Linux station/daemon wrappers** that read/write stamps on stdin/stdout or
 *   serial (`/dev/ttyUSB*`), enqueueing inbound and draining outbound.
 * - **ESP32/LoRa firmware** that packages over‑the‑air frames into `Package`
 *   stamps, and vice‑versa.
 * - **Test harnesses/CLIs** that fuzz stamps, measure latency, and verify
 *   ack/receive surfaces without touching radios.
 *
 * ---
 *
 * @par Operational Model (one look, no guessing)
 * ```
 *  [Wrapper: LoRa/Serial/CLI]           [Core]
 *             │                          │
 *   build Package(inbound) ── add_message() ──►  inbox (bounded)
 *             │                          │
 *             │                   tick(now_ms) ──► process_one()
 *             │                          │             │
 *             │                          │             ├─ dedupe(seq ring)
 *             │                          │             ├─ fragment stub (MVP)
 *             │                          │             └─ dispatch handlers
 *             │                          │
 *             ◄──────────── get_message() ── outbox (bounded)
 *                                 │
 *                   transmit / log / inspect (by eye if needed)
 * ```
 *
 * - **At most one** inbound processed per tick: keeps latency measurable and
 *   behavior deterministic on tiny MCUs. Increase tick rate if you need more
 *   throughput. Determinism beats drama.
 *
 * ---
 *
 * @par Design Constraints & Trade‑offs
 * - **No transport logic here.** Radios, sockets, files — wrappers only.
 * - **No parsing policy.** Flags/keys in `Package.args` are preserved _exactly_
 *   (e.g., `-m`, `--ttl`). Core doesn’t reinterpret your CLI.
 * - **Bounded memory.** Inbox/outbox and recent‑sequence ring are fixed size.
 *   When full, new inputs are refused or oldest seqs expire. No heap storms.
 * - **Fragments (MVP).** Stored via a stub (placeholder). Only 1/1 dispatches.
 *   Full reassembly is planned but not required to move plaintext today.
 * - **Soft TTL guard.** `hops_max_` caps abusive propagation; wrappers should
 *   also enforce transport‑side limits.
 *
 * ---
 *
 * @par Failure Model (read before the field does)
 * - **Inbox full:** `add_message()` returns `false`. Caller decides whether to
 *   retry, drop, or back‑pressure the transport.
 * - **Invalid stamp:** Dropped silently (MVP). Wrappers may log. Future versions
 *   can emit a `-invalid` event if desired.
 * - **Duplicate sequence:** Dropped via recent‑sequence ring.
 * - **Fragmented message:** Held (stub), not dispatched (MVP).
 *
 * ---
 *
 * @par Security Posture (pragmatic, not paranoid)
 * - Plaintext by default for **inspectability** and resilience. Optional
 *   encryption can wrap the body without changing the stamp grammar.
 * - No schema lock‑in; stamps self‑describe. This lowers attack surface tied to
 *   external registries and “magic” formats.
 * - Validation is tight but simple. The smaller the brain, the smaller the bug.
 *
 * ---
 *
 * @par Extensibility Hooks
 * - **Handlers:** Extend `dispatch()` branches (`-m`, `-p`, `-ack`, `--set-id`)
 *   with new flags as the protocol grows.
 * - **Policy knobs:** `set_hops_max()`, `set_frag_cap()` for environment‑specific tuning.
 * - **Reassembly table:** Replace `store_fragment_stub()` with a bounded map when
 *   fragment support graduates from MVP.
 *
 * ---
 *
 * @par Minimal Usage Example
 * @code
 * viatext::Core core("NODE1");
 * // Inbound from wrapper (radio/serial/CLI):
 * viatext::Package in = ...;          // payload stamp + args
 * core.add_message(in);
 *
 * // Tick from your system clock:
 * core.tick(millis());                // processes at most one inbound
 *
 * // Drain outbound:
 * viatext::Package out;
 * while (core.get_message(out)) {
 *   // wrapper transmits or logs `out`
 * }
 * @endcode
 *
 * ---
 *
 * @par Notes for Maintainability (read in a bunker, 20 years out)
 * - Keep the **surface area** small and testable. Resist cleverness.
 * - Favor **deterministic** behavior over throughput. If you need more speed,
 *   spin the tick faster or shard nodes — not the API.
 * - All choices here serve three standards: **Simplicity / Portability / Autonomy**.
 *
 * @authors
 * @author Leo
 * @author ChatGPT
 */
#ifndef VIATEXT_CORE_HPP
#define VIATEXT_CORE_HPP

#include <stdint.h>
#include "etl/deque.h"
#include "etl/string.h"
#include "message_id.hpp"
#include "package.hpp"
#include "message.hpp"

namespace viatext {

/**
 * @class Core
 * @brief Minimal, transport-agnostic orchestrator for ViaText nodes.
 *
 * @details
 * **Core** is the “brain stem” of a ViaText node — small enough to audit
 * in one sitting, yet essential for moving messages through the system.
 * It doesn’t send or receive over the air, wire, or socket — wrappers do
 * that. Core’s job is to:
 * - Hold the node’s identity (callsign) for routing and address matching.
 * - Queue inbound `viatext::Package` objects from any transport wrapper.
 * - On each `tick()`, process at most one inbound package:
 *   - Validate and deduplicate.
 *   - Handle fragments (MVP stub).
 *   - Dispatch to minimal built-in handlers (message, ping/pong, ack, set-id).
 * - Queue resulting outbound packages for retrieval via `get_message()`.
 *
 * **Why it matters:**
 * - Keeps message handling **simple** and predictable under load.
 * - Remains **portable** across embedded and Linux builds.
 * - Stays **autonomous** — no external services or schemas required.
 *
 * @note
 * Core’s public API has three primary functions:
 *   1. `add_message()` — feed inbound traffic into Core.
 *   2. `tick()` — process queued inbound messages.
 *   3. `get_message()` — retrieve outbound messages for sending or logging.
 * These form the main loop for any ViaText wrapper.
 *
 * **Typical usage:**
 * @code
 * viatext::Core core("NODE1");
 *
 * // From your wrapper: enqueue inbound traffic
 * core.add_message(in_pkg);
 *
 * // Advance the node's clock and process one item
 * core.tick(millis());
 *
 * // Drain outbound queue and hand off to wrapper for sending
 * viatext::Package out;
 * while (core.get_message(out)) {
 *   // wrapper transmits or logs `out`
 * }
 * @endcode
 *
 * @see viatext::Package (include/viatext/package.hpp)
 */

class Core {
public:
  /// @name Tunable capacities & policy knobs (compile-time)
  ///@{

  /**
   * @brief Maximum inbound queue size.
   *
   * @details
   * Caps how many inbound packages can wait for processing. When the inbox is full,
   * add_message() returns false. This forces back-pressure upstream and keeps RAM
   * usage predictable in low-bandwidth or unstable links.
   *
   * @see add_message(), tick()
   */
  static constexpr size_t INBOX_CAP          = 16;   ///< Max inbound packages queued

  /**
   * @brief Maximum outbound queue size.
   *
   * @details
   * Caps how many outbound packages can wait to be drained by get_message().
   * Prevents unbounded growth if the transport is slow or paused.
   *
   * @see get_message()
   */
  static constexpr size_t OUTBOX_CAP         = 16;   ///< Max outbound packages queued

  /**
   * @brief Size of the recent-sequence deduplication ring.
   *
   * @details
   * Remember this many recent message sequence IDs to drop duplicates quickly.
   * Larger values catch more replays but use more memory; smaller values save RAM
   * but shorten the replay window.
   */
  static constexpr size_t RECENT_SEQ_CAP     = 64;   ///< Dedupe ring size

  /**
   * @brief Default maximum hop count (TTL).
   *
   * @details
   * Soft limit on how far a message is allowed to propagate. Helps prevent
   * uncontrolled rebroadcast in dense meshes. Wrappers may also enforce their
   * own TTL on the transport side.
   */
  static constexpr uint8_t HOPS_MAX_DEFAULT  = 7;    ///< Default TTL cap

  /**
   * @brief Maximum allowed number of fragments per message.
   *
   * @details
   * Upper bound on accepted fragment count. In the MVP, fragments are stored
   * via a stub and are not reassembled; only 1/1 messages are dispatched.
   * Raising this increases memory pressure once reassembly is implemented.
   *
   * @see store_fragment_stub()
   */
  static constexpr uint8_t FRAG_CAP_DEFAULT  = 8;    ///< Max allowed parts (future reassembly)

  /**
   * @brief Maximum simultaneous in-flight sequences (reserved for future use).
   *
   * @details
   * Placeholder for future concurrency/rate control. Not used by the MVP logic,
   * but documented here so deployments can plan capacity and testing ahead of time.
   */
  static constexpr uint8_t INFLIGHT_CAP      = 4;    ///< Max in-flight sequences (future)
  ///@}

  /**
   * @brief Node ID string type.
   *
   * @details
   * Fixed-capacity ETL string used to store the node callsign. Capacity is 8,
   * which fits the recommended 1..6 character ID with headroom. Stored entirely
   * in static memory; no heap allocation. Visible in logs and used for address
   * matching in handlers.
   *
   * @note Uppercase IDs are recommended for clarity in field logs.
   *
   * @see Core::Core(const NodeIdStr&), Core::Core(const char*),
   *      Core::set_node_id(const NodeIdStr&), Core::set_node_id(const char*)
   */
  /// Node ID string type (fits your 1..6 char callsign with a bit of headroom).
  using NodeIdStr = etl::string<8>;

    
  /**
   * @brief Construct a Core with a node identity.
   *
   * @details
   * Initializes the Core with a prebuilt `NodeIdStr` (fixed-capacity ETL string)
   * representing the node's callsign or unique identifier. This ID is used in
   * routing decisions, address matching, and outbound message headers.
   *
   * **Operational notes:**
   * - Keep IDs short (1–6 characters) for readability in logs and low airtime usage.
   * - Uppercase is recommended for clarity and to avoid case-sensitive mismatches.
   * - No heap allocation — this is MCU-safe.
   *
   * @param node_id
   *   Callsign/ID of this node, stored directly as provided.
   *
   * @see set_node_id(const NodeIdStr&), get_node_id()
   */
  explicit Core(const NodeIdStr& node_id);

  /**
   * @brief Construct a Core with a C-style node identity.
   *
   * @details
   * Initializes the Core by safely copying from a null-terminated string into
   * the fixed-capacity `NodeIdStr` type. This form is useful when IDs come from
   * configuration files, EEPROM, serial input, or other runtime sources.
   *
   * **Operational notes:**
   * - Input longer than `NodeIdStr::max_size()` is silently truncated.
   * - Uppercase IDs are recommended for clarity.
   * - If `node_id` is `nullptr`, the stored ID will be empty until explicitly set.
   * - No heap allocation — safe for embedded environments.
   *
   * @param node_id
   *   Null-terminated string (copied up to NodeIdStr capacity).
   *
   * @see set_node_id(const char*), get_node_id()
   */
  explicit Core(const char* node_id);


  /**
   * @brief Change the node identity at runtime.
   *
   * @details
   * Updates the node's callsign/ID from an existing `NodeIdStr`.
   * This affects how the node identifies itself in all future outbound
   * messages and how it matches inbound addresses.
   *
   * **Operational notes:**
   * - Keep IDs short (1–6 characters) for efficient transmission.
   * - Uppercase is recommended for consistency and readability in logs.
   * - Change takes effect immediately for subsequent ticks.
   *
   * @param node_id
   *   New node identity, provided as an ETL fixed-capacity string.
   *
   * @see Core(const NodeIdStr&), get_node_id()
   */
  void set_node_id(const NodeIdStr& node_id);

  /**
   * @brief Change the node identity at runtime.
   *
   * @details
   * Safely copies a null-terminated C string into the fixed-capacity `NodeIdStr`
   * and uses it as the new node identity. This is intended for IDs sourced
   * from configuration files, serial/CLI input, or other runtime data.
   *
   * **Operational notes:**
   * - Strings longer than `NodeIdStr::max_size()` are silently truncated.
   * - If `node_id` is `nullptr`, the stored ID remains unchanged.
   * - Uppercase is recommended for clarity and address matching.
   * - Change takes effect immediately for subsequent ticks.
   *
   * @param node_id
   *   Null-terminated string containing the new node identity.
   *
   * @see Core(const char*), get_node_id()
   */
  void set_node_id(const char* node_id);

  /**
   * @brief Read the node's current identity.
   *
   * @details
   * Returns a constant reference to the node's callsign/ID as stored in the
   * fixed-capacity `NodeIdStr`. This ID is used in routing decisions and
   * outbound message headers.
   *
   * **Operational notes:**
   * - Returned reference remains valid for the lifetime of the `Core` instance
   *   unless changed via `set_node_id()`.
   * - Use this for read-only access; do not attempt to modify the returned value.
   * - Commonly used by wrappers for logging, display, or outbound routing checks.
   *
   * @return
   *   Constant reference to the current `NodeIdStr` identity.
   *
   * @see set_node_id(const NodeIdStr&), set_node_id(const char*)
   */
  const NodeIdStr& get_node_id() const;

  /**
   * @brief Enqueue an inbound package for processing.
   *
   * @details
   * This is one of Core's three primary loop functions:
   *   1. `add_message()` — feed inbound traffic into Core.
   *   2. `tick()` — process queued inbound messages.
   *   3. `get_message()` — retrieve outbound messages for transmission or logging.
   *
   * Accepts a `viatext::Package` object (defined in `include/viatext/package.hpp`)
   * from any transport wrapper (LoRa, serial, CLI, etc.) and places it in the
   * inbound queue. `Package` is the standardized ViaText message container,
   * holding both:
   *   - **payload** — the message “stamp” (compact header) and body.
   *   - **args** — parsed flags and key/value pairs for routing and control, and 
   *   any other args / flags you might pass to output intentionally. 
   *
   * Once enqueued, the package will be processed on a future call to `tick()`,
   * where Core applies deduplication, basic policy checks, and built-in handler
   * dispatch.
   *
   * @param pkg
   *   The `Package` to enqueue.
   *   - Payload should be a valid ViaText stamp and body.
   *   - Args may include standard flags like `-m`, `-p`, `-ack`, or custom values.
   *
   * @retval true  Successfully enqueued.
   * @retval false Inbox was full; package was not accepted.
   *
   * @note
   * - Inbox size is fixed (`INBOX_CAP`) to ensure bounded memory use.
   * - No deep validation is performed here; that happens during `tick()`.
   * - Wrappers should handle back-pressure or drops if `false` is returned.
   */
  bool add_message(const Package& pkg);

  /**
   * @brief Advance Core's internal clock and process at most one inbound package.
   *
   * @details
   * This is one of Core's three primary loop functions:
   *   1. `add_message()` — feed inbound traffic into Core.
   *   2. `tick()` — process queued inbound messages.
   *   3. `get_message()` — retrieve outbound messages for transmission or logging.
   *
   * `tick()` updates Core's uptime, increments its tick counter, and processes
   * up to one package from the inbound queue:
   *   - Forms a `viatext::Message` from the inbound `Package`.
   *   - Applies minimal policy: hop limit check, fragment stub, deduplication.
   *   - Dispatches to built-in handlers (`-m`, `-p`, `-ack`, `--set-id`).
   *   - Queues any resulting outbound packages in the outbox for later retrieval.
   *
   * This method should be called regularly by the wrapper's main loop, using a
   * monotonic millisecond counter from the host environment.
   *
   * @param now_ms
   *   Current uptime in milliseconds (monotonic, wraps at 32 bits).
   *
   * @note
   * - Processes at most one inbound per call to keep behavior deterministic
   *   in low-power and embedded contexts.
   * - The tick rate controls throughput; faster ticks = higher processing rate.
   * - Uptime is tracked internally as 64-bit to avoid rollover in long-running nodes.
   *
   * @see add_message(), get_message()
   */
  void tick(uint32_t now_ms);

  /**
   * @brief Retrieve the next outbound package for transmission or logging.
   *
   * @details
   * This is one of Core's three primary loop functions:
   *   1. `add_message()` — feed inbound traffic into Core.
   *   2. `tick()` — process queued inbound messages.
   *   3. `get_message()` — retrieve outbound messages for transmission or logging.
   *
   * `get_message()` dequeues the oldest package from the outbox and writes it
   * into `out_pkg`. Wrappers then send the package over their transport
   * (LoRa, serial, TCP, etc.) or handle it as needed (log, store, etc.).
   *
   * Outbound packages are typically generated as a result of:
   *   - Direct message delivery events.
   *   - Protocol responses (e.g., pong to a ping).
   *   - Acknowledgements or other control events.
   *
   * @param out_pkg
   *   Reference to a `viatext::Package` that will be filled with the next outbound item.
   *
   * @retval true  A package was retrieved and `out_pkg` is valid.
   * @retval false The outbox was empty; no action taken.
   *
   * @note
   * - Outbox size is fixed (`OUTBOX_CAP`) to ensure bounded memory usage.
   * - Retrieving a package removes it from the outbox.
   * - Wrappers should call this in a loop until it returns `false` to fully
   *   drain all outbound messages for the current cycle.
   *
   * @see add_message(), tick()
   */
  bool get_message(Package& out_pkg);

   /**
   * @brief Number of ticks performed.
   *
   * @details
   * Returns the monotonic tick counter, incremented once per call to `tick()`.
   * This is useful for measuring how many processing cycles Core has executed
   * since initialization.
   *
   * @return
   *   32-bit tick counter value.
   *
   * @note
   * - Wraps naturally on overflow; not tied to wall clock.
   * - Can be used for lightweight diagnostics or scheduling logic.
   */
  uint32_t tick_count() const { return tick_count_; }

  /**
   * @brief Node uptime in milliseconds.
   *
   * @details
   * Returns the accumulated uptime in milliseconds as a 64-bit counter,
   * based on the `now_ms` values passed to `tick()`.
   *
   * @return
   *   64-bit uptime in milliseconds.
   *
   * @note
   * - Not affected by tick rate; uptime is purely time-based.
   * - Useful for timestamping events or calculating session duration.
   */
  uint64_t uptime_ms() const { return uptime_ms_; }

  /**
   * @brief Get the current maximum hop count (TTL).
   *
   * @details
   * Returns the maximum number of hops a message is allowed before being dropped.
   * This helps limit broadcast storms and uncontrolled propagation in mesh networks.
   *
   * @return
   *   Current hop limit (TTL).
   *
   * @see set_hops_max()
   */
  uint8_t hops_max() const { return hops_max_; }

  /**
   * @brief Set the maximum hop count (TTL).
   *
   * @details
   * Adjusts the hop limit used to determine whether a message should be processed
   * or discarded. Lower values limit range; higher values allow wider propagation.
   *
   * @param v
   *   New maximum hop count.
   *
   * @see hops_max()
   */
  void set_hops_max(uint8_t v) { hops_max_ = v; }

  /**
   * @brief Get the current maximum fragment count.
   *
   * @details
   * Returns the configured maximum allowed parts for a single message.
   * In the MVP, fragments beyond this limit are dropped or ignored.
   *
   * @return
   *   Current fragment capacity.
   *
   * @see set_frag_cap()
   */
  uint8_t frag_cap() const { return frag_cap_; }

  /**
   * @brief Set the maximum fragment count.
   *
   * @details
   * Adjusts the maximum allowed parts for a single message. Used to control
   * memory usage and reassembly complexity.
   *
   * @param v
   *   New fragment capacity limit.
   *
   * @see frag_cap()
   */
  void set_frag_cap(uint8_t v) { frag_cap_ = v; }

private:
  /**
   * @brief Process one inbound package from the inbox.
   *
   * @details
   * Pops a single package from the inbound queue and applies minimal policy:
   *   - Validates the message format.
   *   - Enforces hop limit.
   *   - Stores fragments (MVP stub) without dispatch.
   *   - Drops duplicates using recent sequence ring.
   *   - Dispatches to the correct handler if eligible.
   *
   * @param now_ms Current uptime in milliseconds (monotonic, 32-bit).
   */
  void process_one(uint32_t now_ms);

  /**
   * @brief Dispatch a validated message to the appropriate built-in handler.
   *
   * @details
   * Routes the message based on flags in its args list. Only the first matching
   * handler runs in the MVP. Extend here when adding new protocol verbs.
   *
   * @param msg Validated inbound message.
   */
  void dispatch(const Message& msg);

  
  /**
   * @brief Handle standard message delivery.
   *
   * @details
   * Processes an inbound message with the `-m` flag.
   * - If the message is addressed to this node, and requests an ACK, an ACK
   *   package is queued.
   * - A "delivered" event is always queued for addressed messages.
   * - Forwarding decisions are left to the transport wrapper.
   *
   * @param msg Validated inbound message.
   */
  void handle_message(const Message& msg);

  /**
   * @brief Handle inbound ping requests.
   *
   * @details
   * Processes an inbound message with the `-p` flag and queues a PONG
   * package addressed back to the sender.
   *
   * @param msg Validated inbound message.
   */
  void handle_ping(const Message& msg);

  /**
   * @brief Handle inbound acknowledgments.
   *
   * @details
   * Processes an inbound message with the `-ack` flag and queues a simple
   * ACK event package for surfacing to higher layers (e.g., logging or UI).
   *
   * @param msg Validated inbound message.
   */
  void handle_ack(const Message& msg);

  /**
   * @brief Handle node identity change requests.
   *
   * @details
   * Processes an inbound message with the `--set-id` flag.
   * - If the message body contains a non-empty value, updates the node ID.
   * - Queues an "ID set" event for higher-level handling.
   *
   * @param msg Validated inbound message.
   */
  void handle_set_id(const Message& msg);


  // Recent-sequence ring helpers

  /**
   * @brief Check if a sequence number has been seen recently.
   *
   * @details
   * Scans the recent-sequence ring buffer to determine whether the given
   * message sequence ID has already been processed. Used for duplicate
   * suppression.
   *
   * @param seq Sequence number to check.
   * @return true if the sequence is found in the ring; false otherwise.
   *
   * @note
   * - The ring buffer size is fixed at RECENT_SEQ_CAP.
   * - Older entries are automatically evicted as new ones are added.
   */
  bool contains_sequence(uint16_t seq) const;

  /**
   * @brief Add a sequence number to the recent-sequence ring.
   *
   * @details
   * Inserts a message sequence ID into the deduplication ring buffer.
   * If the ring is full, the oldest entry is evicted.
   *
   * @param seq Sequence number to record.
   *
   * @note
   * Call this only for valid, non-duplicate messages to avoid polluting
   * the deduplication table.
   */
  void push_recent_sequence(uint16_t seq);

  // Fragment placeholder

  /**
   * @brief Store a message fragment (MVP placeholder).
   *
   * @details
   * Placeholder for future fragment reassembly logic.
   * In the MVP, this function does not perform any storage; it simply
   * exists to mark where fragment handling would occur.
   *
   * @param msg    Inbound fragment message.
   * @param now_ms Current uptime in milliseconds (monotonic).
   *
   * @note
   * - Future implementation may maintain a bounded fragment table keyed
   *   by sequence and source.
   * - The fragment limit is defined by frag_cap().
   */
  void store_fragment_stub(const Message& msg, uint32_t now_ms);


  // Outbound builders (sensible outputs)

  /**
   * @brief Build an ACK package in response to a message requesting acknowledgment.
   *
   * @param msg Inbound message that triggered the ACK.
   * @return Fully prepared outbound Package with ACK flag set.
   */
  Package make_ack_package(const Message& msg) const;

  /**
   * @brief Build a "delivered" event package for a successfully received message.
   *
   * @param msg Inbound message that was delivered.
   * @return Outbound Package indicating delivery status.
   */
  Package make_delivered_package(const Message& msg) const;

  /**
   * @brief Build a PONG package in response to a ping.
   *
   * @param msg Inbound ping message.
   * @return Outbound Package containing the pong reply.
   */
  Package make_pong_package(const Message& msg) const;

  /**
   * @brief Build an ACK event package for surfacing inbound acknowledgments.
   *
   * @param msg Inbound ACK message.
   * @return Outbound Package for logging or event handling layers.
   */
  Package make_ack_event_package(const Message& msg) const;

  /**
   * @brief Build an event indicating that the node ID was changed.
   *
   * @param new_id New node identity.
   * @return Outbound Package announcing the ID change.
   */
  Package make_id_set_event(const NodeIdStr& new_id) const;

  // tiny util

  /**
   * @brief Convert an integer to a decimal string without heap usage.
   *
   * @param n Number to convert.
   * @return ETL string containing the decimal representation.
   */
  static etl::string<16> to_string_number(uint32_t n);


private:
  // ---------- state ----------
  NodeIdStr node_id_{};  ///< Current node identity (callsign/ID) used for routing and addressing.

  uint32_t  ticks_{0};          ///< Deprecated alias; maintained for legacy code. Use tick_count_ instead.
  uint32_t  tick_count_{0};     ///< Monotonic count of ticks executed since Core was constructed.
  uint64_t  uptime_ms_{0};      ///< Accumulated uptime in milliseconds (based on tick() input).
  uint64_t  last_ms_{0};        ///< Last tick() time value in milliseconds (monotonic).

  uint8_t   hops_max_{HOPS_MAX_DEFAULT};  ///< Current hop limit (TTL) for inbound messages.
  uint8_t   frag_cap_{FRAG_CAP_DEFAULT};  ///< Current maximum allowed fragments per message.

  etl::deque<Package, INBOX_CAP>  inbox_;        ///< Fixed-capacity queue of inbound packages waiting to be processed.
  etl::deque<Package, OUTBOX_CAP> outbox_;       ///< Fixed-capacity queue of outbound packages waiting to be retrieved.
  etl::deque<uint16_t, RECENT_SEQ_CAP> recent_seqs_; ///< Recent message sequence IDs for duplicate suppression.

};

} // namespace viatext

#endif // VIATEXT_CORE_HPP
