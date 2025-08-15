#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>          // getenv
#include <sys/types.h>      // getuid
#include <unistd.h>         // access(), getuid
#include <cstdint>
#include "CLI11.hpp"

#include "command_dispatch.hpp"   // build_* dispatcher helpers
#include "commands.hpp"           // decode_pretty()
#include "serial_io.hpp"          // open_serial(), write_frame(), read_frame(), close_serial()
#include "node_registry.hpp"      // discover_nodes(), save_registry(), create_symlinks()

// Resolve alias path in user runtime dir:
//   $XDG_RUNTIME_DIR/viatext/viatext-node-<id>
//   fallback: /run/user/$UID/viatext/viatext-node-<id>
static std::string alias_for(const std::string& id) {
  const char* x = std::getenv("XDG_RUNTIME_DIR");
  std::string base = (x && *x) ? std::string(x)
                               : (std::string("/run/user/") + std::to_string(getuid()));
  return base + "/viatext/viatext-node-" + id;
}

int main(int argc, char** argv) {
  CLI::App app{"ViaText CLI"};

  // ---- legacy commands ----
  bool get_id=false, ping=false, do_scan=false, make_aliases=false;
  std::string set_id;

  // ---- new generic param API ----
  std::string get_name;                 // --get <name>
  std::vector<std::string> set_kv;      // --set <name> <value>

  // ---- targeting / device ----
  std::string node_id;                  // --node <id>
  std::string dev="/dev/ttyACM0";       // --dev <path>
  CLI::Option* opt_dev = app.add_option("--dev", dev, "Serial device (e.g. /dev/serial/by-id/...)");

  // ---- io settings ----
  int timeout_ms=1500, baud=115200, boot_delay_ms=400;

  // legacy flags
  app.add_flag("--get-id", get_id, "Query node ID (legacy)");
  app.add_flag("--ping",   ping,    "Ping device");
  app.add_option("--set-id", set_id, "Set node ID (e.g. vt-01)");

  // generic param api
  app.add_option("--get", get_name,
    "Get param: id|alias|fw|uptime|boot_time|freq|sf|bw|cr|tx_pwr|chan|mode|hops|beacon|buf_size|ack|rssi|snr|vbat|temp|free_mem|free_flash|log_count|all");
  app.add_option("--set", set_kv, "Set param: --set <name> <value>")->expected(2);

  // discovery / targeting
  app.add_flag("--scan", do_scan, "Scan and list nodes (prints id/dev/online), saves registry");
  app.add_flag("--aliases", make_aliases,
               "With --scan: create $XDG_RUNTIME_DIR/viatext/viatext-node-<id> symlinks");
  app.add_option("--node", node_id, "Target node by ID (resolves device path)");

  // io tuning
  app.add_option("--timeout", timeout_ms, "Read timeout (ms)");
  app.add_option("--baud", baud, "Baud rate (default 115200)");
  app.add_option("--boot-delay", boot_delay_ms, "Delay after open (ms) to let USB reset");

  CLI11_PARSE(app, argc, argv);

  // -------- scan mode --------
  if (do_scan) {
    auto nodes = viatext::discover_nodes();
    for (const auto& n : nodes) {
      std::cout << "id=" << n.id
                << " dev=" << n.dev_path
                << " online=" << (n.online ? 1 : 0) << "\n";
    }
    viatext::save_registry(nodes);
    if (make_aliases) viatext::create_symlinks(nodes);
    return 0;
  }

  // -------- choose exactly one command (legacy OR generic) --------
  int cmds = 0;
  cmds += get_id ? 1 : 0;
  cmds += ping   ? 1 : 0;
  cmds += (!set_id.empty()) ? 1 : 0;
  cmds += (!get_name.empty()) ? 1 : 0;
  cmds += (set_kv.size()==2) ? 1 : 0;

  if (cmds != 1) {
    std::cerr << "status=error reason=need_exactly_one_command\n";
    return 2;
  }

  // ===== Target resolution =====
  const bool dev_explicit = (opt_dev && opt_dev->count() > 0);

  if (!node_id.empty()) {
    // Try existing alias first
    std::string link = alias_for(node_id);
    if (access(link.c_str(), R_OK) == 0) {
      dev = link;
    } else {
      // Live scan to resolve ID -> device
      auto nodes = viatext::discover_nodes();
      viatext::save_registry(nodes);
      bool found = false;
      for (const auto& n : nodes) {
        if (n.online && n.id == node_id) { dev = n.dev_path; found = true; break; }
      }
      if (!found) {
        std::cerr << "status=error reason=node_not_found id=" << node_id << "\n";
        return 4;
      }
    }
  } else if (!dev_explicit) {
    // No --node and no explicit --dev → auto-select via quick scan
    auto nodes = viatext::discover_nodes();
    viatext::save_registry(nodes);

    int online_count = 0;
    std::string last_dev;
    for (const auto& n : nodes) {
      if (n.online) { online_count++; last_dev = n.dev_path; }
    }

    if (online_count == 1) {
      dev = last_dev; // auto-pick single online node
    } else if (online_count > 1) {
      std::cerr << "status=error reason=multiple_nodes_connected need_target\n";
      for (const auto& n : nodes) {
        if (n.online) std::cerr << "candidate id=" << n.id << " dev=" << n.dev_path << "\n";
      }
      return 5;
    } else {
      std::cerr << "status=error reason=no_nodes_online\n";
      return 6;
    }
  }
  // else: explicit --dev provided → use as-is

  // -------- build request via dispatcher --------
  uint8_t seq = 1;
  std::vector<uint8_t> req;
  std::string derr;

  if (!get_name.empty()) {
    if (!viatext::build_param_get_packet(get_name, seq, req, derr)) {
      std::cerr << "status=error reason=" << derr << "\n"; return 2;
    }
  } else if (set_kv.size()==2) {
    if (!viatext::build_param_set_packet(set_kv[0], set_kv[1], seq, req, derr)) {
      std::cerr << "status=error reason=" << derr << "\n"; return 2;
    }
  } else {
    if (!viatext::build_legacy_packet(get_id, ping, set_id, seq, req, derr)) {
      std::cerr << "status=error reason=" << derr << "\n"; return 2;
    }
  }

  // -------- normal request/response over serial --------
  int fd = viatext::open_serial(dev, baud, boot_delay_ms);
  if (fd < 0) {
    std::cerr << "status=error reason=open_failed dev=" << dev << "\n";
    return 1;
  }

  if (!viatext::write_frame(fd, req)) {
    viatext::close_serial(fd);
    std::cerr << "status=error reason=write_failed\n";
    return 1;
  }

  std::vector<uint8_t> resp;
  if (!viatext::read_frame(fd, resp, timeout_ms)) {
    viatext::close_serial(fd);
    std::cerr << "status=error reason=timeout\n";
    return 3;
  }

  std::cout << viatext::decode_pretty(resp) << "\n";
  viatext::close_serial(fd);
  return 0;
}
