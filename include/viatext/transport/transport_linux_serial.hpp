#pragma once
/**
 * @file transport_linux_serial.hpp
 * @brief Linux USB/tty transport (header-only, termios; non-blocking).
 *
 * Depends on: unistd.h, fcntl.h, termios.h. STL only for std::string (Linux-only path).
 */

#if !defined(__linux__)
#  error "transport_linux_serial.hpp is Linux-only."
#endif

#include "viatext/transport/transport_base.hpp"
#include <string>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <cerrno>

namespace viatext::transport {

struct SerialConfig : public Config {
  std::string path;   // e.g. /dev/serial/by-id/usb-...
  int baud{115200};
};

class LinuxSerial : public ITransport {
public:
  explicit LinuxSerial(const std::string& dev_path = {}, int baud=115200)
  : dev_path_(dev_path), baud_(baud) {}

  bool begin(const Config& cfg) override {
    // We control the call sites; treat cfg as SerialConfig.
    // If a wrong type is passed, it's a programmer error.
    const auto& sc = static_cast<const SerialConfig&>(cfg);

    if (!sc.path.empty()) dev_path_ = sc.path;
    baud_ = sc.baud;
    mtu_  = sc.mtu;

    if (dev_path_.empty()) return false;

    fd_ = ::open(dev_path_.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd_ < 0) return false;

    termios tio{}; 
    if (::tcgetattr(fd_, &tio) != 0) return false;
    ::cfmakeraw(&tio);

    speed_t sp = B115200;
    switch (baud_) {
      case 9600:   sp = B9600; break;
      case 19200:  sp = B19200; break;
      case 38400:  sp = B38400; break;
      case 57600:  sp = B57600; break;
      case 115200: sp = B115200; break;
#ifdef B230400
      case 230400: sp = B230400; break;
#endif
#ifdef B460800
      case 460800: sp = B460800; break;
#endif
      default:     sp = B115200; break;
    }

    ::cfsetispeed(&tio, sp);
    ::cfsetospeed(&tio, sp);
    tio.c_cflag |= CLOCAL | CREAD; // enable receiver, ignore modem ctrl

    if (::tcsetattr(fd_, TCSANOW, &tio) != 0) return false;
    return true;
  }

  void end() override {
    if (fd_ >= 0) { ::close(fd_); fd_ = -1; }
  }

  void poll() override { /* nothing: non-blocking fd */ }

  std::size_t available() const override {
    if (fd_ < 0) return 0;
    int n = 0;
    if (::ioctl(fd_, FIONREAD, &n) != 0) return 0;
    return n > 0 ? static_cast<std::size_t>(n) : 0;
  }

  RxResult recv(uint8_t* out, std::size_t cap, std::size_t& out_len) override {
    out_len = 0;
    if (fd_ < 0 || cap == 0) return RxResult::Error;
    ssize_t r = ::read(fd_, out, cap);
    if (r > 0) { out_len = static_cast<std::size_t>(r); return RxResult::Ok; }
    if (r == 0 || (r < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))) return RxResult::None;
    return RxResult::Error;
  }

  TxResult send(const uint8_t* data, std::size_t len) override {
    if (fd_ < 0 || !data || !len) return TxResult::Error;
    ssize_t w = ::write(fd_, data, len);
    if (w < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) return TxResult::Busy;
    return (w >= 0) ? TxResult::Ok : TxResult::Error;
  }

  const char* name() const override { return "linux-serial"; }
  std::size_t mtu() const override { return mtu_; }

private:
  int fd_{-1};
  std::string dev_path_;
  int baud_{115200};
  std::size_t mtu_{240};
};

} // namespace viatext::transport
