#pragma once

/**
 * @page vt-slip ViaText SLIP Framing
 * @file slip.hpp
 * @brief Tiny SLIP encoder/decoder for byte-accurate message boundaries over noisy serial links.
 *
 * @details
 * OVERVIEW
 * --------
 * This header implements SLIP (Serial Line Internet Protocol) framing for ViaText.
 * SLIP is a micro-protocol that wraps an arbitrary byte payload between sentinel bytes
 * and escapes any sentinel collisions inside the payload. It gives us explicit
 * message boundaries over a raw byte stream without negotiating a heavyweight link layer.
 *
 * WHY SLIP
 * --------
 * 1) Small and obvious. A few constants, a short encoder, and a stateful decoder.
 * 2) Works anywhere a stream of bytes exists: UART, USB CDC ACM, TCP sockets, pipes, PTYs.
 * 3) Resilient to device resets and garbage. Out-of-band boot chatter is flushed
 *    by the decoder when a fresh END sentinel arrives.
 *
 * HOW SLIP WORKS
 * --------------
 * SLIP defines special byte values:
 *   END       (0xC0) marks frame boundaries.
 *   ESC       (0xDB) introduces an escaped code.
 *   ESC_END   (0xDC) stands in for the END byte inside payloads.
 *   ESC_ESC   (0xDD) stands in for the ESC byte inside payloads.
 *
 * Encoding rules:
 *   - A frame begins with END and ends with END.
 *   - Inside a frame, any literal END (0xC0) byte is replaced by ESC, ESC_END.
 *   - Any literal ESC (0xDB) byte is replaced by ESC, ESC_ESC.
 *   - All other bytes are copied through unchanged.
 *
 * Decoding rules:
 *   - Bytes outside a frame are ignored until END is seen. That END starts a frame.
 *   - Bytes between the starting END and the closing END are accumulated.
 *   - If ESC is seen, the next byte must be ESC_END or ESC_ESC and is translated back
 *     to END or ESC respectively. Any other value is treated as a protocol error and
 *     the current frame is dropped.
 *   - When END is seen while in a frame and at least one byte has been collected,
 *     the frame is considered complete and delivered up.
 *   - Empty frames (END END with nothing collected) are treated as separators and ignored.
 *
 * DESIGN NOTES
 * ------------
 * - State lives inside a decoder instance so callers can feed one byte at a time from
 *   poll/select/ISR loops without blocking.
 * - The encoder reserves worst case capacity (2x payload + 2) before writing to reduce
 *   dynamic reallocations.
 * - No heap ownership tricks: caller owns the output vectors.
 *
 * EXAMPLES
 * --------
 * Encode a payload:
 * @code
 *   std::vector<uint8_t> out;
 *   const char* text = "hi\xC0there"; // contains END in the middle
 *   viatext::slip::encode(reinterpret_cast<const uint8_t*>(text), 8, out);
 *   // 'out' now contains: C0 68 69 DB DC 74 68 65 72 65 C0
 * @endcode
 *
 * Decode from a stream:
 * @code
 *   viatext::slip::decoder dec;
 *   std::vector<uint8_t> frame;
 *   for (uint8_t b : incoming_bytes) {
 *       if (dec.feed(b, frame)) {
 *           // 'frame' holds exactly one complete payload (no END/ESC bytes).
 *           handle(frame);
 *           frame.clear(); // optional; feed() overwrites on next completion
 *       }
 *   }
 * @endcode
 *
 * VALIDATING A SLIP IMPLEMENTATION
 * --------------------------------
 * - Round-trip: encode -> bytewise feed -> decode must recover the original payload.
 * - Boundary stress: consecutive frames back-to-back with no idle bytes must decode cleanly.
 * - Corruption: inject random ESC followed by non-ESC_END/ESC_ESC and verify decoder
 *   drops the current frame and waits for the next END to resynchronize.
 *
 * OPERATIONAL NOTES
 * -----------------
 * - SLIP provides framing only. It does not authenticate or protect contents. If you
 *   need integrity, add a checksum or higher-level receipt at the ViaText layer.
 * - On noisy links, favor small frames with retries over very large frames. Smaller
 *   frames reduce the cost of resends when a single byte is lost.
 *
 * @author Leo
 * @author ChatGPT
 */

// Dependencies:
// - <vector>   for caller-owned dynamic byte buffers (std::vector<uint8_t>).
// - <cstdint>  for fixed-size byte type (uint8_t).
#include <vector>
#include <cstdint>

