// ============================================================================
// node_registry.cpp — implementation for node_registry.hpp
// For API/overview see the matching .hpp. For usage examples, check tests/.
// ============================================================================

/**
 * @file node_registry.cpp
 */

#include "node_registry.hpp"  // public types and function declarations for the registry layer
#include "commands.hpp"       // viatext::make_get_id(), viatext::decode_pretty() for probing
#include "serial_io.hpp"      // viatext::open_serial(), write_frame(), read_frame(), close_serial()

#include <filesystem>         // std::filesystem for walking /dev and creating dirs/symlinks
#include <fstream>            // std::ofstream for writing nodes.json
#include <iostream>           // std::cerr for error reporting
#include <sstream>            // std::ostringstream if we need string assembly (kept for symmetry)
#include <chrono>             // std::chrono types (boot delays/timeouts are expressed in ms)
#include <thread>             // std::this_thread::sleep_for (used in serial open delay in serial_io)
#include <fcntl.h>            // POSIX file controls (serial_io may rely on these headers)
#include <unistd.h>           // POSIX calls (getuid(), close, etc.)
#include <glob.h>             // glob(3) for tty fallbacks when /dev/serial/by-id is absent
#include <cerrno>             // errno access for diagnostics
#include <system_error>       // std::error_code for non-throwing filesystem ops
#include <cstdlib>            // getenv for XDG/HOME lookups
#include <cstring>            // strerror for human-readable errno

