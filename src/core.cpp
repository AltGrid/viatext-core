/**
 * @file core.cpp
 * @brief Implementation of ViaText Core — minimal orchestrator and handlers.
 */

#include "viatext/core.hpp"

namespace viatext {

// ---------- public ----------

Core::Core(const NodeIdStr& node_id)
: node_id_(node_id) {
  // nothing else
}

Core::Core(const char* node_id) {
  NodeIdStr tmp;
  if (node_id) {
    // safe, bounded copy into etl::string<8>
    size_t n = 0;
    while (node_id[n] && n < tmp.max_size()) { tmp += node_id[n++]; }
  }
  node_id_ = tmp;
}

void Core::set_node_id(const NodeIdStr& node_id) {
  node_id_ = node_id;
}

void Core::set_node_id(const char* node_id) {
  NodeIdStr tmp;
  if (node_id) {
    size_t n = 0;
    while (node_id[n] && n < tmp.max_size()) { tmp += node_id[n++]; }
  }
  node_id_ = tmp;
}

const Core::NodeIdStr& Core::get_node_id() const {
  return node_id_;
}

bool Core::add_message(const Package& pkg) {
  if (inbox_.full()) return false;
  inbox_.push_back(pkg);
  return true;
}

void Core::tick(uint32_t now_ms32) {
  const uint64_t now_ms = static_cast<uint64_t>(now_ms32);

  if (last_ms_ == 0) {
    last_ms_ = now_ms;
  }
  const uint64_t delta = (now_ms >= last_ms_) ? (now_ms - last_ms_) : 0;
  uptime_ms_ += delta;
  last_ms_ = now_ms;
  ++tick_count_;

  process_one(static_cast<uint32_t>(now_ms)); // pass 32‑bit for helpers if needed
}

bool Core::get_message(Package& out_pkg) {
  if (outbox_.empty()) return false;
  out_pkg = outbox_.front();
  outbox_.pop_front();
  return true;
}

// ---------- private: process & dispatch ----------

void Core::process_one(uint32_t now_ms) {
  (void)now_ms;

  if (inbox_.empty()) return;

  Package in_pkg = inbox_.front();
  inbox_.pop_front();

  // Form a Message; if invalid, drop silently (MVP).
  Message msg(in_pkg);
  if (!msg.is_valid()) {
    return;
  }

  // Policy guard: hops (soft guard; wrappers usually enforce)
  if (msg.hops() > hops_max_) {
    return;
  }

  // Fragment policy (MVP): keep fragments but do not dispatch until complete (1 of 1).
  if (msg.total() > 1) {
    store_fragment_stub(msg, now_ms);
    return;
  }

  // Dedupe whole messages by sequence.
  if (contains_sequence(msg.sequence())) {
    return;
  }
  push_recent_sequence(msg.sequence());

  // Dispatch by args (flags preserved exactly).
  dispatch(msg);
}

void Core::dispatch(const Message& msg) {
  // Order is explicit; only first matching branch runs in MVP.

  if (msg.flag("-m")) {          // standard message
    handle_message(msg);
    return;
  }

  if (msg.flag("-p")) {          // ping
    handle_ping(msg);
    return;
  }

  if (msg.flag("-ack")) {        // ack notice
    handle_ack(msg);
    return;
  }

  if (msg.flag("--set-id")) {    // set node id
    handle_set_id(msg);
    return;
  }

  // Unknown directive: ignore in MVP (future: emit "-unknown" event).
}

// ---------- private: handlers ----------

void Core::handle_message(const Message& msg) {
  // Deliver only if addressed to this node; forwarding is wrapper/radio policy.
  if (msg.to() == node_id_) {
    if (msg.requests_ack()) {
      Package ack = make_ack_package(msg);
      if (!outbox_.full()) outbox_.push_back(ack);
    }
    Package delivered = make_delivered_package(msg);
    if (!outbox_.full()) outbox_.push_back(delivered);
  }
}

void Core::handle_ping(const Message& msg) {
  Package pong = make_pong_package(msg);
  if (!outbox_.full()) outbox_.push_back(pong);
}

void Core::handle_ack(const Message& msg) {
  // Surface as a simple event for now.
  Package evt = make_ack_event_package(msg);
  if (!outbox_.full()) outbox_.push_back(evt);
}

void Core::handle_set_id(const Message& msg) {
  // MVP: take body text as new id if non-empty.
  if (!msg.text().empty()) {
    NodeIdStr new_id = msg.text().c_str();
    node_id_ = new_id;
    Package conf = make_id_set_event(new_id);
    if (!outbox_.full()) outbox_.push_back(conf);
  }
}

// ---------- private: recent sequence ring ----------

bool Core::contains_sequence(uint16_t seq) const {
  for (auto s : recent_seqs_) {
    if (s == seq) return true;
  }
  return false;
}

void Core::push_recent_sequence(uint16_t seq) {
  if (recent_seqs_.full()) {
    recent_seqs_.pop_front();
  }
  recent_seqs_.push_back(seq);
}

// ---------- private: fragments (stub) ----------

void Core::store_fragment_stub(const Message& msg, uint32_t now_ms) {
  (void)msg;
  (void)now_ms;
  // Placeholder for future reassembly table.
  // Intentionally does nothing in MVP.
}

// ---------- private: outbound builders ----------

Package Core::make_ack_package(const Message& msg) const {
  // Mirror sequence, 1/1, set ACK flag, clear REQ_ACK; keep hops as-is (or clamp).
  MessageID id(msg.sequence(), 0, 1, msg.hops(), 0);
  id.set_is_acknowledgment(true);
  id.set_request_acknowledgment(false);

  Message out_msg(id, node_id_.c_str(), msg.from().c_str(), "ACK");

  Package out;
  auto stamp = out_msg.to_payload_stamp_copy();
  out.payload = stamp;
  out.args.set_flag("-ack");
  out.args.set("--to", msg.from().c_str());
  out.args.set("--from", node_id_.c_str());
  return out;
}

Package Core::make_delivered_package(const Message& msg) const {
  Package out;
  out.payload = msg.to_payload_stamp_copy();
  out.args.set_flag("-r"); // received
  out.args.set("--to", node_id_.c_str());
  out.args.set("--from", msg.from().c_str());
  return out;
}

Package Core::make_pong_package(const Message& msg) const {
  // Reuse sequence (policy choice) and flags; ensure it's a clean 1/1 message.
  MessageID id(msg.sequence(), 0, 1, msg.hops(), msg.flags());
  Message out_msg(id, node_id_.c_str(), msg.from().c_str(), "PONG");

  Package out;
  out.payload = out_msg.to_payload_stamp_copy();
  out.args.set_flag("-pong");
  out.args.set("--to", msg.from().c_str());
  out.args.set("--from", node_id_.c_str());
  return out;
}

Package Core::make_ack_event_package(const Message& msg) const {
  Package out;
  out.payload = msg.to_payload_stamp_copy();
  out.args.set_flag("-ack_event");

  // Add "--seq" for convenience.
  auto seq_str = to_string_number(msg.sequence());
  out.args.set("--seq", seq_str.c_str());
  return out;
}

Package Core::make_id_set_event(const NodeIdStr& new_id) const {
  Package out;
  out.payload = "ID_SET~";
  out.payload += new_id;

  out.args.set_flag("-id_set");
  out.args.set("--node", new_id.c_str());
  return out;
}

// ---------- private: tiny util ----------

etl::string<16> Core::to_string_number(uint32_t n) {
  etl::string<16> s;
  // Simple decimal conversion without heap.
  char buf[16];
  int idx = 0;

  if (n == 0) {
    s += '0';
    return s;
  }

  // Build reversed
  while (n > 0 && idx < static_cast<int>(sizeof(buf))) {
    uint32_t digit = n % 10;
    buf[idx++] = static_cast<char>('0' + digit);
    n /= 10;
  }
  // Reverse into s
  for (int i = idx - 1; i >= 0; --i) s += buf[i];
  return s;
}

} // namespace viatext
