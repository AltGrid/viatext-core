#pragma once
/**
 * @file transport_lora_sx1262.hpp
 * @brief ESP32 + SX1262 transport (LilyGO LoRa32 v2.1). Header-only, non-blocking.
 *
 * Requires your chosen SX126x driver. This sketch assumes a generic driver with:
 *   bool begin(pins...); bool rxStart(); bool tx(const uint8_t*, size_t);
 *   bool read(uint8_t*, size_t&, size_t cap); // returns a packet if ready
 *   void onRxDone(void(*)(void*), void*); void onTxDone(void(*)(void*), void*);
 */

#include "viatext/transport/transport_base.hpp"

// Detect Arduino/ESP32 unless explicitly overridden
#ifndef VT_ENABLE_ARDUINO
#  if defined(ARDUINO) || defined(ESP_PLATFORM)
#    define VT_ENABLE_ARDUINO 1
#  else
#    define VT_ENABLE_ARDUINO 0
#  endif
#endif

#if VT_ENABLE_ARDUINO

  #include <Arduino.h>

  // Default LilyGO pin map; override with -DVT_LORA_* if needed
  #ifndef VT_LORA_SCK
  #define VT_LORA_SCK   18
  #define VT_LORA_MISO  19
  #define VT_LORA_MOSI  23
  #define VT_LORA_CS     5
  #define VT_LORA_RST   14
  #define VT_LORA_BUSY  25
  #define VT_LORA_DIO1  26
  #endif

  namespace viatext::transport {

  // Forward-declare (replace with your actual SX126x driver type)
  class Sx1262RadioDriver {
  public:
    bool begin(int sck,int miso,int mosi,int cs,int rst,int busy,int dio1);
    bool setParams(/* sf,bw,cr,power etc. */);
    bool rxStart();
    bool tx(const uint8_t* data, std::size_t len);
    bool read(uint8_t* out, std::size_t& out_len, std::size_t cap);
    void onRxDone(void(*cb)(void*), void* arg);
    void onTxDone(void(*cb)(void*), void* arg);
  };

  struct LoRaConfig : public Config {
    uint8_t power_dbm{14}; // keep modest for bench
    // TODO: sf, bw, cr fields if you like
  };

  class LoRaSX1262 : public ITransport {
  public:
    bool begin(const Config& cfg) override {
      const LoRaConfig* lc = static_cast<const LoRaConfig*>(&cfg);
      (void)lc; // currently unused
      rx_ready_ = tx_done_ = false;
      if (!radio_.begin(VT_LORA_SCK,VT_LORA_MISO,VT_LORA_MOSI,VT_LORA_CS,
                        VT_LORA_RST,VT_LORA_BUSY,VT_LORA_DIO1))
        return false;
      // radio_.setParams(...);
      radio_.onRxDone(&LoRaSX1262::on_rx_isr, this);
      radio_.onTxDone(&LoRaSX1262::on_tx_isr, this);
      mtu_ = 200; // conservative default; tune later
      return radio_.rxStart();
    }

    void end() override {
      // no-op for now; add sleep/shutdown if driver provides it
    }

    void poll() override {
      // Empty: ISR sets flags; you could drain here if driver needs periodic service
    }

    std::size_t available() const override {
      // We don’t know packet length until read(); expose a 0/1 signal.
      return rx_ready_ ? 1 : 0;
    }

    RxResult recv(uint8_t* out, std::size_t cap, std::size_t& out_len) override {
      out_len = 0;
      if (!rx_ready_) return RxResult::None;
      rx_ready_ = false; // consume the flag; single-packet ready
      if (!radio_.read(out, out_len, cap)) return RxResult::Error;
      return out_len ? RxResult::Ok : RxResult::None;
    }

    TxResult send(const uint8_t* data, std::size_t len) override {
      if (!data || !len) return TxResult::Error;
      if (len > mtu_)   return TxResult::Error;
      if (tx_inflight_) return TxResult::Busy;
      tx_done_ = false;
      tx_inflight_ = radio_.tx(data, len);
      return tx_inflight_ ? TxResult::Ok : TxResult::Error;
    }

    const char* name() const override { return "lora-sx1262"; }
    std::size_t mtu() const override { return mtu_; }

  private:
    static void on_rx_isr(void* arg) {
      reinterpret_cast<LoRaSX1262*>(arg)->rx_ready_ = true;
    }
    static void on_tx_isr(void* arg) {
      auto* self = reinterpret_cast<LoRaSX1262*>(arg);
      self->tx_done_ = true; self->tx_inflight_ = false;
      // Optionally restart RX here if driver requires explicit rxStart() post-TX
      self->radio_.rxStart();
    }

    volatile bool rx_ready_{false};
    volatile bool tx_done_{false};
    volatile bool tx_inflight_{false};
    std::size_t mtu_{200};
    Sx1262RadioDriver radio_;
  };

  } // namespace viatext::transport

#else  // ===== Stub for non-Arduino builds =====

  namespace viatext::transport {
    struct LoRaConfig : public Config { uint8_t power_dbm{0}; };
    class LoRaSX1262 : public ITransport {
    public:
      bool begin(const Config&) override { return false; }
      void end() override {}
      void poll() override {}
      std::size_t available() const override { return 0; }
      RxResult recv(uint8_t*, std::size_t, std::size_t& out_len) override { out_len = 0; return RxResult::None; }
      TxResult send(const uint8_t*, std::size_t) override { return TxResult::Error; }
      const char* name() const override { return "lora-sx1262(stub)"; }
      std::size_t mtu() const override { return 0; }
    };
  }

#endif
