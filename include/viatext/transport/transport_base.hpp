#pragma once
/**
 * @file transport_base.hpp
 * @brief Minimal, Core-agnostic transport interface for ViaText wrappers.
 *
 * Header-only on purpose for easy embedding. No STL in the embedded path.
 */

#include <cstddef>
#include <cstdint>

namespace viatext::transport {

// Return codes kept simple for embedded sanity.
enum class TxResult : uint8_t { Ok=0, Busy=1, Error=2 };
enum class RxResult : uint8_t { None=0, Ok=1, Error=2 };

struct Config {
  // You can extend per-transport via downcast or specialized ctors.
  uint16_t mtu{240};   // conservative default; LoRa often lower
  uint8_t  reserved{0};
};

/**
 * @brief Transport trait every wrapper can rely on.
 *
 * Contract:
 *  - begin(cfg) initializes hardware/port.
 *  - poll() does non-blocking service work (ISR flags, read FIFO, etc.).
 *  - available() returns bytes ready for recv().
 *  - recv(buf,cap) pulls up to cap bytes; returns RxResult::Ok & count>0 on success.
 *  - send(buf,len) enqueues/transmits; never blocks for long (chunk or return Busy).
 *  - name() is a short identifier for logs/diagnostics.
 */
class ITransport {
public:
  virtual ~ITransport() = default;
  virtual bool      begin(const Config& cfg) = 0;
  virtual void      end() = 0;
  virtual void      poll() = 0;
  virtual std::size_t available() const = 0;
  virtual RxResult  recv(uint8_t* out, std::size_t cap, std::size_t& out_len) = 0;
  virtual TxResult  send(const uint8_t* data, std::size_t len) = 0;
  virtual const char* name() const = 0;
  virtual std::size_t mtu() const = 0;
};

} // namespace viatext::transport
