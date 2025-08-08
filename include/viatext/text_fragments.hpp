#ifndef VIATEXT_TEXT_FRAGMENTS_HPP
#define VIATEXT_TEXT_FRAGMENTS_HPP

/**
 * @file text_fragments.hpp
 * @brief Fixed-size text fragmentation container for ViaText (Arduino/MCU-safe).
 *
 * The TextFragments struct splits a long string into N fixed-size fragments
 * for transmission or storage on low-memory devices. Designed for use with
 * ETL's string class, and compatible with Arduino/AVR/ESP32/Linux.
 *
 * Usage:
 *   - Pass a text string to the constructor (or set() method).
 *   - The text is split into fixed-size fragments (default: 8 × 32 bytes = 256 bytes).
 *   - Iterate through fragments using next(), or access by index.
 *   - An error flag indicates if the string was too long to fit.
 *   - No dynamic memory, no STL dependencies.
 *
 * @author Leo
 * @author ChatGPT
 * @date 2025-08-07
 */

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
    etl::string<FragSize> fragments[MaxFragments]; ///< Array of text fragments
    uint8_t used_fragments = 0; ///< Number of fragments actually used
    uint8_t error = 0;          ///< Error code: 0=OK, 1=overflow, 2=empty
    uint8_t iter_idx = 0;       ///< For next()/reset() iteration
    size_t get_next_character_index = 0;
    uint8_t get_next_character_fragment_index = 0;
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
