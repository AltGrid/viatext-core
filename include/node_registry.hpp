#pragma once
/**
 * @file node_registry.hpp
 * @brief Discovery, tracking, and aliasing of ViaText nodes connected via serial.
 *
 * @details
 *  - Enumerates candidate devices under `/dev/serial/by-id` (or falls back to /dev/ttyUSB*, /dev/ttyACM*)
 *  - Probes each device for its ViaText node ID (`make_get_id`)
 *  - Writes registry to `~/.config/altgrid/viatext/nodes.json`
 *  - Optionally creates symlinks in `/run` like `/run/viatext-node-<id>`
 *
 * Linux-first design, works on Debian, Fedora, Arch, etc. without libudev.
 *
 * @note This is a core reliability feature — if device detection fails, ViaText
 *       becomes unusable for multi-node operation.
 *
 * @author Leo
 * @author ChatGPT
 */

#include <string>
#include <vector>

namespace viatext {

struct NodeInfo {
    std::string id;       ///< Unique ViaText node ID
    std::string dev_path; ///< Full device path (e.g., /dev/serial/by-id/usb-...)
    bool online;          ///< Whether the node responded to probe
};

std::vector<NodeInfo> discover_nodes();
bool save_registry(const std::vector<NodeInfo>& nodes);
bool create_symlinks(const std::vector<NodeInfo>& nodes);

} // namespace vt
