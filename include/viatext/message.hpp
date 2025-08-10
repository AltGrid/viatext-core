/**
 * @file message.hpp
 * @brief ViaText Message — unified view over routing header, metadata args, and text body.
 *
 * The `Message` class is a thin, ingress-agnostic wrapper that contains:
 *   - a `MessageID`   (the 5-byte routing/flags/fragment header),
 *   - a `Package`     (metadata arguments + expanded 0..255B text),
 *   - parsed stamp parts: `from`, `to`, `data` (body).
 *
 * It parses the canonical ViaText payload stamp once:
 *     <hex10>~<from>~<to>~<data>
 * where <hex10> is the 10-hex-character representation of the 5-byte MessageID
 * (optionally "0x" prefix is permitted if you choose to enable it).
 *
 * ### Design goals
 * - Keep the core simple: it can call `message.sequence()`, `message.from()`,
 *   `message.get_arg("-rssi")`, `message.text()`, etc., without managing sub-objects.
 * - Zero dynamic allocation (ETL only), predictable footprints, beginner-friendly API.
 * - No argument renaming or dash stripping — keys are preserved exactly.
 *
 * ### Common construction paths
 * - From a wrapper-provided `Package` (after reassembly): `Message m(pkg);`
 *   The constructor parses `<hex10>~from~to~data` inside `pkg.payload`.
 * - From scratch (internal generation): default-construct and set fields via setters,
 *   or pass a `MessageID` plus `from/to/data` strings.
 *
 * ### Validity & policy
 * - `is_valid()` indicates whether the stamp/header parsed cleanly.
 * - The class does not perform fragmentation/reassembly; that must happen upstream.
 *
 * @authors
 * @author Leo
 * @author ChatGPT
 */

#pragma once
#include "message_id.hpp"
#include "package.hpp"
#include "etl/string.h"
#include <stdint.h>
#include <stddef.h>

namespace viatext {

/// Max lengths for parsed stamp fields (conservative defaults).
static constexpr size_t VT_FROM_MAX = 8;   ///< fits your 1..6 char callsigns with headroom
static constexpr size_t VT_TO_MAX   = 8;   ///< same as FROM
static constexpr size_t VT_BODY_MAX = 255; ///< body/data lives within Package::payload budget

using FromStr = etl::string<VT_FROM_MAX>;
using ToStr   = etl::string<VT_TO_MAX>;
using BodyStr = etl::string<VT_BODY_MAX>;

/// Result codes for constructing / parsing a Message.
enum class MessageStatus : uint8_t {
  Ok = 0,
  TooShort,
  MissingTildes,
  BadHeaderHex,
  HeaderPolicy,   // e.g., invalid hops/flags/part/total ranges
  Overflow,       // assembled payload would exceed 255
};

class Message {
public:
  // -------- Constructors --------

  /// Default: empty message, invalid until fields are set.
  Message()
  : status_(MessageStatus::TooShort) {
    // leave id_ zeroed by default ctor; pkg_.payload empty
  }

  /// Build from a Package. Parses pkg.payload as "<hex10>~from~to~data".
  explicit Message(const Package& pkg)
  : pkg_(pkg) {
    status_ = parse_from_payload_stamp();
  }

  /// Build from Package + explicit MessageID (skips hex extraction for id).
  Message(const Package& pkg, const MessageID& id)
  : id_(id), pkg_(pkg) {
    status_ = parse_from_payload_after_header_known();
  }

  /// Build from components (internal generation path).
  Message(const MessageID& id, const char* from, const char* to, const char* data)
  : id_(id) {
    set_from(from);
    set_to(to);
    set_text(data);
    // payload is not auto-built; caller can retrieve to_payload_stamp()
    status_ = MessageStatus::Ok;
  }

  // -------- Status / validity --------

  /// True if the header and stamp fields parsed cleanly (Ok).
  bool is_valid() const { return status_ == MessageStatus::Ok; }

  /// Detailed status for logging/tests.
  MessageStatus status() const { return status_; }

  // -------- Getters for routing header (MessageID) --------

  uint16_t sequence()   const { return id_.sequence; }
  uint8_t  part()       const { return id_.part; }
  uint8_t  total()      const { return id_.total; }
  uint8_t  hops()       const { return static_cast<uint8_t>(id_.hops & 0x0F); }
  uint8_t  flags()      const { return static_cast<uint8_t>(id_.flags & 0x0F); }

