// -----------------------------------------------------------------------------
// core.cpp — Implementation of ViaText Core
//
// This file contains the *implementation details* for the Core class
// declared in `core.hpp`.
//
// API & field descriptions:
//   see include/viatext/core.hpp
//
// Runnable examples & usage tests:
//   see tests/ (e.g., tests/test-cli/, tests/test-message-id/)
//
// NOTE: This file focuses on *how* and *why* the logic is implemented,
// including internal policies, guard conditions, and decisions.
// External-facing API contracts live in the header.
// -----------------------------------------------------------------------------
#include "viatext/core.hpp"

namespace viatext {

// ---------- public ----------

Core::Core(const NodeIdStr& node_id)
: node_id_(node_id) {             // directly initialize member with given ID string
  // nothing else                 // no extra setup needed for this constructor
}

Core::Core(const char* node_id) {
  NodeIdStr tmp;                  // temporary fixed-size ETL string for safe copying
  if (node_id) {                  // guard against null pointer input
    // safe, bounded copy into etl::string<8>
    size_t n = 0;                  // track number of characters copied
    while (node_id[n]              // stop if end of C-string reached
           && n < tmp.max_size())  // stop if max length of ETL string reached
    {
      tmp += node_id[n++];         // append char, then increment index
    }
  }
  node_id_ = tmp;                  // assign validated/cropped string to member
}


void Core::set_node_id(const NodeIdStr& node_id) {
  node_id_ = node_id;                 // directly assign ETL string to member
}

void Core::set_node_id(const char* node_id) {
  NodeIdStr tmp;                       // temporary ETL string for safe copy
  if (node_id) {                       // guard against null pointer
    size_t n = 0;                       // track number of characters copied
    while (node_id[n]                   // stop if end of C-string reached
           && n < tmp.max_size())       // or ETL string capacity reached
    {
      tmp += node_id[n++];              // append char, increment index
    }
  }
  node_id_ = tmp;                       // assign validated/cropped string to member
}

const Core::NodeIdStr& Core::get_node_id() const {
  return node_id_;                      // return current node ID (read-only reference)
}

// add_message() — Try to enqueue a package in the inbox; fail if full.
bool Core::add_message(const Package& pkg) {
  if (inbox_.full()) return false;      // reject if inbox is at capacity
  inbox_.push_back(pkg);                // enqueue message for processing
  return true;                          // successfully added
}

// tick() — Advance uptime counters and process one incoming package.
void Core::tick(uint32_t now_ms32) {
  const uint64_t now_ms = static_cast<uint64_t>(now_ms32); // widen to 64-bit for safe math

  if (last_ms_ == 0) {              // first tick call? initialize last_ms_
    last_ms_ = now_ms;
  }

  // calculate elapsed time since last tick (handle wraparound safely)
  const uint64_t delta = (now_ms >= last_ms_) ? (now_ms - last_ms_) : 0;

  uptime_ms_ += delta;              // accumulate uptime in milliseconds
  last_ms_ = now_ms;                 // update last seen tick time
  ++tick_count_;                     // count total tick calls

  process_one(static_cast<uint32_t>(now_ms)); // process 1 inbox item (pass 32-bit if helper expects it)
}

// get_message() — Try to dequeue the next outbound package; fail if empty.
bool Core::get_message(Package& out_pkg) {
  if (outbox_.empty()) return false;  // no messages ready to send
  out_pkg = outbox_.front();          // copy oldest package
  outbox_.pop_front();                // remove it from queue
  return true;                        // success
}

// ---------- private: process & dispatch ----------

// process_one() — Pull the next inbox package, apply policy checks, and dispatch if valid.
void Core::process_one(uint32_t now_ms) {
  (void)now_ms;  // PRE: suppress unused warning; some future policies may need timestamp

  // PRE: check if there's anything to process
  if (inbox_.empty()) return;

  Package in_pkg = inbox_.front();     // PRE: get oldest queued package
  inbox_.pop_front();                  // remove from inbox

  // PRE: construct a Message object from the package
  // DROP if invalid — silent fail for MVP
  Message msg(in_pkg);
  if (!msg.is_valid()) {
    return;
  }

  // POLICY: enforce hop limit (soft guard; transport layer may enforce harder limits)
  if (msg.hops() > hops_max_) {
    return;
  }

  // POLICY: fragment handling — store but don't dispatch until all parts arrive
  if (msg.total() > 1) {
    store_fragment_stub(msg, now_ms);
    return;
  }

  // POLICY: deduplicate based on sequence number
  if (contains_sequence(msg.sequence())) {
    return;
  }
  push_recent_sequence(msg.sequence()); // mark this sequence as seen

  // DISPATCH: hand off to the appropriate handler based on message flags
  dispatch(msg);
}


// -----------------------------------------------------------------------------
// dispatch() — Select a single handler based on flags in priority order.
// PRE:   `msg` is already validated by process_one() (hops, fragments, dedupe).
// POLICY:
//   - First-match-wins (no fallthrough). Order below defines precedence.
//   - Only one handler runs per message in MVP (keeps behavior predictable).
//   - Unknown flags are ignored silently (future: emit "-unknown" event).
// ASSUMPTIONS:
//   - msg.flag("<token>") is a cheap, deterministic check (no heap).
//   - Flags are preserved exactly from transport wrappers.
// TRADEOFFS:
//   - Simplicity over flexibility: explicit `if` chain instead of a jump table.
//   - Edit-friendly: easiest place to tweak precedence during development.
// TODO:
//   - Consider a table-driven dispatch for extensibility and testability.
//   - Add tracing hooks (e.g., "-trace") to surface which branch fired.
//   - Decide on multi-flag semantics (run-many vs run-first) if needed later.
// -----------------------------------------------------------------------------
void Core::dispatch(const Message& msg) {
  // NOTE: Order is explicit; only first matching branch runs in MVP.

  // DISPATCH: standard message payload delivery (highest precedence)
  if (msg.flag("-m")) {          // standard message
    handle_message(msg);
    return;                      // first-match-wins
  }

  // DISPATCH: ping request → reply with pong
  if (msg.flag("-p")) {          // ping
    handle_ping(msg);
    return;
  }

  // DISPATCH: acknowledgment surfaced as an event
  if (msg.flag("-ack")) {        // ack notice
    handle_ack(msg);
    return;
  }

  // DISPATCH: local configuration — set node identity
  if (msg.flag("--set-id")) {    // set node id
    handle_set_id(msg);
    return;
  }

  // NOTE: Unknown directive: ignore in MVP (future: emit "-unknown" event).
}


// ---------- private: handlers ----------

// -----------------------------------------------------------------------------
// handle_message() — Deliver a standard message locally, optionally ACK.
// PRE:
//   - Called only after dispatch() matched "-m".
//   - Message already validated (hops/fragment/dedupe) by process_one().
// POLICY:
//   - Only deliver to self; forwarding is transport/wrapper responsibility.
//   - If sender requested ACK, enqueue an ACK before the delivery event.
// OUT:
//   - May push up to two packages to outbox_: an ACK and a "-r" delivered event.
// NOTE:
//   - Outbox capacity is checked defensively; silent drop if full (MVP).
//   - Future: consider signaling backpressure if outbox_ is full.
// -----------------------------------------------------------------------------
void Core::handle_message(const Message& msg) {
  // Deliver only if addressed to this node; forwarding is wrapper/radio policy.
  if (msg.to() == node_id_) {
    if (msg.requests_ack()) {
      Package ack = make_ack_package(msg);             // build ACK mirroring seq/from
      if (!outbox_.full()) outbox_.push_back(ack);     // OUT: enqueue ACK if space
    }
    Package delivered = make_delivered_package(msg);    // build local "received" event
    if (!outbox_.full()) outbox_.push_back(delivered);  // OUT: enqueue delivery event
  }
}


// -----------------------------------------------------------------------------
// handle_ping() — Respond to a "-p" ping with a "-pong" package.
// PRE:   Called only after dispatch() matched "-p".
// POLICY:
//   - Always respond; no hop or sequence changes here (handled upstream).
// OUT:   One pong package pushed to outbox_ if space is available.
// -----------------------------------------------------------------------------
void Core::handle_ping(const Message& msg) {
  Package pong = make_pong_package(msg);               // build pong reply from msg
  if (!outbox_.full()) outbox_.push_back(pong);         // OUT: enqueue pong if space
}

// -----------------------------------------------------------------------------
// handle_ack() — Surface an acknowledgment as an event package.
// PRE:   Called only after dispatch() matched "-ack".
// POLICY:
//   - ACKs are surfaced, not acted upon here (no resend suppression yet).
// OUT:   One ack_event package pushed to outbox_ if space is available.
// -----------------------------------------------------------------------------
void Core::handle_ack(const Message& msg) {
  // Surface as a simple event for now.
  Package evt = make_ack_event_package(msg);            // build ack_event from msg
  if (!outbox_.full()) outbox_.push_back(evt);          // OUT: enqueue ack_event if space
}

// -----------------------------------------------------------------------------
// handle_set_id() — Change this node's ID via "--set-id" directive.
// PRE:   Called only after dispatch() matched "--set-id".
// POLICY:
//   - Ignore if body text is empty.
//   - Take body text exactly as provided (bounded by NodeIdStr size).
// OUT:   One id_set event package pushed to outbox_ if space is available.
// NOTE:  Actual persistence of node_id_ beyond runtime is wrapper responsibility.
// -----------------------------------------------------------------------------
void Core::handle_set_id(const Message& msg) {
  // MVP: take body text as new id if non-empty.
  if (!msg.text().empty()) {
    NodeIdStr new_id = msg.text().c_str();              // bounded copy into NodeIdStr
    node_id_ = new_id;                                  // update current node ID
    Package conf = make_id_set_event(new_id);           // build confirmation event
    if (!outbox_.full()) outbox_.push_back(conf);       // OUT: enqueue confirmation if space
  }
}


// ---------- private: recent sequence ring ----------

// -----------------------------------------------------------------------------
// contains_sequence() — Check if a given sequence number is in the recent ring.
// PRE:   seq is a message sequence number to check for deduplication.
// POLICY:
//   - Linear search; ring buffer is small so O(n) is fine for MVP.
//   - Equality match only; no wraparound math yet.
// OUT:   true if found, false otherwise.
// -----------------------------------------------------------------------------
bool Core::contains_sequence(uint16_t seq) const {
  for (auto s : recent_seqs_) {        // iterate over stored recent sequences
    if (s == seq) return true;         // match found → duplicate
  }
  return false;                        // no match → new sequence
}

// -----------------------------------------------------------------------------
// push_recent_sequence() — Add a sequence number to the recent ring.
// PRE:   seq is a message sequence number already processed.
// POLICY:
//   - If full, drop oldest before adding new (FIFO behavior).
//   - No deduplication here — caller must ensure it's new.
// -----------------------------------------------------------------------------
void Core::push_recent_sequence(uint16_t seq) {
  if (recent_seqs_.full()) {           // ring at capacity?
    recent_seqs_.pop_front();          // drop oldest entry
  }
  recent_seqs_.push_back(seq);         // append new sequence number
}

// ---------- private: fragments (stub) ----------

// -----------------------------------------------------------------------------
// store_fragment_stub() — Placeholder for fragment reassembly table logic.
// PRE:   msg may be a partial message (total() > 1).
// POLICY:
//   - Currently does nothing; MVP ignores partials.
// NOTE:
//   - Arguments are unused for now; kept to satisfy call sites.
//   - Will eventually track fragments and assemble when all parts are in.
// -----------------------------------------------------------------------------
void Core::store_fragment_stub(const Message& msg, uint32_t now_ms) {
  (void)msg;                           // suppress unused parameter warning
  (void)now_ms;                        // suppress unused parameter warning
  // Placeholder for future reassembly table.
  // Intentionally does nothing in MVP.
}

// ---------- private: outbound builders ----------

// -----------------------------------------------------------------------------
// make_ack_package() — Build an ACK reply to a given message.
// PRE:
//   - msg is a valid, already-processed message requesting or expecting ACK.
// POLICY:
//   - Mirror the incoming sequence number (msg.sequence()).
//   - Force total parts to 1/1.
//   - Set ACK flag, clear REQ_ACK flag.
//   - Preserve hops as-is (or clamp later if needed).
// OUT:
//   - Returns a Package containing the ACK payload and routing args.
// NOTE:
//   - Caller must ensure outbox_ has space before pushing this.
// -----------------------------------------------------------------------------
Package Core::make_ack_package(const Message& msg) const {
  // Mirror sequence, 1/1, set ACK flag, clear REQ_ACK; keep hops as-is (or clamp).
  MessageID id(msg.sequence(), 0, 1, msg.hops(), 0);
  id.set_is_acknowledgment(true);                // mark as acknowledgment
  id.set_request_acknowledgment(false);          // remove request-for-ack flag

  Message out_msg(id, node_id_.c_str(),          // from: this node
                  msg.from().c_str(),            // to: original sender
                  "ACK");                        // minimal body text

  Package out;
  auto stamp = out_msg.to_payload_stamp_copy();  // serialize header/body to payload string
  out.payload = stamp;
  out.args.set_flag("-ack");                     // add ack flag to arg list
  out.args.set("--to", msg.from().c_str());      // explicit recipient
  out.args.set("--from", node_id_.c_str());      // explicit sender
  return out;
}


// -----------------------------------------------------------------------------
// make_delivered_package() — Build a local "message received" event.
// PRE:
//   - msg is a valid message addressed to this node and accepted for delivery.
// POLICY:
//   - Use the exact payload from the original message (no modifications).
//   - Add "-r" flag to indicate receipt.
//   - Explicitly set "--to" and "--from" for clarity.
// OUT:
//   - Returns a Package representing the delivery event.
// NOTE:
//   - Caller must ensure outbox_ has space before pushing this.
// -----------------------------------------------------------------------------
Package Core::make_delivered_package(const Message& msg) const {
  Package out;
  out.payload = msg.to_payload_stamp_copy();       // copy original payload as-is
  out.args.set_flag("-r");                         // received indicator
  out.args.set("--to", node_id_.c_str());          // local node ID as recipient
  out.args.set("--from", msg.from().c_str());      // original sender
  return out;
}


// -----------------------------------------------------------------------------
// make_pong_package() — Build a PONG reply to a received ping.
// PRE:
//   - msg is a valid message with "-p" flag (ping).
// POLICY:
//   - Reuse the incoming sequence number (keeps ping/pong pair linked).
//   - Copy flags from original msg (except ensure it's a single-part message).
//   - Preserve hop count as-is.
//   - Body text is fixed to "PONG".
// OUT:
//   - Returns a Package containing the pong payload and routing args.
// NOTE:
//   - Caller must ensure outbox_ has space before pushing this.
// -----------------------------------------------------------------------------
Package Core::make_pong_package(const Message& msg) const {
  // Reuse sequence (policy choice) and flags; ensure it's a clean 1/1 message.
  MessageID id(msg.sequence(), 0, 1, msg.hops(), msg.flags());
  Message out_msg(id, node_id_.c_str(),             // from: this node
                  msg.from().c_str(),               // to: original sender
                  "PONG");                          // fixed body

  Package out;
  out.payload = out_msg.to_payload_stamp_copy();    // serialize to payload string
  out.args.set_flag("-pong");                       // mark as pong
  out.args.set("--to", msg.from().c_str());         // recipient = original sender
  out.args.set("--from", node_id_.c_str());         // sender = this node
  return out;
}


// -----------------------------------------------------------------------------
// make_ack_event_package() — Build an event package from an incoming ACK.
// PRE:
//   - msg is a valid acknowledgment message.
// POLICY:
//   - Preserve the exact payload from the ACK.
//   - Add "-ack_event" flag to distinguish from a normal ack response.
//   - Include "--seq" with the original sequence number for reference.
// OUT:
//   - Returns a Package representing the ACK as an event.
// -----------------------------------------------------------------------------
Package Core::make_ack_event_package(const Message& msg) const {
  Package out;
  out.payload = msg.to_payload_stamp_copy();       // copy ACK payload as-is
  out.args.set_flag("-ack_event");                 // mark as acknowledgment event

  // Add "--seq" for convenience (string form of sequence number).
  auto seq_str = to_string_number(msg.sequence());
  out.args.set("--seq", seq_str.c_str());          // attach sequence number arg
  return out;
}

// -----------------------------------------------------------------------------
// make_id_set_event() — Build an event indicating the node ID was changed.
// PRE:
//   - new_id is the updated NodeIdStr (already validated).
// POLICY:
//   - Encode event as "ID_SET~<new_id>" in payload.
//   - Add "-id_set" flag for easy filtering.
//   - Include "--node" arg with the new ID.
// OUT:
//   - Returns a Package representing the ID change event.
// -----------------------------------------------------------------------------
Package Core::make_id_set_event(const NodeIdStr& new_id) const {
  Package out;
  out.payload = "ID_SET~";                         // event prefix
  out.payload += new_id;                           // append new node ID

  out.args.set_flag("-id_set");                    // mark as ID set event
  out.args.set("--node", new_id.c_str());          // include new ID as arg
  return out;
}


// ---------- private: tiny util ----------

// -----------------------------------------------------------------------------
// to_string_number() — Convert an unsigned integer to ETL string (decimal).
// PRE:
//   - n is a non-negative integer to be converted.
// POLICY:
//   - No heap allocation; uses fixed-size ETL string<16>.
//   - Output is decimal ASCII, no padding or formatting beyond digits.
//   - Stops if buffer limit reached (prevents overflow).
// OUT:
//   - Returns ETL string containing decimal representation of n.
// NOTE:
//   - This avoids sprintf() to remain safe on embedded systems without stdlib I/O.
// -----------------------------------------------------------------------------
etl::string<16> Core::to_string_number(uint32_t n) {
  etl::string<16> s;
  // Simple decimal conversion without heap.
  char buf[16];                                    // temp buffer for reversed digits
  int idx = 0;

  if (n == 0) {                                    // special-case zero
    s += '0';
    return s;
  }

  // Build reversed string of digits
  while (n > 0 && idx < static_cast<int>(sizeof(buf))) {
    uint32_t digit = n % 10;                       // isolate least significant digit
    buf[idx++] = static_cast<char>('0' + digit);   // store ASCII digit
    n /= 10;                                       // drop the digit
  }

  // Reverse into s (to correct digit order)
  for (int i = idx - 1; i >= 0; --i) s += buf[i];
  return s;
}

/**
 * @brief Create a new message with a unique sequence number.
 */
Message Core::create_new_message() {
  // Allocate a sequence that's not currently in the recent ring.
  // Bound the attempts to avoid any theoretical pathological case.
  uint16_t seq = 0;
  bool ok = false;

  for (size_t i = 0; i < RECENT_SEQ_CAP + 1; ++i) {
    seq = seq_.allocate();
    if (!contains_sequence(seq)) { ok = true; break; }
  }

  // If all attempts collided (extremely unlikely), we still proceed with the last seq.
  // The message will be valid; worst case it might be dropped by dedupe if reused too soon.
  (void)ok;

  return Message::get_new_message_template(seq);
}


} // namespace viatext
