/**
 * @page vt-serial-io-hdr ViaText Serial I/O API (Header)
 * @file serial_io.hpp
 * @brief Public API for opening a Linux TTY in raw mode and moving SLIP-framed messages.
 *
 * @details
 * PURPOSE
 * -------
 * This header declares the minimal surface needed to talk to a serial-attached
 * radio or microcontroller from a Linux host. It pairs with serial_io.cpp for
 * the POSIX work and with slip.hpp for message framing. The goal is to keep the
 * link boring and predictable so the rest of ViaText can focus on routing and logic.
 *
 * ROLE IN VIATEXT
 * ---------------
 * - viatext::open_serial: acquire a file descriptor to a TTY, set raw mode, and
 *   absorb the usual boot noise after device reset.
 * - viatext::write_frame: SLIP-encode one payload and write it as a single frame.
 * - viatext::read_frame: poll and accumulate bytes until a full SLIP frame is decoded.
 * - viatext::close_serial: close the descriptor cleanly.
 *
 * These functions are used by:
 * - viatext-cli: one-shot send/receive from shell scripts or other programs.
 * - station/daemon wrappers: long-running processes that hold the port open.
 * - test harnesses: pseudo-TTY or TCP-to-serial bridges during development.
 *
 * DESIGN CHOICES
 * --------------
 * - Simplicity: free functions, no class hierarchy, no hidden threads.
 * - Portability: relies on POSIX termios and poll. Runs on laptops, Pi, thin clients.
 * - Autonomy: no manager process required. Your process opens the port and talks.
 *
 * HOW IT FITS TOGETHER
 * --------------------
 *   [your code] -> open_serial() -> write_frame()/read_frame() -> close_serial()
 *            \-> slip.hpp (framing)  \-> serial_io.cpp (syscalls)
 *
 * OPERATIONAL NOTES
 * -----------------
 * - Device selection: prefer /dev/serial/by-id/... or your own udev rules for stable paths.
 * - Permissions: ensure the runtime user is in the dialout group, or run as a service user
 *   with explicit access.
 * - Timeouts: read_frame uses poll with a millisecond timeout. A false return can mean a
 *   clean timeout, or an underlying poll/read error. Upper layers decide how to retry.
 * - Framing: SLIP provides boundaries only. If you need integrity or receipts, add them
 *   at the ViaText message layer.
 *
 * EXAMPLE
 * -------
 * @code
 *   using namespace viatext;
 *   int fd = open_serial("/dev/ttyACM0", 115200, 300);
 *   if (fd < 0) { // handle open failure  }
 *
 *   std::vector<uint8_t> payload = {'p','i','n','g'};
 *   if (!write_frame(fd, payload)) {
 *       // handle write failure (fd may be invalid or output buffer full)
 *   }
 *
 *   std::vector<uint8_t> frame;
 *   if (read_frame(fd, frame, 1000)) {
 *       // got one complete payload in 'frame'
 *   } else {
 *       // timeout or link error; decide whether to retry or reopen
 *   }
 *
 *   close_serial(fd);
 * @endcode
 *
 * LIMITATIONS AND TRADE-OFFS
 * --------------------------
 * - Baud table: the implementation maps a small set of common baud rates to termios speeds.
 *   Unknown values fall back to 115200. Extend as needed for your hardware.
 * - Atomic writes: write_frame attempts to write the entire encoded frame in one call.
 *   If only a partial write occurs, it returns false rather than looping. This keeps the
 *   code small and predictable. For very large frames or very small driver buffers, add a
 *   higher-layer retry or a looping writer.
 * - Concurrency: do not share a single fd between threads without external synchronization.
 *
 * DEPENDENCIES
 * ------------
 * - serial_io.cpp: termios configuration, poll loop, and syscalls.
 * - slip.hpp: SLIP encoder and bytewise decoder.
 *
 * MAINTENANCE
 * -----------
 * Keep this API narrow. If platform variance is required, isolate it in serial_io.cpp
 * behind these exact declarations rather than adding new call paths.
 *
 * @author Leo
 * @author ChatGPT
 */
