#pragma once
/**
 * @page vt-node-registry ViaText Node Registry
 * @file node_registry.hpp
 * @brief Discovery, tracking, and aliasing of ViaText nodes connected via serial.
 *
 * @details
 * PURPOSE
 * -------
 * This header defines the public interface for discovering ViaText nodes attached to a Linux host,
 * probing them for identity, and recording that state in a persistent registry. Without this step,
 * higher layers cannot reliably target nodes by ID. It is the "roster" that lets the rest of the
 * system know which radios or devices are present and usable.
 *
 * WHAT THIS DOES
 * --------------
 * - Scans the host for serial devices likely to be ViaText nodes.
 *   - Prefers stable symlinks under `/dev/serial/by-id` (which persist across reboots and port swaps).
 *   - Falls back to `/dev/ttyUSB*` or `/dev/ttyACM*` if `/dev/serial/by-id` is not available.
 * - Probes each candidate by opening the port and issuing a "get-id" command using
 *   viatext::make_get_id() (from commands.hpp). This ensures the device is actually running ViaText firmware.
 * - Collects the results into a vector of NodeInfo structs containing:
 *     * the node’s reported ID,
 *     * the device path,
 *     * and whether it responded to probe (online flag).
 * - Provides helpers to:
 *     * Save that registry to disk (`nodes.json`) under `$HOME/.config/altgrid/viatext/`.
 *     * Create runtime symlinks under `$XDG_RUNTIME_DIR/viatext/` or `/run/user/<uid>/viatext/`
 *       such as `/run/viatext-node-N3` → `/dev/ttyACM0`. These symlinks provide stable handles
 *       for scripting and automation.
 *
 * HOW IT FITS INTO VIATEXT
 * ------------------------
 * - CLI tools call discover_nodes() at startup to map physical devices to logical node IDs.
 * - The registry file allows long-running processes and scripts to recall node layout across sessions.
 * - Symlinks simplify human and system targeting, so you can refer to `/run/viatext-node-N20`
 *   without guessing which /dev path it currently has.
 *
 * RELIABILITY AND TRADE-OFFS
 * --------------------------
 * - No libudev dependency: everything is done with filesystem inspection and globbing, making it
 *   portable across Debian, Fedora, Arch, and similar distros.
 * - Probing costs time (~1 second per candidate). In exchange, we avoid misidentifying
 *   unrelated USB devices as ViaText nodes.
 * - If device detection fails, ViaText multi-node operation is crippled. For this reason,
 *   discovery and registry maintenance are considered a core reliability feature.
 *
 * OPERATIONAL NOTES
 * -----------------
 * - The registry JSON is simple and human-editable; it can be inspected offline in a bunker or
 *   during debugging. No external tools are required.
 * - Symlinks in `/run` are ephemeral. They disappear on reboot; always regenerate them at startup.
 * - Users must have permission to access the device nodes (`dialout` group or similar).
 *
 * EXAMPLE
 * -------
 * @code
 *   // Discover devices
 *   auto nodes = viatext::discover_nodes();
 *
 *   // Persist results
 *   if (!viatext::save_registry(nodes)) {
 *       std::cerr << "Failed to save node registry\n";
 *   }
 *
 *   // Create symlinks for easy reference
 *   if (!viatext::create_symlinks(nodes)) {
 *       std::cerr << "Symlink creation failed\n";
 *   }
 *
 *   // Use NodeInfo vector directly
 *   for (const auto& n : nodes) {
 *       if (n.online) {
 *           std::cout << "Node " << n.id << " at " << n.dev_path << " is online\n";
 *       }
 *   }
 * @endcode
 *
 * DEPENDENCIES
 * ------------
 * - serial_io.hpp (for probing ports).
 * - commands.hpp (for building/decoding the get-id command).
 * - C++17 filesystem and iostream headers for registry and symlink management.
 *
 * @note This is Linux-first design. Other OS support would require reimplementing
 *       discovery and symlink logic.
 *
 * @author Leo
 * @author ChatGPT
 */

#include <string>
#include <vector>

