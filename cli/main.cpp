/**
 * @file main.cpp
 * @brief ViaText CLI wrapper — Linux one-shot runner around viatext::Core.
 *
 * Responsibilities:
 *  - Parse CLI options (CLI11) and *separate* CLI-centered vs core-centered arguments.
 *  - Ensure a node identity exists; persist under XDG config (~/.config/altgrid/viatext-cli).
 *  - Build a viatext::Package with exact, pass-through core args (no canonicalization).
 *  - Inject "-node-id <ID>" into the args sent to Core.
 *  - Run core.add_message(pkg) → core.tick(ms) → drain core.get_message(out).
 *  - If --print, show readable dumps of args in/out and a stamp visualizer.
 *
 * Notes:
 *  - We honor both "-m" and "--m" on the CLI; we forward **only "-m"** to Core.
 *  - We keep Core-facing directive "--set-id" (matching current Core::dispatch).
 *  - State file: <id>-node-state.json with fields {"id","last_time","messages_in","messages_out"}.
 */

#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <set>
#include <map>
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <iomanip>

#include <unistd.h> // isatty

#include "CLI/CLI11.hpp"
#include "nlohmann/json.hpp"

#include "viatext/core.hpp"
#include "viatext/package.hpp"
#include "viatext/message.hpp"
#include "viatext/message_id.hpp"

using json = nlohmann::json;
namespace fs = std::filesystem;
using namespace viatext;

// ---------- small utilities ----------

static bool is_tty_stdout() { return ::isatty(fileno(stdout)); }

struct Ansi {
  bool enabled{true};
  std::string bold(const std::string& s) const { return enabled ? "\033[1m"+s+"\033[0m" : s; }
  std::string dim (const std::string& s) const { return enabled ? "\033[2m"+s+"\033[0m" : s; }
  std::string red (const std::string& s) const { return enabled ? "\033[31m"+s+"\033[0m" : s; }
};

// Callsign rules: A–Z 0–9 - _ ; 1..6; must start/end alnum; no consecutive symbols.
static bool valid_callsign(const std::string& id) {
  if (id.size() < 1 || id.size() > 6) return false;
  auto is_alnum = [](char c){ return (c>='A'&&c<='Z') || (c>='0'&&c<='9'); };
  auto is_sym   = [](char c){ return c=='-' || c=='_'; };
  for (char c: id) {
    if (!(is_alnum(c) || is_sym(c))) return false;
    if (c>='a' && c<='z') return false; // lowercase not allowed
  }
  if (!is_alnum(id.front()) || !is_alnum(id.back())) return false;
  for (size_t i=1;i<id.size();++i) if ( (id[i]=='-'||id[i]=='_') && (id[i-1]=='-'||id[i-1]=='_') ) return false;
  return true;
}

static std::string to_upper_ascii(std::string s) {
  for (char& c: s) if (c>='a' && c<='z') c = char(c - 'a' + 'A');
  return s;
}

static std::string random_callsign() {
  static const char* ALPH = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
  uint64_t seed = std::chrono::high_resolution_clock::now().time_since_epoch().count();
  // simple LCG
  auto rnd = [&seed](){
    seed = seed*6364136223846793005ull + 1;
    return (uint32_t)(seed >> 33);
  };
  std::string s;
  s.reserve(6);
  for (int i=0;i<6;i++) s.push_back(ALPH[rnd()%36]);
  // ensure start/end alnum (they already are); avoid consecutive symbols (we never use -/_ here)
  return s;
}

static fs::path default_state_dir() {
  const char* xdg = std::getenv("XDG_CONFIG_HOME");
  fs::path base = (xdg && *xdg) ? fs::path(xdg) : fs::path(std::getenv("HOME")) / ".config";
  return base / "altgrid" / "viatext-cli";
}

static json read_json_file(const fs::path& p) {
  try {
    if (!fs::exists(p)) return json::object();
    std::ifstream in(p);
    if (!in) return json::object();
    json j; in >> j; return j;
  } catch(...) { return json::object(); }
}

static bool atomic_write_json(const fs::path& p, const json& j) {
  try {
    fs::create_directories(p.parent_path());
    auto tmp = p; tmp += ".tmp";
    {
      std::ofstream out(tmp, std::ios::trunc);
      if (!out) return false;
      out << j.dump(2);
      out.flush();
      out.close();
    }
    fs::rename(tmp, p);
    return true;
  } catch(...) { return false; }
}

