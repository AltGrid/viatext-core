/**
 * @page vt-package ViaText Package & Argument Manager
 * @file package.hpp
 * @brief ViaText Package & Argument Manager — minimal, heap-free metadata container.
 *
 * @details
 * ## Overview
 * In ViaText, everything that moves through the core is treated as a small text message
 * plus a handful of arguments describing how to handle it. Radios, CLIs, serial bridges,
 * tests — all of them normalize their inputs into this same shape. That keeps the core
 * deterministic, portable, and easy to reason about when infrastructure is weak or absent.
 *
 * This header defines two tiny building blocks:
 *   - ArgList  : fixed-capacity list of arguments as key->value pairs.
 *                Keys are preserved exactly (e.g., "-m", "-rssi", "--mode").
 *                Only leading/trailing whitespace is trimmed. Empty value = presence flag.
 *   - Package  : the in-system message container: `payload` (0..255 bytes text) + `args`.
 *
 * No STL. No heap. Embedded-safe ETL containers. Works the same on Linux and microcontrollers.
 *
 * ## Why this exists
 * ViaText Core must be ingress-agnostic. It should not care whether bytes came from LoRa,
 * a UART, or a unit test. Converging on a single, compact type avoids transport-specific
 * branches leaking into core logic. Payload stays human-readable; metadata stays minimal.
 *
 * ## What this is NOT
 * - Not a parser: your wrapper (LoRa/CLI/Serial) is responsible for parsing inputs, then
 *   populating `Package` via `args.set(...)` / `args.set_flag(...)` and `payload`.
 * - Not a radio header: the on-air 5-byte MessageID lives in the radio frame. Wrappers may
 *   expose select header fields as arguments (e.g., "-seq", "-part") only if the core needs them.
 *
 * ## Data model
 * - payload    : `etl::string<255>` containing the expanded, in-system text (post-reassembly).
 * - args       : `ArgList` of key->value pairs; value may be empty to mean "present".
 * - identity   : keys are stored verbatim after trim; no renaming, no canonicalization.
 *
 * ## Ingress-agnostic flow (write -> tick -> read)
 * 1) Wrapper builds a `Package` with `payload` and `args`.
 * 2) Wrapper enqueues it: `core.add_message(pkg);`
 * 3) Host calls `core.tick(ms)` periodically.
 * 4) Wrapper drains outbound work via `core.get_message(next);`
 *
 * The core operates only on `payload` and `args`.
 *
 * ## Design choices
 * - Keys preserved: "-rssi" remains "-rssi"; "--data-length" remains "--data-length".
 * - Whitespace trimming only: leading/trailing spaces/tabs removed; interior spacing preserved.
 * - Flags as empty values: presence-only options store `v == ""`; check with `args.has(...)`.
 * - Linear scans over small N: O(n) lookups over `VT_ARGS_MAX` — simple and MCU-friendly.
 * - No exceptions, no dynamic allocation: predictable behavior under memory pressure.
 *
 * ## Tuning capacities
 * - VT_KEY_MAX   (default 32)   : max key length.
 * - VT_VAL_MAX   (default 128)  : max value length.
 * - VT_ARGS_MAX  (default 24)   : max stored arguments.
 * - Text255                     : payload capacity fixed at 255 by design.
 *
 * Keep these tight for embedded safety. If capacity is exceeded, `set()` returns false.
 *
 * ## Examples
 * ### LoRa ingress (after reassembly)
 * @code
 * viatext::Package p;
 * p.payload = "0x4F2B000131~SHREK~DONKEY~Shut Up";
 * p.args.set_flag("-m");              // presence-only
 * p.args.set("-rssi", "-92");
 * p.args.set("-snr",  "4.5");
 * p.args.set("-sf",   "7");
 * p.args.set("-bw",   "125");
 * p.args.set("--freq-mhz", "915.0");  // optional bridge detail
 * core.add_message(p);
 * @endcode
 *
 * ### Linux CLI ingress
 * @code
 * viatext::Package p;
 * p.payload = "HELLO WORLD";
 * p.args.set("--to",    "NODE1");
 * p.args.set("--from",  "PC1");
 * p.args.set("--ttl",   "4");
 * p.args.set_flag("--broadcast");
 * core.add_message(p);
 * @endcode
 *
 * ## Complexity and constraints
 * - Arg operations are O(n) with n <= VT_ARGS_MAX.
 * - Not thread-safe; guard externally if accessed from multiple contexts.
 * - Cross-platform: Linux and ESP32/Arduino via ETL, no STL.
 *
 * ## Testing checklist
 * - Trimming: `"   -rssi\t"` stores as `"-rssi"`.
 * - Replacement: calling `set("-rssi","-90")` twice updates in place.
 * - Flags: `set_flag("-m")` => `has("-m")` is true; `get("-m")` returns empty string.
 * - Capacity: fill to VT_ARGS_MAX and verify final `set()` fails gracefully.
 *
 * ## Field notes
 * Keep arguments honest. If your wrapper invents new keys, keep them short and clear.
 * If data needs structure, encode it in the payload as text. Let the core stay small.
 *
 * @authors
 * @author Leo
 * @author ChatGPT
 */