  bool requests_ack()   const { return id_.requests_acknowledgment(); }
  bool is_ack()         const { return id_.is_acknowledgment(); }
  bool is_encrypted()   const { return id_.is_encrypted(); }

  // -------- Setters for routing header --------

  void set_sequence(uint16_t s) { id_.sequence = s; }
  void set_part(uint8_t p)      { id_.part = p; }
  void set_total(uint8_t t)     { id_.total = t; }
  void set_hops(uint8_t h)      { id_.hops = (h & 0x0F); }
  void set_flags(uint8_t f)     { id_.flags = (f & 0x0F); }

  void set_request_ack(bool on) { id_.set_request_acknowledgment(on); }
  void set_is_ack(bool on)      { id_.set_is_acknowledgment(on); }
  void set_is_encrypted(bool on){ id_.set_is_encrypted(on); }

  /// Increment hops (with nibble clamp).
  void bump_hops() { set_hops(static_cast<uint8_t>((id_.hops + 1) & 0x0F)); }

  // -------- Getters for args (delegated to Package) --------

  bool has_arg(const char* key) const { return pkg_.args.has(key); }
  const ValStr* get_arg(const char* key) const { return pkg_.args.get(key); }
  bool flag(const char* key) const { return pkg_.flag(key); }

  // -------- Setters for args (delegated) --------

  bool set_arg(const char* key, const char* val) { return pkg_.args.set(key, val); }
  bool set_flag(const char* key)                 { return pkg_.args.set_flag(key); }
  bool remove_arg(const char* key)               { return pkg_.args.remove(key); }

  // -------- Getters for parsed stamp parts --------

  const FromStr& from() const { return from_; }
  const ToStr&   to()   const { return to_; }
  const BodyStr& text() const { return data_; }   // "data" a.k.a. message body

  // -------- Setters for stamp parts --------

  void set_from(const char* s) { assign_trim(from_, s); }
  void set_to(const char* s)   { assign_trim(to_,   s); }
  void set_text(const char* s) { assign_trim(data_, s); }

  // -------- Package access (rarely needed by the core) --------

  const Package& package() const { return pkg_; }
  Package&       package()       { return pkg_; }

  // -------- Assembly helpers --------

  /**
   * @brief Assemble "<hex10>~from~to~data" into `out`.
   * @return MessageStatus::Ok on success; Overflow if >255 bytes.
   */
  MessageStatus to_payload_stamp(Text255& out) const {
    // hex string from id (10 chars)
    etl::string<11> hex = id_.to_hex_string(); // 10 chars
    // compute total length conservatively and check
    size_t total_len = hex.size() + 1 + from_.size() + 1 + to_.size() + 1 + data_.size();
    if (total_len > out.max_size()) return MessageStatus::Overflow;

    out.clear();
    out += hex;
    out += '~';
    out += from_;
    out += '~';
    out += to_;
    out += '~';
    out += data_;
    return MessageStatus::Ok;
  }

  /// Convenience: return a transient assembled string (copy).
  Text255 to_payload_stamp_copy() const {
    Text255 out;
    (void)to_payload_stamp(out);
    return out;
  }

private:
  MessageID  id_{};
  Package    pkg_{};
  FromStr    from_{};
  ToStr      to_{};
  BodyStr    data_{};
  MessageStatus status_{MessageStatus::TooShort};

  // --- Parsing helpers ---

  static inline bool is_hex_char(char c) {
    return (c >= '0' && c <= '9') ||
           (c >= 'A' && c <= 'F') ||
           (c >= 'a' && c <= 'f');
  }

    template <size_t N>
    static void assign_trim(etl::string<N>& dst, const char* src) {
        dst.clear();
        if (!src) return;
        // trim leading/trailing spaces/tabs (simple + deterministic)
        const char* s = src;
        while (*s == ' ' || *s == '\t') ++s;
        const char* e = s;
        while (*e) ++e;
        while (e > s && (e[-1] == ' ' || e[-1] == '\t')) --e;
        for (const char* p = s; p < e && dst.size() < dst.max_size(); ++p) dst += *p;
    }