namespace fs = std::filesystem;   // short handle; used heavily below
namespace viatext {

// ---------------------------------------------------------------------------
// Probe-time constants (tuned for USB CDC/tty experience).
// - PROBE_BAUD:   default via firmware; adjust only if firmware changes.
// - PROBE_TIMEOUT_MS: read timeout per device so the whole scan stays bounded.
// - PROBE_BOOT_MS: time to let a port settle after open (USB CDC sometimes re-enumerates).
// ---------------------------------------------------------------------------
static constexpr int PROBE_BAUD       = 115200;
static constexpr int PROBE_TIMEOUT_MS = 1200;   // ms per device (read_frame timeout)
static constexpr int PROBE_BOOT_MS    = 400;    // ms after open to let USB CDC reset


// -------- helpers --------

/*
 * probe_id()
 * ----------
 * Open a candidate serial device, send GET_ID, and extract the ID string
 * from the pretty-decoded response line. Quietly returns {} on any failure.
 *
 * Assumptions:
 * - serial_io handles SLIP framing and write/read correctness.
 * - GET_ID has no TLVs and is supported by all nodes.
 *
 * Trade-offs:
 * - We parse from the lossy decode_pretty() line to keep the probing fast
 *   and shell-friendly; exact TLV parsing would be stricter but overkill here.
 *
 * Phases:
 *   1) open port (with boot delay),
 *   2) write request,
 *   3) read response (bounded by timeout),
 *   4) close port,
 *   5) parse "id=" token out of the summary.
 */
static std::string probe_id(const std::string& dev_path) {
    int fd = viatext::open_serial(dev_path, /*baud*/PROBE_BAUD, /*boot_delay_ms*/PROBE_BOOT_MS);
    if (fd < 0) return {};  // can't open: not our device or no permission

    auto req = viatext::make_get_id(1);               // simple, deterministic seq=1 is fine for probe
    bool ok  = viatext::write_frame(fd, req);         // Step 2: write

    std::vector<uint8_t> resp;
    if (ok) ok = viatext::read_frame(fd, resp, PROBE_TIMEOUT_MS);  // Step 3: read (bounded)

    viatext::close_serial(fd);                        // Step 4: always close
    if (!ok) return {};                               // timeout or I/O error

    // Step 5: Parse "status=ok seq=N id=XYZ"
    auto line = viatext::decode_pretty(resp);
    auto pos  = line.find("id=");                     // we only care about the ID token
    if (pos == std::string::npos) return {};
    return line.substr(pos + 3);                      // everything after "id=" (ID may contain hyphens etc.)
}


/*
 * append_glob()
 * -------------
 * Append results of a glob() pattern to a vector of strings.
 *
 * Why glob?:
 * - When /dev/serial/by-id is missing, classic tty names like /dev/ttyACM* and
 *   /dev/ttyUSB* are the fallback.
 *
 * Pitfall:
 * - glob() allocates; always globfree() to avoid leaks.
 */
static void append_glob(std::vector<std::string>& out, const char* pattern) {
    glob_t g{};
    if (glob(pattern, 0, nullptr, &g) == 0) {
        for (size_t i = 0; i < g.gl_pathc; ++i)
            out.emplace_back(g.gl_pathv[i]);  // copy the discovered path
    }
    globfree(&g);  // release resources even on partial success
}


/*
 * runtime_dir()
 * -------------
 * Determine where to place runtime symlinks:
 * - Prefer $XDG_RUNTIME_DIR/viatext if defined (user-scoped, tmpfs-like).
 * - Otherwise use /run/user/<uid>/viatext (common Linux layout).
 *
 * Rationale:
 * - Symlinks are ephemeral runtime aids; they belong under /run, not $HOME.
 */
static fs::path runtime_dir() {
    if (const char* x = std::getenv("XDG_RUNTIME_DIR"); x && *x)
        return fs::path(x) / "viatext";
    return fs::path("/run/user") / std::to_string(getuid()) / "viatext";
}


// -------- public API --------

/*
 * discover_nodes()
 * ----------------
 * Walk the system looking for serial devices that might be ViaText nodes,
 * then probe each to confirm by requesting its ID.
 *
 * Strategy:
 * - Prefer /dev/serial/by-id symlinks for stability across reboots/ports.
 * - If that directory is absent, fall back to globbing tty patterns.
 * - For each candidate, attempt probe_id(); if it returns a non-empty string,
 *   mark the node as online.
 *
 * Failure handling:
 * - We never throw; errors just result in fewer entries or online=false.
 */
std::vector<NodeInfo> discover_nodes() {
    std::vector<NodeInfo> result;
    std::vector<std::string> candidates;

    // Prefer stable paths first: /dev/serial/by-id -> resolved to canonical device
    const fs::path by_id("/dev/serial/by-id");
    if (fs::exists(by_id)) {
        for (const auto& e : fs::directory_iterator(by_id)) {
            if (!e.is_symlink()) continue;                   // skip anything unexpected
            std::error_code ec;
            auto canon = fs::canonical(e.path(), ec);        // don't throw if it fails
            if (!ec) candidates.push_back(canon.string());   // keep canonical device path
        }
    } else {
        // Fallbacks: classic Linux USB-serial names
        append_glob(candidates, "/dev/ttyACM*");
        append_glob(candidates, "/dev/ttyUSB*");
    }

    // Probe each candidate device and record the result
    for (const auto& dev : candidates) {
        std::string id = probe_id(dev);                      // empty if not ours/offline
        result.push_back({id, dev, !id.empty()});            // online flag is id presence
    }
    return result;
}


/*
 * save_registry()
 * ---------------
 * Serialize the discovered roster to a small JSON file under
 * $HOME/.config/altgrid/viatext/nodes.json.
 *
 * Design:
 * - Minimal JSON so humans can read/edit it easily.
 * - No external JSON library to keep dependencies small.
 *
 * Edge cases:
 * - If $HOME is not set, fs::path("") yields a relative path; we still attempt
 *   to create directories (caller will see a failure via return false).
 */
bool save_registry(const std::vector<NodeInfo>& nodes) {
    fs::path conf = fs::path(std::getenv("HOME") ? std::getenv("HOME") : "") / ".config/altgrid/viatext";
    std::error_code ec;
    fs::create_directories(conf, ec);                         // non-throwing; check ec
    if (ec) { std::cerr << "config dir error: " << ec.message() << "\n"; return false; }

    std::ofstream ofs(conf / "nodes.json");
    if (!ofs) { std::cerr << "open nodes.json failed\n"; return false; }

    // Write a tiny JSON array; commas placed between elements only.
    ofs << "[\n";
    for (size_t i = 0; i < nodes.size(); ++i) {
        ofs << "  {\"id\":\"" << nodes[i].id
            << "\",\"dev_path\":\"" << nodes[i].dev_path
            << "\",\"online\":" << (nodes[i].online ? "true" : "false") << "}";
        if (i + 1 < nodes.size()) ofs << ",";                 // avoid trailing comma
        ofs << "\n";
    }
    ofs << "]\n";
    return true;
}


/*
 * create_symlinks()
 * -----------------
 * Create runtime-friendly symlinks for online nodes:
 *   <runtime_dir>/viatext-node-<ID> -> <device path>
 *
 * Behavior details:
 * - Ensures the runtime directory exists.
 * - Replaces existing symlinks quietly.
 * - Skips entries with empty IDs or offline==false.
 *
 * Why symlinks:
 * - Scripts can target nodes by ID without caring about current /dev name.
 */
bool create_symlinks(const std::vector<NodeInfo>& nodes) {
    std::error_code ec;
    fs::path dir = runtime_dir();
    fs::create_directories(dir, ec);
    if (ec) { std::cerr << "alias dir error: " << ec.message() << "\n"; return false; }

    for (const auto& n : nodes) {
        if (!n.online || n.id.empty()) continue;              // only link reachable, named nodes
        fs::path link = dir / ("viatext-node-" + n.id);
        if (fs::exists(link, ec)) fs::remove(link, ec);       // best-effort remove old link
        fs::create_symlink(n.dev_path, link, ec);             // create new link to current device
        if (ec) { std::cerr << "alias failed for " << n.id << ": " << ec.message() << "\n"; return false; }
    }
    return true;
}

} // namespace viatext