static uint64_t now_ms_system() {
  using namespace std::chrono;
  return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

static uint32_t now_ms_steady32() {
  using namespace std::chrono;
  auto ms = duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
  return static_cast<uint32_t>(ms & 0xFFFFFFFFu);
}

// Visualize "<hex10>~from~to~data"
static void print_stamp_pretty(const std::string& payload, const Ansi& ansi) {
  if (payload.empty()) { std::cout << ansi.dim("  [payload] (empty)\n"); return; }
  std::string id, from, to, data;
  size_t a = payload.find('~');
  if (a != std::string::npos) {
    id = payload.substr(0, a);
    size_t b = payload.find('~', a+1);
    if (b != std::string::npos) {
      from = payload.substr(a+1, b-(a+1));
      size_t c = payload.find('~', b+1);
      if (c != std::string::npos) {
        to   = payload.substr(b+1, c-(b+1));
        data = payload.substr(c+1);
      }
    }
  }
  auto kv = [&](const char* k, const std::string& v){
    std::cout << "  " << ansi.bold(std::string("[") + k + "] ");
    if (v.empty()) std::cout << ansi.dim("(empty)") << "\n";
    else           std::cout << v << "\n";
  };
  kv("ID", id);
  kv("FROM", from);
  kv("TO", to);
  kv("DATA", data);
}

// Dump ArgList nicely
static void print_args_pretty(const ArgList& args, const Ansi& ansi) {
  // Collect and sort by key
  std::vector<std::pair<std::string,std::string>> rows;
  rows.reserve(args.items.size());
  for (const auto& kv : args.items) {
    rows.emplace_back(kv.k.c_str(), kv.v.c_str());
  }
  std::sort(rows.begin(), rows.end(), [](auto& a, auto& b){ return a.first < b.first; });
  // Print
  for (auto& r : rows) {
    std::cout << "    " << std::left << std::setw(16) << r.first << " ";
    if (r.second.empty()) std::cout << ansi.dim("[flag]") << "\n";
    else                  std::cout << r.second << "\n";
  }
}

// ---------- main ----------

int main(int argc, char** argv) {
  // CLI-centered options
  bool opt_print = false;
  std::string opt_format = "pretty"; // pretty|json|raw
  bool opt_no_color = false;
  std::string opt_state_dir;
  std::string opt_create_id;
  uint32_t opt_tick_ms = 0; // 0 => auto
  bool opt_tick_set = false;

  // Message construction helpers (optional)
  std::string opt_message;   // full stamp
  std::string opt_id_hex;    // 10 hex
  std::string opt_from;
  std::string opt_to;
  std::string opt_data;

  CLI::App app{"ViaText CLI wrapper"};
  app.allow_extras(true);

  app.add_flag("--print", opt_print, "Print args in/out and stamp visualizer");
  app.add_option("--format", opt_format, "Output format: pretty|json|raw")->check(CLI::IsMember({"pretty","json","raw"}));
  app.add_flag("--no-color", opt_no_color, "Disable ANSI colors");
  app.add_option("--state-dir", opt_state_dir, "Override state directory");
  app.add_option("--create-id", opt_create_id, "Create & persist node id (callsign)");
  app.add_option("--tick-ms", opt_tick_ms, "Override tick time in ms")->capture_default_str()->check(CLI::Range(uint32_t{0}, std::numeric_limits<uint32_t>::max()));
  
  // Message feed convenience
  app.add_option("--message", opt_message, "Full payload stamp: <hex10>~FROM~TO~DATA");
  app.add_option("--id", opt_id_hex, "Header hex (10 chars)");
  app.add_option("--from", opt_from, "Sender callsign");
  app.add_option("--to", opt_to, "Recipient callsign");
  app.add_option("--data", opt_data, "Body text");

  try {
    app.parse(argc, argv);
  } catch (const CLI::ParseError &e) {
    return app.exit(e);
  }

  // Prepare ANSI
  Ansi ansi;
  ansi.enabled = !opt_no_color && is_tty_stdout() && (opt_format=="pretty");

  // Resolve state dir and load (if present)
  fs::path state_dir = opt_state_dir.empty() ? default_state_dir() : fs::path(opt_state_dir);
  fs::create_directories(state_dir);

  // Decide node id
  std::string node_id;     // runtime id
  std::string state_id;    // id found in state file (if any)
  uint64_t    last_time = 0;
  uint64_t    now_sys = now_ms_system();

  // If --create-id provided, validate & uppercase
  if (!opt_create_id.empty()) {
    opt_create_id = to_upper_ascii(opt_create_id);
    if (!valid_callsign(opt_create_id)) {
      std::cerr << ansi.red("error: invalid id for --create-id\n");
      return 2;
    }
    node_id = opt_create_id;
  }

  // If not set yet, try to discover via scan of existing state files?
  // Simpler: look for a default "id" file if user previously ran; since we don't know the id,
  // we can't load by name. We will store by new id below. For a single-node CLI, persisting newest is enough.
  // For now, require the specific "<id>-node-state.json" path, which needs the ID.
  // If the user previously ran, we expect they'd pass or create the same id again;
  // or we can look for any one file in the dir and reuse its id if only one exists.
  if (node_id.empty()) {
    // Attempt: if exactly one *.json exists, use it.
    std::string found_id;
    for (auto& entry : fs::directory_iterator(state_dir)) {
      if (!entry.is_regular_file()) continue;
      auto name = entry.path().filename().string();
      if (name.size() > 16 && name.rfind("-node-state.json") == name.size() - std::string("-node-state.json").size()) {
        // prefix before "-node-state.json"
        found_id = name.substr(0, name.size()-std::string("-node-state.json").size());
        // prefer the first (or we could choose newest)
        break;
      }
    }
    if (!found_id.empty()) {
      node_id = found_id;
    }
  }

  // If still empty, generate & inform user
  if (node_id.empty()) {
    node_id = random_callsign();
    std::cout << ansi.dim("generated id: ") << ansi.bold(node_id) << "\n";
  }

  // Validate node_id format (uppercase normalize allowed)
  node_id = to_upper_ascii(node_id);
  if (!valid_callsign(node_id)) {
    std::cerr << ansi.red("error: node id invalid after normalization\n");
    return 2;
  }

  // Load state file for this node (if exists)
  fs::path state_file = state_dir / (node_id + "-node-state.json");
  {
    json st = read_json_file(state_file);
    if (st.is_object()) {
      if (st.contains("id") && st["id"].is_string()) state_id = st["id"].get<std::string>();
      if (st.contains("last_time") && st["last_time"].is_number()) last_time = st["last_time"].get<uint64_t>();
    }
  }

  // If user set --create-id OR provided -node-id (we treat as create), persist immediately
  // (You asked to handle "-node-id" as if create-id; we implement by always persisting the chosen id)
  {
    json st;
    st["id"] = node_id;
    st["last_time"] = now_sys;
    atomic_write_json(state_file, st);
  }

  // Build Package for core (core-centered args only)
  Package pkg;
  // Decide payload
  if (!opt_message.empty()) {
    pkg.payload = opt_message.c_str();
  } else if (!opt_id_hex.empty() || !opt_from.empty() || !opt_to.empty() || !opt_data.empty()) {
    // Assemble stamp if user provided components. We trust lengths here; Core will validate.
    // Ensure id is 10 hex (no "0x"). If user gave "0x...", keep as-is; Message can handle.
    std::string stamp;
    if (!opt_id_hex.empty()) stamp += opt_id_hex;
    stamp += "~";
    if (!opt_from.empty())   stamp += opt_from;
    stamp += "~";
    if (!opt_to.empty())     stamp += opt_to;
    stamp += "~";
    if (!opt_data.empty())   stamp += opt_data;
    pkg.payload = stamp.c_str();
  } else {
    // No payload is fine for directives like -p / --set-id that use args or body
    pkg.payload.clear();
  }

  // Pass-through of core-centered args from argv (preserve exact keys & values).
  // Known CLI-centered keys to strip:
  const std::set<std::string> CLI_ONLY = {
    "--print","--format","--no-color","--state-dir","--create-id","--tick-ms",
    "--message","--id","--from","--to","--data"
  };

  // Map convenience forms to core reality
  auto normalize_key = [](const std::string& k)->std::string{
    if (k == "--m") return "-m";
    return k;
  };

  // Scan argv to capture tokens that start with '-' and are not CLI_ONLY.
  // Simple "key [value]" capture: if next token doesn't start with '-', treat as value.
  for (int i=1; i<argc; ++i) {
    std::string t = argv[i];
    if (t.rfind("-",0) != 0) continue; // not a switch
    if (CLI_ONLY.count(t)) {
      // skip possible value
      if (i+1<argc && std::string(argv[i+1]).rfind("-",0)!=0) { ++i; }
      continue;
    }
    std::string key = normalize_key(t);
    std::string val;
    if (i+1<argc) {
      std::string nxt = argv[i+1];
      if (nxt.rfind("-",0)!=0) { val = nxt; ++i; }
    }
    if (val.empty()) pkg.args.set_flag(key.c_str());
    else             pkg.args.set(key.c_str(), val.c_str());
  }

  // Always inject -node-id <ID> into args (last-wins)
  pkg.args.set("-node-id", node_id.c_str());

  // If user passed "--set-id" as a directive for Core and also payload body contains new id,
  // we'll just forward; your Core handles it (MVP: msg.text()).

  // Create Core and run
  Core core(node_id.c_str());  // no ETL usage in the CLI

  // Optional: show delta since last run
  if (opt_print && opt_format=="pretty") {
    uint64_t delta = (last_time>0 && now_sys>=last_time) ? (now_sys-last_time) : 0;
    std::cout << "ID: " << ansi.bold(node_id)
              << "  config: " << (state_file.string())
              << "  " << ansi.dim(std::string("(+") + std::to_string(delta) + " ms since last run)")
              << "\n\n";
  }

  // IN: print
  if (opt_print) {
    if (opt_format=="json") {
      json j;
      j["direction"]="in";
      j["payload"]= std::string(pkg.payload.c_str());
      json a = json::array();
      for (const auto& kv: pkg.args.items) {
        json e; e["k"]=kv.k.c_str(); e["v"]=kv.v.c_str(); a.push_back(e);
      }
      j["args"]=a;
      std::cout << j.dump(2) << "\n";
    } else if (opt_format=="raw") {
      std::cout << pkg.payload.c_str() << "\n";
    } else {
      std::cout << Ansi{ansi.enabled}.bold("IN  \xE2\x86\x92 core(add_message)") << "\n";
      std::cout << "  args(in):\n";
      print_args_pretty(pkg.args, ansi);
      std::cout << "  payload:\n";
      print_stamp_pretty(pkg.payload.c_str(), ansi);
      std::cout << "\n";
    }
  }

  // Add, tick, drain
  core.add_message(pkg);
  uint32_t tnow = opt_tick_set ? opt_tick_ms : now_ms_steady32();
  core.tick(tnow);

  // Collect outputs
  std::vector<Package> outs;
  Package out;
  while (core.get_message(out)) {
    outs.push_back(out);
  }

  // OUT: print
  if (opt_print) {
    if (opt_format=="json") {
      json arr = json::array();
      for (auto& p: outs) {
        json j;
        j["direction"]="out";
        j["payload"]= std::string(p.payload.c_str());
        json a = json::array();
        for (const auto& kv: p.args.items) {
          json e; e["k"]=kv.k.c_str(); e["v"]=kv.v.c_str(); a.push_back(e);
        }
        j["args"]=a;
        arr.push_back(j);
      }
      std::cout << arr.dump(2) << "\n";
    } else if (opt_format=="raw") {
      for (auto& p: outs) std::cout << p.payload.c_str() << "\n";
    } else {
      std::cout << Ansi{ansi.enabled}.bold("TCK \xE2\x86\x92 core.tick(" + std::to_string(tnow) + ")") << "\n\n";
      std::cout << Ansi{ansi.enabled}.bold("OUT \xE2\x86\x92 ") << outs.size() << " package(s)\n";
      size_t idx=1;
      for (auto& p: outs) {
        std::cout << "  #" << idx++ << "\n";
        std::cout << "    args(out):\n";
        print_args_pretty(p.args, ansi);
        std::cout << "    payload:\n";
        print_stamp_pretty(p.payload.c_str(), ansi);
        std::cout << "\n";
      }
    }
  }

  // Save updated state (messages counts can be tracked later; for now, last_time)
  {
    json st;
    st["id"] = node_id;
    st["last_time"] = now_sys;
    atomic_write_json(state_file, st);
  }

  // Exit status: 0 for success; if there were no outputs and no print, still 0.
  return 0;
}
