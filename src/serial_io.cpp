// ============================================================================
// serial_io.cpp — implementation for serial_io.hpp
// For API/overview see the matching .hpp. For usage examples, check tests/.
// ============================================================================

/**
 * @file serial_io.cpp
 */

#include "serial_io.hpp"   // declarations for open_serial(), write_frame(), read_frame(), close_serial()
#include "slip.hpp"        // viatext::slip::encode() and decoder for frame boundaries

// POSIX / termios headers for low-level serial port handling
#include <fcntl.h>         // ::open flags (O_RDWR, O_NOCTTY, etc.)
#include <unistd.h>        // ::read, ::write, ::close, usleep
#include <termios.h>       // termios struct + raw mode helpers
#include <poll.h>          // poll(2) for timeout-based read loop
#include <cstring>         // memset, etc. (used indirectly by termios calls)

namespace viatext {

// ---------------------------------------------------------------------------
// set_raw()
// ----------
// Configure a file descriptor for raw serial I/O at the given baud.
// - Disables echo, line buffering, and flow control (8N1 raw mode).
// - Sets VMIN=0, VTIME=0 (non-blocking reads; poll() handles timing).
// - Flushes both input/output buffers after applying settings.
//
// Returns: true on success, false if tcgetattr/tcsetattr fails.
//
// Design:
// - We keep this static since it's an internal helper for open_serial() only.
// - Encapsulates the boilerplate termios setup so callers stay clean.
// ---------------------------------------------------------------------------
static bool set_raw(int fd, speed_t baud) {
    termios tio{};
    if (tcgetattr(fd, &tio) != 0) return false;   // fetch current settings

    cfmakeraw(&tio);                              // wipe into raw 8N1 mode
    cfsetispeed(&tio, baud);                      // set baud in/out
    cfsetospeed(&tio, baud);

    tio.c_cflag |= (CLOCAL | CREAD);              // ignore modem ctrl, enable read
    tio.c_cflag &= ~CRTSCTS;                      // disable hardware flow control
    tio.c_cc[VMIN]  = 0;                          // no minimum chars per read
    tio.c_cc[VTIME] = 0;                          // no interbyte timer (we use poll)

    if (tcsetattr(fd, TCSANOW, &tio) != 0) return false;  // apply immediately
    tcflush(fd, TCIOFLUSH);                       // flush in/out buffers
    return true;
}


// ---------------------------------------------------------------------------
// open_serial()
// -------------
// Open and initialize a serial port at the requested baud.
// - Applies O_NOCTTY (don’t steal controlling terminal) and O_NONBLOCK.
// - Maps common baud integers (9600..230400) to termios constants.
// - Calls set_raw() to apply 8N1 raw mode.
// - Sleeps boot_delay_ms to allow USB CDC devices to reset on open.
// - Flushes boot chatter after delay.
//
// Returns: file descriptor (>=0) or -1 on failure.
// ---------------------------------------------------------------------------
int open_serial(const std::string& dev, int baud, int boot_delay_ms) {
    int fd = ::open(dev.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0) return -1;                        // open failed (perm, missing, etc.)

    speed_t sp = B115200;                         // default
    if      (baud == 9600)   sp = B9600;
    else if (baud == 19200)  sp = B19200;
    else if (baud == 38400)  sp = B38400;
    else if (baud == 57600)  sp = B57600;
    else if (baud == 230400) sp = B230400;

    set_raw(fd, sp);                              // configure low-level mode

    usleep(boot_delay_ms * 1000);                 // allow USB-serial auto-reset
    tcflush(fd, TCIOFLUSH);                       // flush any reboot chatter
    return fd;
}


// ---------------------------------------------------------------------------
// write_frame()
// -------------
// Encode a payload into SLIP format and write it to the serial fd.
//
// Returns: true if entire frame was written successfully, false otherwise.
//
// Notes:
// - SLIP adds END/ESC bytes so payload boundaries are preserved.
// - We buffer into a temporary vector before calling ::write().
// ---------------------------------------------------------------------------
bool write_frame(int fd, const std::vector<uint8_t>& payload) {
    std::vector<uint8_t> out;
    viatext::slip::encode(payload.data(), payload.size(), out);
    return ::write(fd, out.data(), out.size()) == (ssize_t)out.size();
}


// ---------------------------------------------------------------------------
// read_frame()
// ------------
// Blocking read loop that assembles exactly one SLIP frame.
// - Uses poll() for timeout (timeout_ms).
// - Feeds bytes into a slip::decoder until a full frame is seen.
// - Stores decoded payload into 'out'.
//
// Returns: true if a full frame was decoded, false on timeout or error.
//
// Design:
// - VMIN/VTIME are zero (non-blocking), so poll() controls blocking time.
// - Reads one byte at a time; low throughput, but reliable for framing.
// ---------------------------------------------------------------------------
bool read_frame(int fd, std::vector<uint8_t>& out, int timeout_ms) {
    viatext::slip::decoder dec;
    out.clear();
    uint8_t byte = 0;
    pollfd pfd{fd, POLLIN, 0};

    while (true) {
        int pr = ::poll(&pfd, 1, timeout_ms);
        if (pr == 0) return false;                // timeout expired
        if (pr < 0)  return false;                // poll error (EINTR etc.)
        if (pfd.revents & POLLIN) {
            ssize_t n = ::read(fd, &byte, 1);
            if (n == 1 && dec.feed(byte, out))    // decoder returns true if END seen
                return true;                      // full frame ready in 'out'
        }
    }
}


// ---------------------------------------------------------------------------
// close_serial()
// --------------
// Close a serial fd if valid (>=0).
// ---------------------------------------------------------------------------
void close_serial(int fd) {
    if (fd >= 0) ::close(fd);
}

} // namespace viatext
