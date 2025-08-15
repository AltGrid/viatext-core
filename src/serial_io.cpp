/**
 * @file serial_io.cpp
 */
#include "serial_io.hpp"
#include "slip.hpp"

#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <poll.h>
#include <cstring>

namespace viatext {

static bool set_raw(int fd, speed_t baud){
    termios tio{};
    if (tcgetattr(fd, &tio) != 0) return false;
    cfmakeraw(&tio);
    cfsetispeed(&tio, baud);
    cfsetospeed(&tio, baud);
    tio.c_cflag |= (CLOCAL | CREAD);
    tio.c_cflag &= ~CRTSCTS;
    tio.c_cc[VMIN]  = 0;
    tio.c_cc[VTIME] = 0;
    if (tcsetattr(fd, TCSANOW, &tio) != 0) return false;
    tcflush(fd, TCIOFLUSH);
    return true;
}

int open_serial(const std::string& dev, int baud, int boot_delay_ms){
    int fd = ::open(dev.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0) return -1;
    speed_t sp = B115200;
    if (baud == 9600) sp = B9600;
    else if (baud == 19200) sp = B19200;
    else if (baud == 38400) sp = B38400;
    else if (baud == 57600) sp = B57600;
    else if (baud == 230400) sp = B230400;
    set_raw(fd, sp);
    usleep(boot_delay_ms * 1000); // allow USB-serial reboot
    tcflush(fd, TCIOFLUSH);       // drain boot spew
    return fd;
}

bool write_frame(int fd, const std::vector<uint8_t>& payload){
    std::vector<uint8_t> out;
    viatext::slip::encode(payload.data(), payload.size(), out);
    return ::write(fd, out.data(), out.size()) == (ssize_t)out.size();
}

bool read_frame(int fd, std::vector<uint8_t>& out, int timeout_ms){
    viatext::slip::decoder dec;
    out.clear();
    uint8_t byte = 0;
    pollfd pfd{fd, POLLIN, 0};
    while (true){
        int pr = ::poll(&pfd, 1, timeout_ms);
        if (pr == 0) return false;            // timeout
        if (pr < 0)  return false;            // poll error
        if (pfd.revents & POLLIN){
            ssize_t n = ::read(fd, &byte, 1);
            if (n == 1 && dec.feed(byte, out)) return true; // got a full frame
        }
    }
}

void close_serial(int fd){ if (fd >= 0) ::close(fd); }

} // namespace vt
