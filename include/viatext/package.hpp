/**
 * @file package.hpp
 * @brief ViaText Package & Argument Manager — minimal, heap-free metadata container.
 *
 * This header defines two tiny types used across ViaText Core:
 *
 *  - `ArgList` : a small, fixed-capacity list of **arguments** as key→value pairs.
 *                Keys are preserved EXACTLY as provided (e.g., "-m", "-rssi", "--mode").
 *                Only leading/trailing whitespace is trimmed from keys and values.
 *                Values may be empty to represent **presence-only flags**.
 *
 *  - `Package` : a logical message + its metadata. The `payload` holds the expanded
 *                in-system text (up to 255 bytes), while `args` stores all metadata
 *                and control flags as an `ArgList`.
 *
 * ## Why this exists
 * ViaText Core accepts messages from different sources (LoRa, Linux CLI, Serial, tests),
 * yet the core logic should be **ingress-agnostic**. Instead of passing around many
 * source-specific structs, we normalize everything to:
 *
 *  - a small **payload** string (`etl::string<255>`) for the message content, and
 *  - a tiny **argument manager** for metadata and control flags.
 *
 * This keeps the core **simple, portable, and deterministic**:
 * - No STL, no dynamic allocation (Arduino-safe).
 * - ETL strings/vectors with fixed capacities.
 * - Keys are not renamed or canonicalized; they’re stored exactly as given.
 *
 * ## What this is NOT
 * - It does **not** parse command lines or radio frames by itself.
 *   Your wrappers (LoRa bridge, CLI, Serial) should parse inputs and then populate
 *   a `Package` via `args.set(...)` / `args.set_flag(...)` and `payload`.
 * - It does **not** carry the on-air 5-byte `MessageID`. The radio frame already
 *   embeds that inside the ≤80B packet. Wrappers may still *expose* selected
 *   header fields as arguments (e.g., "-seq", "-part") **if** the core needs them.
 *
 * ## Typical flow (write → tick → read)
 * Your wrappers:
 *   1) build a `Package` with `payload` (0..255B in-system text) and `args`
 *   2) submit it to the core: `core.add_message(pkg);`
 *   3) call `core.tick(ms)` periodically
 *   4) pull outbound work with `core.get_message(next)`
 *
 * The core’s internal rules operate only on:
 *   - `pkg.payload` (text in / text out), and
 *   - `pkg.args` (presence/values of arguments).
 *
 * ## Design choices (important)
 * - **Keys preserved**: "-rssi" stays "-rssi", "--data-length" stays "--data-length".
 *   This makes it trivial to mirror Linux-like flags and radio-bridge fields without
 *   any surprise renaming.
 * - **Whitespace trimming only**: leading/trailing spaces and tabs are removed from
 *   both keys and values; internal spacing is preserved.
 * - **Flags = empty value**: a presence-only option like "-m" is stored with `v == ""`.
 *   Check with `args.has("-m")`.
 * - **Linear lookups**: tiny O(n) scans over a fixed, small `VT_ARGS_MAX`. This avoids
 *   maps/allocations and is fast enough for microcontrollers at small n.
 *
 * ## Example — LoRa ingress (after reassembly)
 * @code
 * viatext::Package p;
 * p.payload = "0x4F2B000131~SHREK~DONKEY~Shut Up";  // expanded 0..255B
 * p.args.set_flag("-m");            // presence-only
 * p.args.set("-rssi", "-92");       // exact keys preserved
 * p.args.set("-snr",  "4.5");
 * p.args.set("-sf",   "7");
 * p.args.set("-bw",   "125");
 * // Optional extras your bridge wants to expose:
 * p.args.set("--freq-mhz", "915.0");
 * core.add_message(p);
 * @endcode
 *
 * ## Example — Linux CLI ingress
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
 * ## Tuning capacities
 * - `VT_KEY_MAX`   : max key length (default 32). Increase if you use very long option names.
 * - `VT_VAL_MAX`   : max value length (default 128). Increase cautiously for MCU RAM budgets.
 * - `VT_ARGS_MAX`  : max number of stored arguments (default 24).
 * - `Text255`      : internal payload capacity (fixed at 255 by design).
 *
 * Keep these small for embedded safety. If you exceed `VT_ARGS_MAX`, `set()` returns false.
 *
 * ## Complexity and constraints
 * - All operations are **amortized O(n)** over `items.size() ≤ VT_ARGS_MAX`.
 * - No exceptions, no heap, ETL-only — portable to Linux and ESP32/Arduino.
 * - Not thread-safe — guard externally if you use from multiple contexts.
 *
 * ## Testing hints
 * - Verify trimming: `"   -rssi\t"` should store as `"-rssi"`.
 * - Verify replacement: calling `set("-rssi","-90")` again updates the existing entry.
 * - Verify flags: `set_flag("-m")` then `has("-m")` → true; `get("-m")` → empty string.
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

// Tunables — keep these tight for MCU safety.
static constexpr size_t VT_KEY_MAX  = 32;   ///< Maximum length for a key (e.g., "--data-length")
static constexpr size_t VT_VAL_MAX  = 128;  ///< Maximum length for a value
static constexpr size_t VT_ARGS_MAX = 24;   ///< Maximum number of key/value entries

/// Internal text payload, expanded 0..255 bytes in-system (post-reassembly).
using Text255 = etl::string<255>;

/// Argument key and value string types.
using KeyStr = etl::string<VT_KEY_MAX>;
using ValStr = etl::string<VT_VAL_MAX>;

/// @brief A single argument entry (key→value). Empty value means presence-only flag.
/// @note Keys are preserved EXACTLY as supplied (e.g., "-m", "-rssi"), with only
///       leading/trailing whitespace removed.
struct ArgKV {
    KeyStr k;  ///< Key string, preserved as provided (after whitespace trim)
    ValStr v;  ///< Value string; empty => presence-only flag
};

/// @brief Minimal, heap-free argument list with fixed capacity.
/// Keys are stored exactly as provided (no canonicalization). Only whitespace is trimmed.
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
