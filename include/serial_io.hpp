/**
 * @file serial_io.hpp
 */
#pragma once
#include <vector>
#include <string>

namespace viatext {

// Open, configure raw TTY (returns fd or -1)
int  open_serial(const std::string& dev, int baud = 115200, int boot_delay_ms = 400);

// Write one SLIP-framed payload
bool write_frame(int fd, const std::vector<uint8_t>& payload);

// Read one SLIP-framed payload with timeout (ms)
bool read_frame(int fd, std::vector<uint8_t>& out, int timeout_ms = 1500);

// Close fd
void close_serial(int fd);

} // namespace vt
