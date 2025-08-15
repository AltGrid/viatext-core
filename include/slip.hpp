/**
 * @file slip.hpp
 */
#pragma once
#include <vector>
#include <cstdint>

namespace viatext {
namespace slip {

static constexpr uint8_t END     = 0xC0;
static constexpr uint8_t ESC     = 0xDB;
static constexpr uint8_t ESC_END = 0xDC;
static constexpr uint8_t ESC_ESC = 0xDD;

/// Encode raw payload into SLIP frame.
inline void encode(const uint8_t* in, size_t n, std::vector<uint8_t>& out) {
    out.clear();
    out.reserve(n * 2 + 2);
    out.push_back(END); // start
    for (size_t i = 0; i < n; ++i) {
        uint8_t b = in[i];
        if (b == END) { out.push_back(ESC); out.push_back(ESC_END); }
        else if (b == ESC) { out.push_back(ESC); out.push_back(ESC_ESC); }
        else { out.push_back(b); }
    }
    out.push_back(END); // end
}

/// Stateful decoder — feed bytes one at a time; outputs a frame when ready.
struct decoder {
    std::vector<uint8_t> buf;
    bool esc = false;
    bool in_frame = false;

    /// Feed one byte. Returns true if a complete frame is output into `frame`.
    bool feed(uint8_t b, std::vector<uint8_t>& frame) {
        if (b == END) {
            if (in_frame && !buf.empty()) {
                frame = buf;
                buf.clear();
                in_frame = false;
                esc = false;
                return true; // completed a frame
            } else {
                buf.clear();
                in_frame = true;
                esc = false;
                return false; // start of new frame
            }
        }
        if (!in_frame) return false;

        if (esc) {
            esc = false;
            if      (b == ESC_END) b = END;
            else if (b == ESC_ESC) b = ESC;
            else { buf.clear(); in_frame = false; return false; } // protocol error
        } else if (b == ESC) {
            esc = true;
            return false;
        }
        buf.push_back(b);
        return false;
    }
};

} // namespace slip
} // namespace viatext