namespace viatext {

/**
 * @struct NodeInfo
 * @brief Minimal record describing a discovered ViaText node.
 *
 * Role in the system:
 *   NodeInfo is the registry unit. Each instance binds together a node's
 *   reported identity and the Linux device path it was found on. Tools
 *   use this to correlate "who" (id) with "where" (dev_path) and "can we
 *   talk right now" (online).
 *
 * Field utility:
 *   In field deployments, device files can shift after reboots or cable
 *   swaps. Keeping both the human-stable id and the volatile dev_path
 *   lets scripts re-target reliably and quickly rebuild symlinks.
 */
struct NodeInfo {
    std::string id;       /**< Unique ViaText node ID reported by the device (e.g., "N3"). */
    std::string dev_path; /**< Absolute device path on Linux (e.g., "/dev/serial/by-id/usb-..."). */
    bool online;          /**< True if the node responded to probe during discovery. */
};


/**
 * @brief Discover ViaText nodes attached to this Linux host.
 *
 * Operation:
 *   - Scans candidate serial devices (preferring /dev/serial/by-id).
 *   - Probes each device by opening the port and issuing a GET_ID request.
 *   - Returns a vector of NodeInfo entries with id/dev_path/online set.
 *
 * Why it matters:
 *   Downstream tools need a reliable roster before they can target nodes
 *   by id. This function is the first step every CLI or service should run.
 *
 * Returns:
 *   - An empty vector does not necessarily mean failure; it may simply mean
 *     no nodes are connected or none responded within timeout.
 *
 * Side effects:
 *   - Opens serial ports briefly for probing.
 *
 * Example:
 * @code
 *   auto nodes = viatext::discover_nodes();
 *   for (const auto& n : nodes) {
 *       std::cout << (n.online ? "[up]   " : "[down] ")
 *                 << n.id << " -> " << n.dev_path << "\n";
 *   }
 * @endcode
 *
 * @return List of discovered NodeInfo records.
 */
std::vector<NodeInfo> discover_nodes();


/**
 * @brief Persist the discovered node registry to disk as JSON.
 *
 * Purpose:
 *   Saves a simple, human-readable snapshot (e.g., ~/.config/altgrid/viatext/nodes.json)
 *   so other tools and later sessions know what was found and where.
 *
 * Notes:
 *   - The JSON format is intentionally minimal so you can read/edit it by hand.
 *   - Intended for Linux. Paths assume a typical $HOME layout.
 *
 * Failure modes:
 *   - Returns false if the config directory cannot be created or the file
 *     cannot be written.
 *
 * Example:
 * @code
 *   auto nodes = viatext::discover_nodes();
 *   if (!viatext::save_registry(nodes)) {
 *       std::cerr << "Could not save registry.\n";
 *   }
 * @endcode
 *
 * @param nodes The in-memory roster to serialize.
 * @return true on success, false on error.
 */
bool save_registry(const std::vector<NodeInfo>& nodes);


/**
 * @brief Create runtime symlinks for online nodes under XDG runtime dir.
 *
 * Purpose:
 *   Produces stable, script-friendly handles like:
 *     /run/user/<uid>/viatext/viatext-node-<ID> -> /dev/ttyACM0
 *   so you can target nodes by id without remembering volatile tty names.
 *
 * Behavior:
 *   - Only creates links for nodes with online == true and non-empty id.
 *   - Existing links are replaced.
 *
 * Practical use:
 *   After discovery, run this and then reference nodes as:
 *     $ picocom /run/user/$UID/viatext/viatext-node-N3
 *
 * Failure modes:
 *   - Returns false if the runtime directory cannot be created or if
 *     creating a symlink fails. Partial success is possible; check logs.
 *
 * Example:
 * @code
 *   auto nodes = viatext::discover_nodes();
 *   viatext::create_symlinks(nodes);
 * @endcode
 *
 * @param nodes The roster from discover_nodes().
 * @return true if all applicable symlinks were created, false on error.
 */
bool create_symlinks(const std::vector<NodeInfo>& nodes);


} // namespace vt
