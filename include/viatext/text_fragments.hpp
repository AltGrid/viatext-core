/**
 * @file text_fragments.hpp
 * @brief TextFragments — Fixed-size input splitter for microcontroller-safe tokenized messaging.
 *
 * ---
 *
 * ## Purpose and Role in ViaText
 *
 * The `TextFragments<MaxFragments, FragSize>` class is the **foundation of all input parsing**
 * in the ViaText system. It acts as a lightweight adapter that breaks an incoming null-terminated
 * string (`const char*`) into a fixed number of equal-size string fragments. These fragments are
 * stored in statically allocated ETL containers, making them safe for **embedded systems** and
 * consistent across **Linux and microcontroller** targets.
 *
 * This class enables **both human-to-human messaging and machine-to-machine instruction** using
 * the same compact structure — a 256-byte (8×32) chunk of space shared between:
 *
 * - User message text
 * - Routing headers (`~delimited`)
 * - Shell-style key/value arguments (e.g., `-rssi 92`, `-data hello`)
 *
 * ---
 *
 * ## Why Not `std::string`?
 *
 * While C++'s standard library containers offer flexibility, they are **unsuitable** for
 * memory-constrained environments like Arduino, ESP32, or ATmega chips.
 *
 * Using `std::string` in embedded contexts introduces:
 * - **Heap allocations** — Unsafe and unpredictable on low-RAM MCUs
 * - **Exception handling** — Unsupported or disabled in most embedded toolchains
 * - **Increased binary size** — Bloated binaries with STL linkage
 * - **Non-deterministic memory behavior** — Hard to reason about RAM usage
 * - **Platform inconsistency** — Code may run fine on Linux but crash on MCU
 *
 * Instead, `TextFragments` uses [ETL (Embedded Template Library)](https://www.etlcpp.com/) containers:
 * - `etl::string<N>` for fixed-capacity strings
 * - `etl::vector<T,N>` and `etl::map<K,V,N>` for safe, bounded collections
 *
 * **ETL GitHub**: https://github.com/ETLCPP/etl  
 * **ETL Homepage**: https://www.etlcpp.com/
 *
 * ---
 *
 * ## Memory Footprint (Default Configuration)
 *
 * Default instantiation:
 * @code
 * TextFragments<8, 32>
 * @endcode
 *
 * - Uses 8 ETL strings, each with capacity for 32 characters
 * - Total static buffer size = **256 bytes**
 * - No heap usage, no dynamic allocation
 *
 * This size is intentionally aligned with common LoRa/serial limits and allows
 * complete messages (headers + routing + payload) to be processed without overflow.
 *
 * ---
 *
 * ## Core Capabilities
 *
 * - Splits any `const char*` input into up to `MaxFragments` fragments
 * - Offers safe indexed and linear access via:
 *   - `operator[](idx)` → indexed access
 *   - `next()` → fragment iteration
 *   - `get_next_character()` → linear char stream (across all fragments)
 * - Provides error state: overflow (`error = 1`), empty (`error = 2`)
 * - Can be reused and reset multiple times (safe for loops, services, or daemons)
 * - Compatible with all Core components (e.g., `ArgParser`, CLI, LoRa nodes)
 *
 * ---
 *
 * ## Why This Matters
 *
 * This class is what allows ViaText to unify input handling across:
 * - LoRa radio packets
 * - Serial-over-USB messages
 * - Linux shell input
 * - Internal CLI and daemon systems
 *
 * It ensures that whether a message is typed, transmitted, or programmatically
 * assembled, it gets processed by the same logic:
 *
 * - Split → Tokenized → Parsed → Routed → Logged
 *
 * If this system were replaced with `std::string`, you would immediately lose:
 * - Cross-platform compatibility
 * - Determinism in memory layout
 * - Microcontroller support
 * - Safety in deeply embedded runtimes
 *
 * ---
 *
 * ## Usage Example
 *
 * ```cpp
 * TextFragments<8, 32> fragments("-m -rssi 92 -data hello~world");
 * ArgParser args(fragments);
 * if (args.has_flag("-m")) {
 *     auto msg = args.get_argument("-data");
 * }
 * ```
 *
 * ---
 *
 * ## Error Flags
 *
 * | Code | Meaning                 |
 * |------|-------------------------|
 * | 0    | OK                      |
 * | 1    | Overflow (input too big)|
 * | 2    | Empty or not initialized|
 *
 * ---
 *
 * ## Related Components
 * - `ArgParser` — Uses this class as its tokenizer
 * - `Core` — Relies on arguments and payloads split here
 * - `Message` — Pulls fields out of `-data` payloads fragmented here
 *
 * ---
 *
 * ## Authors
 * @author Leo
 * @author ChatGPT
 * @date 2025-08-08
 */
