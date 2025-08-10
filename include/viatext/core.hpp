/**
 * @file core.hpp
 * @brief ViaText Core — minimal node brain (tick→process loop, sensible I/O, basic handlers).
 *
 * The Core owns a node's identity and provides a transport‑agnostic message loop:
 *  - Wrappers enqueue inbound @ref viatext::Package objects via @ref add_message.
 *  - On each @ref tick, Core tries to process at most one inbound item.
 *  - Completed results are exposed as outbound @ref viatext::Package objects via @ref get_message.
 *
 * This minimal version focuses on **sensible input/output** and a small set of
 * built‑in handlers (message delivery, ping/pong, ack surface, set‑id).
 * Transport/radio policy, routing, persistence, and full fragmentation reassembly
 * are intentionally deferred.
 *
 * ## Design notes
 * - **No parser:** keys in Package.args are preserved exactly (e.g., "-m", "--ttl").
 * - **No transport logic:** Core never touches hardware; wrappers decide how to send.
 * - **Minimal state:** fixed‑capacity queues (inbox/outbox), recent sequence ring for dedupe.
 * - **Fragments:** kept via a stub; only 1/1 messages are dispatched in this MVP.
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
 * @brief Minimal orchestrator for ViaText nodes.
 *
 * Typical usage:
 * @code
 * viatext::Core core("NODE1");
 * core.add_message(in_pkg);
 * core.tick(millis());
 * viatext::Package out;
 * while (core.get_message(out)) {
 *   // wrapper sends/uses `out`
 * }
 * @endcode
 */
class Core {
public:
  /// @name Tunable capacities & policy knobs (compile‑time)
  ///@{
  static constexpr size_t INBOX_CAP          = 16;   ///< Max inbound packages queued
  static constexpr size_t OUTBOX_CAP         = 16;   ///< Max outbound packages queued
  static constexpr size_t RECENT_SEQ_CAP     = 64;   ///< Dedupe ring size
  static constexpr uint8_t HOPS_MAX_DEFAULT  = 7;    ///< Default TTL cap
  static constexpr uint8_t FRAG_CAP_DEFAULT  = 8;    ///< Max allowed parts (future reassembly)
  static constexpr uint8_t INFLIGHT_CAP      = 4;    ///< Max in‑flight sequences (future)
  ///@}

  /// Node ID string type (fits your 1..6 char callsign with a bit of headroom).
  using NodeIdStr = etl::string<8>;
  

  
  /**
   * @brief Construct a Core with a node identity.
   * @param node_id Callsign/ID of this node (1..6 uppercase recommended).
   */
  explicit Core(const NodeIdStr& node_id);

  /**
   * @brief Construct a Core with a C-style node identity.
   * @param node_id C-style string (null-terminated, 1..6 chars).
   */
  explicit Core(const char* node_id);


  /// @brief Change the node identity at runtime.
  void set_node_id(const NodeIdStr& node_id);

  /// @brief Change the node identity at runtime.
  void set_node_id(const char* node_id);

  /// @brief Read the node identity.
  const NodeIdStr& get_node_id() const;

  /**
   * @brief Enqueue an inbound Package from any wrapper (LoRa, Linux, Serial).
   * @param pkg The package to enqueue.
   * @return true if enqueued; false if the inbox is full.
   */
  bool add_message(const Package& pkg);

  /**
   * @brief Advance time and process at most one inbound item.
   * @param now_ms Current uptime in milliseconds (monotonic).
   *
   * Behavior:
   *  - Updates internal uptime and tick counter.
   *  - Pops one item from inbox (if any), tries to form a Message,
   *    runs minimal policy (dedupe, fragment stub), and dispatches handlers.
   *  - Any resulting outputs are queued to outbox.
   */
  void tick(uint32_t now_ms);

  /**
   * @brief Dequeue the next outbound Package for wrappers to handle/transmit.
   * @param out_pkg Destination for the next package.
   * @return true if a package was available; false if outbox is empty.
   */
  bool get_message(Package& out_pkg);

  /// @brief Number of ticks performed (monotonic counter).
  uint32_t tick_count() const { return tick_count_; }

  /// @brief Node uptime in milliseconds (64‑bit).
  uint64_t uptime_ms() const { return uptime_ms_; }

  /// @brief Policy getters/setters.
  uint8_t hops_max() const { return hops_max_; }
  void set_hops_max(uint8_t v) { hops_max_ = v; }

  uint8_t frag_cap() const { return frag_cap_; }
  void set_frag_cap(uint8_t v) { frag_cap_ = v; }

private:
  // ---------- internal helpers ----------
  void process_one(uint32_t now_ms);
  void dispatch(const Message& msg);

  // Minimal handlers
  void handle_message(const Message& msg);
  void handle_ping(const Message& msg);
  void handle_ack(const Message& msg);
  void handle_set_id(const Message& msg);

  // Recent-sequence ring helpers
  bool contains_sequence(uint16_t seq) const;
  void push_recent_sequence(uint16_t seq);

  // Fragment placeholder
  void store_fragment_stub(const Message& msg, uint32_t now_ms);

  // Outbound builders (sensible outputs)
  Package make_ack_package(const Message& msg) const;
  Package make_delivered_package(const Message& msg) const;
  Package make_pong_package(const Message& msg) const;
  Package make_ack_event_package(const Message& msg) const;
  Package make_id_set_event(const NodeIdStr& new_id) const;

  // tiny util
  static etl::string<16> to_string_number(uint32_t n);

private:
  // ---------- state ----------
  NodeIdStr node_id_{};
  uint32_t  ticks_{0};          // deprecated alias; use tick_count()
  uint32_t  tick_count_{0};
  uint64_t  uptime_ms_{0};
  uint64_t  last_ms_{0};

  uint8_t   hops_max_{HOPS_MAX_DEFAULT};
  uint8_t   frag_cap_{FRAG_CAP_DEFAULT};

  etl::deque<Package, INBOX_CAP>  inbox_;
  etl::deque<Package, OUTBOX_CAP> outbox_;
  etl::deque<uint16_t, RECENT_SEQ_CAP> recent_seqs_;
};

} // namespace viatext

#endif // VIATEXT_CORE_HPP