#pragma once
#include "etl/string.h"
#include "etl/vector.h"
#include <stdint.h>
#include <stddef.h>

namespace viatext {

// -----------------------------------------------------------------------------
// Tunables — keep these tight for MCU safety.
// -----------------------------------------------------------------------------

/**
 * @section vt-package-tunables Tunable Limits
 * @brief Capacity limits for keys, values, and argument count.
 *
 * @details
 * These constants define the fixed-size limits for all `ArgList` storage inside
 * ViaText Core. They are deliberately tight to ensure:
 *
 * - MCU safety: predictable memory footprint; no heap use.
 * - Deterministic behavior: no reallocations or fragmentation.
 * - Portability: behaves the same on Linux and microcontrollers.
 *
 * Why fixed sizes?
 * - In low-resource or unstable environments, dynamic memory is a liability.
 * - ETL containers enforce capacity at compile time.
 * - When limits are hit, `set()` fails cleanly instead of allocating.
 *
 * When to adjust:
 * - VT_KEY_MAX   : Max characters in a key (including leading dashes).
 * - VT_VAL_MAX   : Max characters in a value. Use `payload` for bulk text.
 * - VT_ARGS_MAX  : Max key/value pairs stored. Larger values cost RAM and
 *                  increase O(n) scan time; test on your slowest target.
 *
 * Field note:
 * Keep these just high enough for your known argument space. In hostile or
 * power‑starved deployments, smaller is safer.
 */

/// @copydoc vt-package-tunables
static constexpr size_t VT_KEY_MAX  = 32;   ///< Maximum characters allowed in a key (e.g., "--data-length")

/// @copydoc vt-package-tunables
static constexpr size_t VT_VAL_MAX  = 128;  ///< Maximum characters allowed in a value string

/// @copydoc vt-package-tunables
static constexpr size_t VT_ARGS_MAX = 24;   ///< Maximum number of stored key/value entries


/**
 * @brief Internal text payload type for ViaText messages.
 *
 * @details
 * Fixed-capacity string (255 bytes) used to store the expanded in-system
 * message text after any reassembly or decoding has taken place.
 *
 * Design notes:
 * - Size is capped at 255 for predictable RAM use and MCU safety.
 * - Capacity matches the maximum payload the core will ever handle internally.
 * - Does not store the on-air 5-byte MessageID; that lives in the radio frame.
 *
 * This type is used in `Package::payload` and throughout the core for
 * transport-agnostic message text handling.
 */
using Text255 = etl::string<255>;

/**
 * @brief Argument key string type.
 *
 * @details
 * Fixed-capacity string sized by `VT_KEY_MAX` (default 32). Used for
 * storing metadata keys exactly as supplied (e.g., "-rssi", "--mode").
 *
 * Leading/trailing whitespace is trimmed on insertion, but the spelling
 * and case are preserved exactly.
 */
using KeyStr = etl::string<VT_KEY_MAX>;

/**
 * @brief Argument value string type.
 *
 * @details
 * Fixed-capacity string sized by `VT_VAL_MAX` (default 128). Used for
 * storing metadata values exactly as supplied, after trimming
 * leading/trailing whitespace.
 *
 * May be empty to represent a presence-only flag (see `ArgList::set_flag`).
 */
using ValStr = etl::string<VT_VAL_MAX>;


/**
 * @brief Single argument entry: key -> value.
 *
 * @details
 * This struct is the atomic unit of metadata inside an ArgList.
 * It holds exactly one key and one value:
 *
 * - k: Key string, preserved exactly as supplied (after trimming
 *       leading/trailing spaces or tabs).
 * - v: Value string, trimmed the same way. May be empty to indicate
 *       a presence-only flag.
 *
 * Design notes:
 * - Keys are not renamed or canonicalized. If the ingress provided "-m",
 *   it stays "-m".
 * - Whitespace trimming is minimal and non-destructive.
 * - Empty v means the key is a boolean flag; check with ArgList::has().
 *
 * Example:
 * @code
 * ArgKV entry;
 * entry.k = "-rssi";  // preserved exactly
 * entry.v = "-92";    // numeric value stored as string
 *
 * ArgKV flag;
 * flag.k = "-m";
 * flag.v = "";        // presence-only flag
 * @endcode
 */
struct ArgKV {
    KeyStr k;  ///< Key string, preserved as provided (after whitespace trim)
    ValStr v;  ///< Value string; empty => presence-only flag
};


/**
 * @brief Minimal, heap-free argument list with fixed capacity.
 *
 * @details
 * ArgList is a small container for key/value metadata entries, built for
 * predictable behavior in low-resource or embedded environments.
 *
 * Characteristics:
 * - Stores keys exactly as provided (no renaming or canonicalization).
 * - Trims only leading/trailing spaces and tabs from keys and values.
 * - Uses a fixed-capacity ETL vector; no dynamic allocation.
 * - Lookups are O(n) over a very small maximum size (VT_ARGS_MAX).
 *
 * Field notes:
 * - Best for short metadata lists where performance and RAM stability matter.
 * - Capacity is enforced at compile time; when full, set() fails cleanly.
 * - Not thread-safe; caller must guard if accessed from multiple contexts.
 *
 * Example usage:
 * @code
 * ArgList args;
 * args.set("-rssi", "-92");   // key -> value
 * args.set_flag("-m");        // presence-only flag
 * if (args.has("-m")) {
 *     // do something
 * }
 * @endcode
 */
struct ArgList {
    etl::vector<ArgKV, VT_ARGS_MAX> items;  ///< Storage for key/value entries