#ifndef VIATEXT_TEXT_FRAGMENTS_HPP
#define VIATEXT_TEXT_FRAGMENTS_HPP

#include "etl/string.h"
#include <stdint.h>
#include <stddef.h>

namespace viatext {

/**
 * @class TextFragments
 * @brief Splits and stores a long string in up to N fixed-size fragments for transmission or storage.
 * 
 * Typical use: for LoRa, mesh radio, or serial protocols on memory-constrained systems.
 * - Uses ETL string for portability and reliability.
 * - Handles fragmentation internally; user never touches fragments directly.
 * 
 * @tparam MaxFragments Number of fragments to allocate (default: 8)
 * @tparam FragSize Maximum size of each fragment (default: 32)
 */
template <size_t MaxFragments = 8, size_t FragSize = 32>
struct TextFragments {
    /** 
     * @brief Array of statically allocated fragments for holding split input text.
     * Each fragment is a fixed-capacity ETL string of size `FragSize`.
     */
    etl::string<FragSize> fragments[MaxFragments];

    /** 
     * @brief Number of fragments currently in use.
     * Always ≤ MaxFragments; updated during `set()`.
     */
    uint8_t used_fragments = 0;

    /** 
     * @brief Error state indicator.
     * - 0 = OK  
     * - 1 = Overflow (input exceeded total fragment capacity)  
     * - 2 = Empty input or cleared state
     */
    uint8_t error = 0;

    /** 
     * @brief Iterator index used for `next()` and `reset()` fragment iteration.
     */
    uint8_t iter_idx = 0;

    /** 
     * @brief Current character offset within the active fragment.
     * Used by `get_next_character()` for linear character streaming.
     */
    size_t get_next_character_index = 0;

    /** 
     * @brief Current fragment index for character-level streaming.
     * Combined with `get_next_character_index` to traverse across fragments.
     */
    uint8_t get_next_character_fragment_index = 0;

    /** 
     * @brief Flag indicating whether character iteration is finished.
     * Set true after the full character stream has been consumed.
     */
    bool character_iteration_complete = false;



    /**
     * @brief Default constructor. Creates empty (error=2).
     */
    TextFragments();

    /**
     * @brief Construct from a C-string; automatically splits into fragments.
     * @param src Null-terminated source string.
     */
    TextFragments(const char* src);

    /**
     * @brief Set fragments from a C-string. Resets state, splits text into fragments.
     * @param src Null-terminated string to fragment.
     */
    void set(const char* src);

    /**
     * @brief Reset iteration state for next().
     */
    void reset();

    /**
     * @brief Get pointer to next fragment, or nullptr if done.
     * @return Pointer to fragment string, or nullptr.
     */
    const etl::string<FragSize>* next();
    

    /**
     * @brief Number of fragments currently used.
     * @return Count of non-empty fragments.
     */
    uint8_t count() const;

    /**
     * @brief Safe access to fragment at index.
     * @param idx Index of fragment.
     * @return Const reference to fragment string.
     */
    const etl::string<FragSize>& operator[](size_t idx) const;

    /**
     * @brief Clear all fragments and reset state.
     * @details
     *   - Empties every fragment string.
     *   - Resets used_fragments, error code, and iterator index.
     *   - Puts the object in a “fresh, empty” state for reuse.
     */
    void clear();

    /**
     * @brief Returns the next character in the fragment sequence.
     * @return Next character, or 0 if complete (character_iteration_complete becomes true).
     */
    char get_next_character();

    /**
     * @brief Reset character iteration state.
     */
    void reset_character_iterator();


};

} // namespace viatext

#endif // VIATEXT_TEXT_FRAGMENTS_HPP
