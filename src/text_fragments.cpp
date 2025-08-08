
/**
 * @file text_fragments.cpp
 * @brief TextFragments implementation (fragmented string container)
 *
 * Refer to:
 *   - message.md for design philosophy and blueprint
 *   - text_fragments.hpp for full code-level documentation
 *
 * This implementation is designed for:
 *   - microcontroller environments (no dynamic memory)
 *   - ETL (Embedded Template Library) string containers
 *   - safe, linear parsing of long strings in constrained systems
 */
#include "viatext/text_fragments.hpp"

namespace viatext {

// Default constructor: create empty fragment container
// -----------------------------------------------------
// Initializes an empty TextFragments instance:
// - used_fragments: 0 (no data yet)
// - error: 2 (convention for 'empty')
// - iter_idx: 0 (no iteration started)
template <size_t MaxFragments, size_t FragSize>
TextFragments<MaxFragments, FragSize>::TextFragments()
    : used_fragments(0), error(2), iter_idx(0) {}


// Constructor: immediately splits input string into fragments
// -----------------------------------------------------------
// Calls `set(src)` during construction, which:
// - Splits the source string into fixed-size chunks
// - Stores each chunk in `fragments[]`
// - Updates `used_fragments`, and sets `error` if input overflows
//
// This is a convenient way to create a pre-filled container in one step.
// Note: Does not use dynamic memory — safe for MCUs.
template <size_t MaxFragments, size_t FragSize>
TextFragments<MaxFragments, FragSize>::TextFragments(const char* src)
    : used_fragments(0), error(0), iter_idx(0)
{
    set(src);
}


// set(): Splits a null-terminated string into fixed-size fragments
// ----------------------------------------------------------------
// This method takes a full string (e.g., a full message payload) and
// breaks it into smaller, fixed-size chunks stored in the `fragments[]` array.
// Each chunk can hold up to FragSize characters. A maximum of MaxFragments
// chunks are stored.
//
// Flags:
// - error = 0: success
// - error = 1: input too long to fit into available fragment slots
// - error = 2: empty input
template <size_t MaxFragments, size_t FragSize>
void TextFragments<MaxFragments, FragSize>::set(const char* src) {
    // Step 1: Reset internal state
    used_fragments = 0;  // Clear any previously stored data
    error = 0;           // Assume success unless proven otherwise
    iter_idx = 0;        // Reset iterator for .next() usage

    // Step 2: Calculate input string length
    // -------------------------------------
    // We don't assume strlen() is available or safe on all MCUs.
    size_t len = 0;
    while (src[len] != '\0') ++len;

    // Step 3: Handle edge case: empty input string
    if (len == 0) {
        error = 2; // Error code 2 = empty input
        return;
    }

    // Step 4: Start fragmenting
    // -------------------------
    // Each loop iteration fills one fragment up to FragSize characters.
    size_t written = 0; // Tracks how much of the input has been processed

    for (size_t i = 0; i < MaxFragments && written < len; ++i) {
        fragments[i].clear();  // Reset the current fragment string

        // Step 4a: Determine how many characters to copy into this fragment
        size_t frag_len = (len - written > FragSize) ? FragSize : (len - written);

        // Step 4b: Copy characters from the input string into the fragment
        for (size_t j = 0; j < frag_len; ++j)
            fragments[i].push_back(src[written + j]);

        written += frag_len;   // Update how many characters we've processed
        used_fragments++;      // Track that this fragment is now in use
    }

    // Step 5: Detect overflow condition
    // ---------------------------------
    // If we ran out of fragments before all characters were copied,
    // then the input was too long and we signal an error.
    if (written < len)
        error = 1; // Error code 1 = overflow: not all input could be stored
}


// reset(): Sets iterator back to start
// This resets the internal fragment iterator used by `next()`,
// allowing a new loop through the stored fragments from the beginning.
template <size_t MaxFragments, size_t FragSize>
void TextFragments<MaxFragments, FragSize>::reset() {
    iter_idx = 0; // Start iteration from the first fragment (index 0)
}

// next(): Returns pointer to next fragment, or nullptr if all fragments consumed
// This provides access to fragments one by one in order. After calling `reset()`,
// repeated calls to `next()` will return each fragment until none remain.
template <size_t MaxFragments, size_t FragSize>
const etl::string<FragSize>* TextFragments<MaxFragments, FragSize>::next() {
    if (iter_idx < used_fragments) {
        // There is another unused fragment; return pointer and increment index
        return &fragments[iter_idx++];
    }
    // All fragments have been returned; signal end of iteration
    return nullptr;
}


// get_next_character(): Returns the next character in all fragments, one-by-one.
//
// This method provides linear access to each character stored across all fragments,
// as if the entire set of fragments were one contiguous string. It skips from fragment
// to fragment automatically and safely.
//
// When the end of the data is reached, it returns '\0' and sets the
// `character_iteration_complete` flag to prevent further calls unless reset.
//
// Usage: Call repeatedly until it returns 0. Use `reset_character_iterator()` to start over.

template <size_t MaxFragments, size_t FragSize>
char TextFragments<MaxFragments, FragSize>::get_next_character() {
    // Exit early if iteration is already complete or if no fragments exist
    if (character_iteration_complete || used_fragments == 0) {
        return 0; // Null char indicates end of input
    }

    // If we're past the end of the current fragment, advance to next one
    if (get_next_character_index >= fragments[get_next_character_fragment_index].size()) {
        get_next_character_fragment_index++;  // Move to next fragment
        get_next_character_index = 0;         // Reset index within new fragment

        // If we’ve gone past the last used fragment, mark iteration as complete
        if (get_next_character_fragment_index >= used_fragments) {
            get_next_character_index = 0;
            get_next_character_fragment_index = 0;
            character_iteration_complete = true;
            return 0; // Signal end of iteration
        }
    }

    // Return next character from current position, then advance index
    char c = fragments[get_next_character_fragment_index][get_next_character_index];
    get_next_character_index++;
    return c;
}


// reset_character_iterator(): Prepares the character-level iterator to start over from the beginning.
template <size_t MaxFragments, size_t FragSize>
void TextFragments<MaxFragments, FragSize>::reset_character_iterator() {
    get_next_character_index = 0;             // Start at the first character of the first fragment
    get_next_character_fragment_index = 0;    // Start at the first fragment
    character_iteration_complete = false;     // Allow iteration to proceed
}

// count(): Returns how many fragments are currently in use.
// This is typically set by the set() function, and reflects how much of the buffer is meaningful.
template <size_t MaxFragments, size_t FragSize>
uint8_t TextFragments<MaxFragments, FragSize>::count() const {
    return used_fragments;
}

// operator[]: Provides safe access to a specific fragment by index.
// Caller must ensure index is within bounds [0, used_fragments).
template <size_t MaxFragments, size_t FragSize>
const etl::string<FragSize>& TextFragments<MaxFragments, FragSize>::operator[](size_t idx) const {
    return fragments[idx];
}


// Explicit template instantiation for common config
// This forces the compiler to generate code for the standard case of 8 fragments × 32 bytes each.
// Useful in embedded environments to avoid template bloat.
template struct TextFragments<8, 32>;

// clear(): Erases all stored fragments and resets internal state.
// Used to reset the structure before re-use.
template <size_t MaxFragments, size_t FragSize>
void TextFragments<MaxFragments, FragSize>::clear() {
    // Clear all possible fragments, even unused ones
    for (size_t i = 0; i < MaxFragments; ++i) {
        fragments[i].clear(); // ETL strings, zero length after clear()
    }

    used_fragments = 0; // Nothing stored
    error = 2;          // Consistent with "empty" state as in default constructor
    iter_idx = 0;       // Reset fragment-level iterator
}



} // namespace viatext