#pragma once
#include <vector>
#include <string>

namespace viatext {

/**
 * @brief Open a Linux TTY device, configure it for raw I/O, and return its file descriptor.
 *
 * What it does:
 *   - Opens the device path (e.g., "/dev/ttyACM0") with O_RDWR | O_NOCTTY | O_NONBLOCK.
 *   - Puts the port into "raw" mode (8N1, no echo, no line processing).
 *   - Sets the baud rate using a small internal mapping (9600..230400; otherwise 115200).
 *   - Waits briefly after open to let USB CDC ACM devices finish their auto-reset.
 *   - Flushes any boot chatter from the driver buffers.
 *
 * When to use:
 *   Call this once at startup to acquire a stable file descriptor for read/write operations.
 *
 * Parameters:
 *   @param dev            Absolute device path, e.g. "/dev/serial/by-id/usb-..." or "/dev/ttyACM0".
 *   @param baud           Requested baud rate (common values supported: 9600, 19200, 38400,
 *                         57600, 115200 (default), 230400). Unknown values fall back to 115200.
 *   @param boot_delay_ms  Milliseconds to sleep after opening before first I/O
 *                         (typical 200–500 ms for USB CDC auto-reset). Default: 400 ms.
 *
 * Returns:
 *   @return File descriptor (non-negative) on success, or -1 on failure.
 *
 * Notes:
 *   - The returned fd is non-blocking. The higher layers use poll() for reads with timeouts.
 *   - Caller owns the descriptor and must close it with close_serial().
 */
int open_serial(const std::string& dev, int baud = 115200, int boot_delay_ms = 400);


/**
 * @brief SLIP-encode one payload and write it to the serial port as a single frame.
 *
 * What it does:
 *   - Takes a raw payload buffer and SLIP-encodes it (adds frame delimiters and escapes).
 *   - Attempts a single write(2) of the entire encoded frame.
 *
 * Parameters:
 *   @param fd       File descriptor previously returned by open_serial().
 *   @param payload  Raw, unframed bytes to send (will be SLIP-encoded internally).
 *
 * Returns:
 *   @return true if the entire encoded frame was written in one system call; false otherwise.
 *
 * Notes:
 *   - If the underlying driver accepts only a partial write, this returns false rather than looping.
 *     If you need guaranteed delivery, add a higher-level retry/loop on the caller side.
 *   - This function does not add checksums or acknowledgments. If you need integrity or receipts,
 *     implement them at the ViaText message layer.
 */
bool write_frame(int fd, const std::vector<uint8_t>& payload);


/**
 * @brief Read one SLIP-framed payload from the serial port, with a millisecond timeout.
 *
 * What it does:
 *   - Uses poll(2) to wait for readability up to timeout_ms.
 *   - Feeds incoming bytes to a SLIP decoder until one full frame is reconstructed.
 *   - On success, places the raw (decoded) payload into @p out.
 *
 * Parameters:
 *   @param fd          File descriptor previously returned by open_serial().
 *   @param out         Output buffer that will receive the decoded payload bytes on success.
 *                      It is cleared at entry.
 *   @param timeout_ms  Milliseconds to wait for a complete frame (default: 1500 ms).
 *
 * Returns:
 *   @return true if a complete SLIP frame was received and decoded into @p out;
 *           false on timeout or I/O error.
 *
 * Notes:
 *   - A false return value does not distinguish between a clean timeout and an error;
 *     the caller should decide whether to retry, reopen, or abort based on context.
 *   - This reads one frame at a time. If you expect multiple frames, call repeatedly.
 */
bool read_frame(int fd, std::vector<uint8_t>& out, int timeout_ms = 1500);


/**
 * @brief Close a serial file descriptor obtained from open_serial().
 *
 * Parameters:
 *   @param fd  File descriptor to close. If negative, the call is a no-op.
 *
 * Side effects:
 *   - Releases the underlying OS handle and any associated kernel resources.
 *
 * Notes:
 *   - Safe to call exactly once per descriptor ownership. Do not use @p fd after closing.
 */
void close_serial(int fd);


} // namespace vt