    /// @brief Trim leading and trailing spaces/tabs (in-place) for keys.
    static void trim_str(etl::string<VT_KEY_MAX>& s) {
        while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.erase(0, 1);
        while (!s.empty() && (s.back()  == ' ' || s.back()  == '\t')) s.pop_back();
    }

    /// @brief Trim leading and trailing spaces/tabs (in-place) for values.
    static void trim_str(etl::string<VT_VAL_MAX>& s) {
        while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.erase(0, 1);
        while (!s.empty() && (s.back()  == ' ' || s.back()  == '\t')) s.pop_back();
    }

    /**
     * @brief Set or replace a key→value pair.
     * @param key Raw key (kept exactly as provided after trimming). Must not be nullptr.
     * @param val Raw value (trimmed). May be nullptr to store an empty value.
     * @return true on success; false if capacity is full or key is null.
     *
     * If the key already exists, its value is replaced.
     */
    bool set(const char* key, const char* val) {
        if (!key) return false;
        KeyStr k_str = key; trim_str(k_str);
        ValStr v_str; if (val) { v_str = val; trim_str(v_str); }
        // Replace if present
        for (auto& kv : items) {
            if (kv.k == k_str) { kv.v = v_str; return true; }
        }
        // Insert if space remains
        if (items.full()) return false;
        items.push_back({k_str, v_str});
        return true;
    }

    /**
     * @brief Set a presence-only flag (stores an empty value).
     * @param key Raw key (kept exactly as provided after trimming).
     * @return true on success; false if capacity is full or key is null.
     */
    bool set_flag(const char* key) { return set(key, ""); }

    /**
     * @brief Check if a key exists.
     * @param key Key to search for (exact match after caller’s own formatting).
     * @return true if found.
     */
    bool has(const char* key) const {
        for (auto& kv : items) if (kv.k == key) return true;
        return false;
    }

    /**
     * @brief Get the stored value for a key (read-only).
     * @param key Key to search for.
     * @return Pointer to value string if found; nullptr otherwise.
     * @note For flags, the returned string is empty.
     */
    const ValStr* get(const char* key) const {
        for (auto& kv : items) if (kv.k == key) return &kv.v;
        return nullptr;
    }

    /**
     * @brief Remove a key (and its value) if present.
     * @param key Key to remove.
     * @return true if removed; false if not found.
     */
    bool remove(const char* key) {
        for (size_t i = 0; i < items.size(); ++i) {
            if (items[i].k == key) {
                items.erase(items.begin() + i);
                return true;
            }
        }
        return false;
    }

    /// @brief Number of stored entries.
    size_t size() const { return items.size(); }

    /// @brief Indexed access (read-only).
    const ArgKV& operator[](size_t i) const { return items[i]; }

    /// @brief Indexed access (mutable).
    ArgKV&       operator[](size_t i)       { return items[i]; }
};

/**
 * @brief A logical ViaText message inside the core: payload + arguments.
 *
 * - `payload` : expanded in-system text (0..255 bytes). This is NOT the on-air frame.
 * - `args`    : all metadata and control flags as an `ArgList`, with exact keys preserved.
 *
 * Wrappers (LoRa/CLI/Serial) populate a `Package` and submit it to the core.
 * The core’s rules operate on `payload` and `args` only, keeping logic ingress-agnostic.
 */
struct Package {
    Text255 payload;  ///< In-system message content (0..255 bytes)
    ArgList args;     ///< Metadata/control arguments (exact keys, trimmed whitespace)

    /// @brief Convenience checker for presence-only flags.
    /// @param key Exact key (e.g., "-m", "--broadcast").
    /// @return true if the key exists in `args`.
    bool flag(const char* key) const { return args.has(key); }
};

} // namespace viatext
