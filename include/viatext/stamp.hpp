/**
 * @file Stamp.hpp
 * @brief Standalone routing envelope class for ViaText messages.
 * @details
 * The Stamp class encapsulates routing metadata in a self-contained text format:
 *   @code
 *   <id>|<from-list>|<to-list>|<message>
 *   @endcode
 *
 * Where:
 *  - <id>        : Unique message identifier (hex string).
 *  - <from-list> : Colon-separated node IDs the message has already traversed (empty at origin).
 *  - <to-list>   : Colon-separated node IDs the message is yet to traverse (empty at destination).
 *  - <message>   : The payload string, always last.
 *
 * Routing Behavior:
 *  Each node performing a relay calls shift_route(my_id), which moves the first element
 *  of the to-list into the end of the from-list if it matches my_id, then re-serializes.
 *  No external routing state is required.
 *
 * Example Sequence:
 *   Origin sends:    42A1F9||A1:B3:C4|Hello
 *   Node A1 shifts:  42A1F9|A1|B3:C4|Hello
 *   Node B3 shifts:  42A1F9|A1:B3|C4|Hello
 *   Node C4 shifts:  42A1F9|A1:B3:C4||Hello  (delivered)
 *
 * @note All methods use only C++ standard library components.
 */

#pragma once

#include <string>
#include <vector>
#include <sstream>

namespace viatext {

/**
 * @class Stamp
 * @brief Data model for self-routing ViaText envelope.
 * @details
 * Represents the complete routing envelope, including unique ID, history, future path,
 * and payload. Designed for stateless, text-based multihop routing.
 */
class Stamp {
public:
    /** @brief Unique message identifier (e.g., "42A1F9"). */
    std::string id;
    /** @brief Nodes that have already processed the message (FIFO order). */
    std::vector<std::string> from;
    /** @brief Nodes scheduled to receive the message next (FIFO order). */
    std::vector<std::string> to;
    /** @brief The message payload (user data). */
    std::string message;

    /**
     * @brief Parse a raw on-wire string into a Stamp object.
     * @param raw  The full wire format: "<id>|<from>|<to>|<message>".
     * @return Stamp instance populated with parsed segments.
     * @details
     *   Splits the input on '|' into four segments, then splits the
     *   `from` and `to` segments on ':' into vectors. Missing segments
     *   yield empty strings or vectors.
     */
    static Stamp from_message(const std::string& raw);

    /**
     * @brief Serialize the Stamp into the raw on-wire format.
     * @return A string: "<id>|<from-list>|<to-list>|<message>".
     */
    std::string to_message() const;

    /**
     * @brief Perform routing shift: move `my_id` from the front of `to` to `from`.
     * @param my_id  ID of the node performing the relay.
     * @details
     *   If the first element of the `to` vector equals `my_id`, it is removed
     *   from `to` and appended to `from`.
     */
    void shift_route(const std::string& my_id);

    /**
     * @brief Check if this node is the next destination.
     * @param my_id  ID of the current node.
     * @return true if `to` is non-empty and its first element matches `my_id`.
     */
    bool is_final_destination(const std::string& my_id) const;

private:
    /**
     * @brief Split a colon-delimited string into vector segments.
     * @param s    Input string (e.g., "A1:B2:C3").
     * @param out  Vector to populate with split elements.
     */
    static void split_colon(const std::string& s, std::vector<std::string>& out);

    /**
     * @brief Join a vector of strings into a colon-delimited string.
     * @param v  Vector of strings.
     * @return Colon-separated string (e.g., "A1:B2:C3").
     */
    static std::string join_colon(const std::vector<std::string>& v);
};

// ===========================================================================
//                      Inline Implementations
// ===========================================================================

inline Stamp Stamp::from_message(const std::string& raw) {
    Stamp s;
    size_t pos = 0, next;
    std::vector<std::string> parts;
    // Split raw into up to 4 segments by '|'
    while (parts.size() < 3 && (next = raw.find('|', pos)) != std::string::npos) {
        parts.emplace_back(raw.substr(pos, next - pos));
        pos = next + 1;
    }
    // Capture final segment (message) or default
    if (parts.size() == 3) {
        parts.emplace_back(raw.substr(pos));
    }
    // Assign parsed fields
    s.id = parts.size() > 0 ? parts[0] : "";
    split_colon(parts.size() > 1 ? parts[1] : "", s.from);
    split_colon(parts.size() > 2 ? parts[2] : "", s.to);
    s.message = parts.size() > 3 ? parts[3] : "";
    return s;
}

inline std::string Stamp::to_message() const {
    std::ostringstream oss;
    oss << id << '|' << join_colon(from) << '|' << join_colon(to) << '|' << message;
    return oss.str();
}

inline void Stamp::shift_route(const std::string& my_id) {
    if (!to.empty() && to.front() == my_id) {
        from.push_back(my_id);
        to.erase(to.begin());
    }
}

inline bool Stamp::is_final_destination(const std::string& my_id) const {
    return !to.empty() && to.front() == my_id;
}

inline void Stamp::split_colon(const std::string& s, std::vector<std::string>& out) {
    size_t start = 0, end;
    while ((end = s.find(':', start)) != std::string::npos) {
        out.emplace_back(s.substr(start, end - start));
        start = end + 1;
    }
    if (start < s.size()) out.emplace_back(s.substr(start));
}

inline std::string Stamp::join_colon(const std::vector<std::string>& v) {
    std::ostringstream oss;
    for (size_t i = 0; i < v.size(); ++i) {
        if (i) oss << ':';
        oss << v[i];
    }
    return oss.str();
}

} // namespace viatext


