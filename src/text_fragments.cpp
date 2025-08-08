#include "viatext/text_fragments.hpp"

namespace viatext {

// Default constructor: create empty, error=2 (empty)
template <size_t MaxFragments, size_t FragSize>
TextFragments<MaxFragments, FragSize>::TextFragments()
    : used_fragments(0), error(2), iter_idx(0) {}

// Constructor: split input text into fragments immediately
template <size_t MaxFragments, size_t FragSize>
TextFragments<MaxFragments, FragSize>::TextFragments(const char* src)
    : used_fragments(0), error(0), iter_idx(0)
{
    set(src);
}

// set(): Splits a null-terminated string into fixed-size fragments
template <size_t MaxFragments, size_t FragSize>
void TextFragments<MaxFragments, FragSize>::set(const char* src) {
    used_fragments = 0;
    error = 0;
    iter_idx = 0;

    // Count input length
    size_t len = 0;
    while (src[len] != '\0') ++len;

    // Handle empty input
    if (len == 0) { error = 2; return; }

    size_t written = 0;
    // For each fragment, fill with up to FragSize chars
    for (size_t i = 0; i < MaxFragments && written < len; ++i) {
        fragments[i].clear();
        size_t frag_len = (len - written > FragSize) ? FragSize : (len - written);
        // Copy chars into fragment
        for (size_t j = 0; j < frag_len; ++j)
            fragments[i].push_back(src[written + j]);
        written += frag_len;
        used_fragments++;
    }
    // If input didn't fit, set error flag
    if (written < len) error = 1; // overflow: too much input for available fragments
}

// reset(): Sets iterator back to start
template <size_t MaxFragments, size_t FragSize>
void TextFragments<MaxFragments, FragSize>::reset() { iter_idx = 0; }

// next(): Returns pointer to next fragment, or nullptr if all fragments consumed
template <size_t MaxFragments, size_t FragSize>
const etl::string<FragSize>* TextFragments<MaxFragments, FragSize>::next() {
    if (iter_idx < used_fragments)
        return &fragments[iter_idx++];
    return nullptr;
}


template <size_t MaxFragments, size_t FragSize>
char TextFragments<MaxFragments, FragSize>::get_next_character() {
    if (character_iteration_complete || used_fragments == 0) {
        return 0;
    }

    // Bounds check: are we past the end of current fragment?
    if (get_next_character_index >= fragments[get_next_character_fragment_index].size()) {
        // Try next fragment
        get_next_character_fragment_index++;
        get_next_character_index = 0;

        // If past last used fragment, we're done
        if (get_next_character_fragment_index >= used_fragments) {
            get_next_character_index = 0;
            get_next_character_fragment_index = 0;
            character_iteration_complete = true;
            return 0;
        }
    }

    // Get character
    char c = fragments[get_next_character_fragment_index][get_next_character_index];
    get_next_character_index++;
    return c;
}

template <size_t MaxFragments, size_t FragSize>
void TextFragments<MaxFragments, FragSize>::reset_character_iterator() {
    get_next_character_index = 0;
    get_next_character_fragment_index = 0;
    character_iteration_complete = false;
}


// count(): Returns how many fragments are in use
template <size_t MaxFragments, size_t FragSize>
uint8_t TextFragments<MaxFragments, FragSize>::count() const { return used_fragments; }

// operator[]: Safe access to any fragment by index
template <size_t MaxFragments, size_t FragSize>
const etl::string<FragSize>& TextFragments<MaxFragments, FragSize>::operator[](size_t idx) const {
    return fragments[idx];
}

// Explicit template instantiation for common config
template struct TextFragments<8, 32>;

template <size_t MaxFragments, size_t FragSize>
void TextFragments<MaxFragments, FragSize>::clear() {
    for (size_t i = 0; i < MaxFragments; ++i) {
        fragments[i].clear();
    }
    used_fragments = 0;
    error = 2; // Consistent with empty/error state
    iter_idx = 0;
}


} // namespace viatext