namespace viatext {
namespace slip {

/**
 * @name SLIP sentinel and escape codes
 * @{
 */

/**
 * @brief Frame boundary marker byte.
 *
 * Meaning: An END byte begins a frame when seen outside a frame, and closes a frame
 * when seen inside a frame. Empty frames (END followed immediately by END) are ignored.
 *
 * Value: 0xC0
 */
static constexpr uint8_t END = 0xC0;

/**
 * @brief Escape introducer byte.
 *
 * Meaning: Inside a frame, a literal END or ESC cannot appear unescaped. Each is
 * represented by an ESC followed by a code (ESC_END or ESC_ESC).
 *
 * Value: 0xDB
 */
static constexpr uint8_t ESC = 0xDB;

/**
 * @brief Escaped representation of a literal END within payload.
 *
 * Sequence on wire: ESC, ESC_END
 * Decodes to: END
 *
 * Value: 0xDC
 */
static constexpr uint8_t ESC_END = 0xDC;

/**
 * @brief Escaped representation of a literal ESC within payload.
 *
 * Sequence on wire: ESC, ESC_ESC
 * Decodes to: ESC
 *
 * Value: 0xDD
 */
static constexpr uint8_t ESC_ESC = 0xDD;
/** @} */

/**
 * @brief Encode a raw payload into a single SLIP frame.
 *
 * The output frame begins with END, escapes any in-payload END/ESC bytes, and closes with END.
 * This yields unambiguous message boundaries over byte streams (UART, USB CDC, TCP, PTYs).
 *
 * @param in  Pointer to the first byte of the payload to encode.
 * @param n   Number of payload bytes at @p in.
 * @param out Destination vector that receives the encoded frame. This function clears it first.
 *
 * @note Capacity hint: reserves worst case capacity (2*n + 2) to reduce reallocations when
 *       many bytes require escaping.
 * @warning Provides framing only. If you need integrity or authenticity, add checksums or
 *          higher-level receipts at the ViaText layer.
 */
inline void encode(const uint8_t* in, size_t n, std::vector<uint8_t>& out) {
    out.clear();            // fresh frame
    out.reserve(n * 2 + 2); // worst case: every byte escapes, plus start/end

    out.push_back(END);     // start-of-frame sentinel

    for (size_t i = 0; i < n; ++i) {
        uint8_t b = in[i];  // next input byte

        if (b == END) {                 // cannot place raw END in payload
            out.push_back(ESC);         // introduce escape
            out.push_back(ESC_END);     // represent literal END
        } else if (b == ESC) {          // cannot place raw ESC in payload
            out.push_back(ESC);         // introduce escape
            out.push_back(ESC_ESC);     // represent literal ESC
        } else {
            out.push_back(b);           // ordinary byte passes through
        }
    }

    out.push_back(END);     // end-of-frame sentinel
}

/**
 * @brief Stateful SLIP decoder for byte-at-a-time feeds.
 *
 * Role: Accepts bytes as they arrive (poll/select/ISR) and reconstructs payloads delimited
 * by SLIP END markers. Maintains minimal state to survive noise and resynchronize on END.
 *
 * Field constraints and tradeoffs:
 * - Uses simple boolean flags (in_frame, esc) for clarity and speed.
 * - Drops a partial frame on malformed escape and waits for next END to resync.
 * - Assigns the completed payload into the caller-provided vector to avoid hidden allocations.
 */
struct decoder {
    /**
     * @brief Accumulator for the current frame payload (without END/ESC sentinel bytes).
     *
     * Ownership: internal to the decoder until a frame completes, at which point @ref feed
     * assigns its contents to the caller-provided @p frame and clears this buffer.
     */
    std::vector<uint8_t> buf;

    /**
     * @brief Escape-state flag.
     *
     * Meaning: true iff the previous byte was ESC and the decoder is expecting the next byte
     * to be either ESC_END or ESC_ESC. Cleared on resolution, frame completion, or reset.
     */
    bool esc = false;

    /**
     * @brief In-frame flag.
     *
     * Meaning: true after a starting END has been seen and until a closing END completes
     * a non-empty frame or the frame is dropped due to a protocol error.
     */
    bool in_frame = false;

    /**
     * @brief Feed one byte from the stream; emit a frame when a non-empty payload is closed.
     *
     * @param b      Next raw byte from the underlying stream.
     * @param frame  Output vector that receives the decoded payload when a frame completes.
     *               On return true, this vector contains exactly one payload (no END/ESC bytes).
     *
     * @return bool  true if a complete frame was produced in @p frame; false otherwise.
     *
     * Behavior:
     * - Outside a frame, all bytes are ignored until an END begins a new frame.
     * - Inside a frame:
     *     - END completes the frame if at least one byte was collected; empty frames are ignored.
     *     - ESC sets @ref esc and defers action to the next byte (which must be ESC_END or ESC_ESC).
     *     - Any other byte is appended as payload (or translated from an escape first).
     * - On malformed escape (ESC followed by an unexpected code), the partial frame is dropped,
     *   state is reset, and the decoder waits for the next END to resynchronize.
     */
    bool feed(uint8_t b, std::vector<uint8_t>& frame) {
        // END has global meaning: either completes a non-empty frame or starts a fresh one.
        if (b == END) {
            if (in_frame && !buf.empty()) {
                frame = buf;       // deliver payload
                buf.clear();       // reset accumulation
                in_frame = false;  // exit frame state
                esc = false;       // clear any pending escape
                return true;       // completed a frame
            } else {
                buf.clear();       // discard noise / clear empties
                in_frame = true;   // start new frame on next bytes
                esc = false;       // escapes never carry across frames
                return false;      // no frame produced yet
            }
        }

        // Ignore everything until we have seen a starting END.
        if (!in_frame) return false;

        if (esc) {
            // Resolve the escape by translating the code into its literal byte.
            esc = false;
            if      (b == ESC_END) b = END;  // ESC, ESC_END => literal END
            else if (b == ESC_ESC) b = ESC;  // ESC, ESC_ESC => literal ESC
            else {
                // Malformed escape sequence: drop current frame and wait for next END.
                buf.clear();
                in_frame = false;
                return false;
            }
        } else if (b == ESC) {
            // Mark that the next byte must specify which literal we intended.
            esc = true;
            return false;
        }

        // Append ordinary byte (or the literal resolved from an escape) to the payload.
        buf.push_back(b);
        return false; // frame not complete until a closing END is observed
    }
};

} // namespace slip
} // namespace viatext
