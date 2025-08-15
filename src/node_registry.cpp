/**
 * @file node_registry.cpp
 */

#include "node_registry.hpp"
#include "commands.hpp"    // viatext::make_get_id(), viatext::decode_pretty()
#include "serial_io.hpp"   // viatext::open_serial(), write_frame(), read_frame(), close_serial()

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <chrono>
#include <thread>
#include <fcntl.h>
#include <unistd.h>
#include <glob.h>
#include <cerrno>
#include <system_error>
#include <cstdlib>         // getenv
#include <cstring>         // strerror

namespace fs = std::filesystem;
namespace viatext {

static constexpr int PROBE_BAUD       = 115200;
static constexpr int PROBE_TIMEOUT_MS = 1200;   // ms per device (read_frame timeout)
static constexpr int PROBE_BOOT_MS    = 400;    // ms after open to let USB CDC reset

// -------- helpers --------

static std::string probe_id(const std::string& dev_path) {
    int fd = viatext::open_serial(dev_path, /*baud*/PROBE_BAUD, /*boot_delay_ms*/PROBE_BOOT_MS);
    if (fd < 0) return {};

    auto req = viatext::make_get_id(1);
    bool ok = viatext::write_frame(fd, req);

    std::vector<uint8_t> resp;
    if (ok) ok = viatext::read_frame(fd, resp, PROBE_TIMEOUT_MS);

    viatext::close_serial(fd);
    if (!ok) return {};

    // Parse "status=ok seq=N id=XYZ"
    auto line = viatext::decode_pretty(resp);
    auto pos  = line.find("id=");
    if (pos == std::string::npos) return {};
    return line.substr(pos + 3);
}

static void append_glob(std::vector<std::string>& out, const char* pattern) {
    glob_t g{};
    if (glob(pattern, 0, nullptr, &g) == 0) {
        for (size_t i = 0; i < g.gl_pathc; ++i) out.emplace_back(g.gl_pathv[i]);
    }
    globfree(&g);
}

static fs::path runtime_dir() {
    if (const char* x = std::getenv("XDG_RUNTIME_DIR"); x && *x)
        return fs::path(x) / "viatext";
    return fs::path("/run/user") / std::to_string(getuid()) / "viatext";
}

// -------- public API --------

std::vector<NodeInfo> discover_nodes() {
    std::vector<NodeInfo> result;
    std::vector<std::string> candidates;

    // Prefer stable paths
    const fs::path by_id("/dev/serial/by-id");
    if (fs::exists(by_id)) {
        for (const auto& e : fs::directory_iterator(by_id)) {
            if (!e.is_symlink()) continue;
            std::error_code ec;
            auto canon = fs::canonical(e.path(), ec);
            if (!ec) candidates.push_back(canon.string());
        }
    } else {
        // Fallbacks
        append_glob(candidates, "/dev/ttyACM*");
        append_glob(candidates, "/dev/ttyUSB*");
    }

    // Probe each candidate
    for (const auto& dev : candidates) {
        std::string id = probe_id(dev);
        result.push_back({id, dev, !id.empty()});
    }
    return result;
}

bool save_registry(const std::vector<NodeInfo>& nodes) {
    fs::path conf = fs::path(std::getenv("HOME") ? std::getenv("HOME") : "") / ".config/altgrid/viatext";
    std::error_code ec;
    fs::create_directories(conf, ec);
    if (ec) { std::cerr << "config dir error: " << ec.message() << "\n"; return false; }

    std::ofstream ofs(conf / "nodes.json");
    if (!ofs) { std::cerr << "open nodes.json failed\n"; return false; }

    ofs << "[\n";
    for (size_t i = 0; i < nodes.size(); ++i) {
        ofs << "  {\"id\":\"" << nodes[i].id
            << "\",\"dev_path\":\"" << nodes[i].dev_path
            << "\",\"online\":" << (nodes[i].online ? "true" : "false") << "}";
        if (i + 1 < nodes.size()) ofs << ",";
        ofs << "\n";
    }
    ofs << "]\n";
    return true;
}

bool create_symlinks(const std::vector<NodeInfo>& nodes) {
    std::error_code ec;
    fs::path dir = runtime_dir();
    fs::create_directories(dir, ec);
    if (ec) { std::cerr << "alias dir error: " << ec.message() << "\n"; return false; }

    for (const auto& n : nodes) {
        if (!n.online || n.id.empty()) continue;
        fs::path link = dir / ("viatext-node-" + n.id);
        if (fs::exists(link, ec)) fs::remove(link, ec);
        fs::create_symlink(n.dev_path, link, ec);
        if (ec) { std::cerr << "alias failed for " << n.id << ": " << ec.message() << "\n"; return false; }
    }
    return true;
}

} // namespace viatext