  /**
   * @brief Parse pkg_.payload as "<hex10>~from~to~data" and fill id_/from_/to_/data_.
   * Expects a reassembled logical payload (no fragments). Accepts uppercase/lowercase hex.
   */
  MessageStatus parse_from_payload_stamp() {
    const Text255& pl = pkg_.payload;
    if (pl.empty()) return MessageStatus::TooShort;

    // token 0: hex id (10 chars) possibly prefixed by "0x"
    size_t i = 0;
    if (pl.size() >= 2 && pl[0] == '0' && (pl[1] == 'x' || pl[1] == 'X')) {
    i = 2; // skip "0x"
    }

    // read up to '~' as id token
    etl::string<12> idtok; // up to "0x"+10, but we'll store the 10 hex for conversion
    for (; i < pl.size(); ++i) {
      if (pl[i] == '~') break;
      if (idtok.size() < idtok.max_size()) idtok += pl[i];
    }
    if (i >= pl.size() || pl[i] != '~') return MessageStatus::MissingTildes;

    // normalize: if it still has "0x", strip it
    const char* idc = idtok.c_str();
    if ((idtok.size() >= 2) && idc[0] == '0' && (idc[1] == 'x' || idc[1] == 'X')) {
      // shift left by 2 — or simply advance pointer below without modifying string
      idc += 2;
    }

    // Expect exactly 10 hex chars
    size_t hexlen = 0;
    while (idc[hexlen]) ++hexlen;
    if (hexlen != 10) return MessageStatus::BadHeaderHex;
    for (size_t k = 0; k < 10; ++k) if (!is_hex_char(idc[k])) return MessageStatus::BadHeaderHex;

    // Build id_ from hex
    id_ = MessageID(idc);

    // Parse from / to / data (split next two '~', rest is body)
    size_t pos = i + 1;
    // FROM
    FromStr f; while (pos < pl.size() && pl[pos] != '~' && f.size() < f.max_size()) { f += pl[pos++]; }
    if (pos >= pl.size() || pl[pos] != '~') return MessageStatus::MissingTildes;
    ++pos;
    // TO
    ToStr t; while (pos < pl.size() && pl[pos] != '~' && t.size() < t.max_size()) { t += pl[pos++]; }
    if (pos > pl.size()) return MessageStatus::MissingTildes;
    // DATA (may be empty; may include '~' beyond the third delimiter — keep rest)
    BodyStr b;
    if (pos < pl.size() && pl[pos] == '~') ++pos;
    while (pos < pl.size() && b.size() < b.max_size()) { b += pl[pos++]; }

    from_ = f;
    to_   = t;
    data_ = b;

    // basic nibble guards (policy-friendly, non-fatal if you prefer)
    id_.hops  &= 0x0F;
    id_.flags &= 0x0F;

    // minimal header sanity
    if (id_.total == 0 || id_.part >= id_.total) return MessageStatus::HeaderPolicy;

    return MessageStatus::Ok;
  }

  /// Variant used when id_ is already known; parses only "~from~to~data".
  MessageStatus parse_from_payload_after_header_known() {
    const Text255& pl = pkg_.payload;
    if (pl.empty()) return MessageStatus::TooShort;

    // Find first '~' (end of header token we won't validate here)
    size_t i = 0;
    while (i < pl.size() && pl[i] != '~') ++i;
    if (i >= pl.size()) return MessageStatus::MissingTildes;

    size_t pos = i + 1;
    // FROM
    FromStr f; while (pos < pl.size() && pl[pos] != '~' && f.size() < f.max_size()) { f += pl[pos++]; }
    if (pos >= pl.size() || pl[pos] != '~') return MessageStatus::MissingTildes;
    ++pos;
    // TO
    ToStr t; while (pos < pl.size() && pl[pos] != '~' && t.size() < t.max_size()) { t += pl[pos++]; }
    if (pos > pl.size()) return MessageStatus::MissingTildes;
    // DATA
    BodyStr b; if (pos < pl.size() && pl[pos] == '~') ++pos;
    while (pos < pl.size() && b.size() < b.max_size()) { b += pl[pos++]; }

    from_ = f; to_ = t; data_ = b;
    id_.hops  &= 0x0F;
    id_.flags &= 0x0F;
    if (id_.total == 0 || id_.part >= id_.total) return MessageStatus::HeaderPolicy;

    return MessageStatus::Ok;
  }
};

} // namespace viatext
