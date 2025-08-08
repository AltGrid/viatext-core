#ifndef VIATEXT_ARG_PARSER_HPP
#define VIATEXT_ARG_PARSER_HPP

/**
 * @file arg_parser.hpp
 * @brief Argument parser for ViaText: embedded-safe, heap-free, STL-free.
 *
 * Parses a TextFragments<> input stream character-by-character,
 * tokenizes shell-style arguments, recognizes flags, key-value pairs,
 * and "tail" keys that consume the rest of the input.
 *
 * Usage:
 *   TextFragments<> fragments(raw_input);
 *   ArgParser args(fragments);
 *   if (args.has_flag("-m")) { ... }
 *   auto val = args.get_argument("-rssi");
 *
 * Requirements:
 *   - ETL containers: etl::string<32>, etl::vector<...,8>, etl::map<...,12>
 *   - No dynamic memory
 *   - MCU-compatible
 *
 * @author Leo
 * @author ChatGPT
 * @date 2025-08-07
 */

#include "text_fragments.hpp"
#include "etl/string.h"
#include "etl/vector.h"
#include "etl/map.h"
#include <stdint.h>

namespace viatext {

class ArgParser {
public:
    static constexpr size_t TOKEN_SIZE  = 32;
    static constexpr size_t MAX_FLAGS   = 8;
    static constexpr size_t MAX_ARGS    = 12;
    static constexpr size_t MAX_TOKENS  = 16;

    using token_t    = etl::string<TOKEN_SIZE>;
    using flag_list_t = etl::vector<token_t, MAX_FLAGS>;
    using arg_map_t   = etl::map<token_t, token_t, MAX_ARGS>;

    /**
     * @brief Construct and parse from a TextFragments stream.
     * @param fragments Fragmented input; iterator is reset.
     */
    ArgParser(TextFragments<>& fragments) {
        parse(fragments);
    }

    // Return the list of standalone flags
    const flag_list_t& flags() const {
        return m_flags;
    }

    // Return the map of all key→value arguments
    const arg_map_t& arguments() const {
        return m_args;
    }

    /**
     * @brief Check if a standalone flag was provided.
     * @param flag Key string (e.g. "-m").
     * @return true if present.
     */
    bool has_flag(const token_t& flag) const {
        for (size_t i = 0; i < m_flags.size(); ++i) {
            if (m_flags[i] == flag) {
                return true;
            }
        }
        return false;
    }

    /**
     * @brief Check if a key-value argument exists.
     * @param key Key string (e.g. "-rssi").
     * @return true if present.
     */
    bool has_argument(const token_t& key) const {
        return m_args.find(key) != m_args.end();
    }

    /**
     * @brief Retrieve a value for a key.
     * @param key Key string.
     * @return Reference to value string (empty if not found).
     */
    const token_t& get_argument(const token_t& key) const {
        auto it = m_args.find(key);
        return (it != m_args.end()) ? it->second : m_empty;
    }

    /**
     * @brief Get the main directive (first token).
     */
    const token_t& directive() const {
        return m_directive;
    }

private:
    token_t     m_directive;
    flag_list_t m_flags;
    arg_map_t   m_args;
    token_t     m_empty;  ///< Returned for missing lookups

    /**
     * @brief Parse the input fragments into flags and args.
     */
    void parse(TextFragments<>& fragments) {
        fragments.reset_character_iterator();

        // Gather tokens
        etl::vector<token_t, MAX_TOKENS> tokens;
        token_t tok;
        while (true) {
            tok.clear();
            char c;
            // Skip spaces
            do {
                c = fragments.get_next_character();
            } while (c == ' ');
            if (c == 0) break;
            // Read token
            do {
                if (tok.size() < TOKEN_SIZE) {
                    tok.push_back(c);
                }
                c = fragments.get_next_character();
            } while (c != 0 && c != ' ');

            if (!tok.empty() && tokens.size() < MAX_TOKENS) {
                tokens.push_back(tok);
            }
            if (c == 0) break;
        }

        // Process tokens: first is directive
        size_t idx = 0;
        if (!tokens.empty()) {
            m_directive = tokens[0];
            idx = 1;
        }

        // List of tail keys that consume rest of input
        static const token_t tailKeys[] = { token_t("-data") };

        // Parse remaining tokens
        while (idx < tokens.size()) {
            token_t& key = tokens[idx];
            // Check for tail key
            bool isTail = false;
            for (auto& tk : tailKeys) {
                if (key == tk) {
                    isTail = true;
                    break;
                }
            }
            if (isTail) {
                // Join rest tokens into one value
                token_t rest;
                for (size_t j = idx + 1; j < tokens.size(); ++j) {
                    if (rest.size() + tokens[j].size() + 1 < TOKEN_SIZE) {
                        if (j > idx + 1) rest.push_back(' ');
                        rest.append(tokens[j]);
                    }
                }
                m_args.insert(arg_map_t::value_type(key, rest));
                break;
            }
            // Try key-value pair
            if (idx + 1 < tokens.size() && tokens[idx+1][0] != '-') {
                m_args.insert(arg_map_t::value_type(key, tokens[idx+1]));
                idx += 2;
            }
            else {
                // Standalone flag
                if (m_flags.size() < MAX_FLAGS) {
                    m_flags.push_back(key);
                }
                idx++;
            }
        }
    }
};

} // namespace viatext

#endif // VIATEXT_ARG_PARSER_HPP
