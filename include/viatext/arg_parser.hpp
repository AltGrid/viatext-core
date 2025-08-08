/**
 * @file arg_parser.hpp
 * @brief ViaText ArgParser — MCU-safe argument tokenizer for flags and key-value pairs.
 *
 * This class provides a lightweight, deterministic parser for interpreting command-line-style
 * arguments in the ViaText Core. It is fully compatible with **microcontrollers** (ESP32, ATmega),
 * **Linux** terminals, and **LoRa payload strings**, enabling all node types to share a unified
 * input format and logic.
 *
 * ---
 *
 * @section viatext_argparser_intro Overview
 *
 * The ArgParser reads a `TextFragments<>` input stream and splits it into:
 *
 * - **A directive** (the first token, often `-m`, `-p`, etc.)
 * - **Standalone flags** (keys like `-m` with no associated value)
 * - **Key-value pairs** (e.g., `-rssi 92`, `-sf 7`)
 * - **Tail arguments** (like `-data`, which consume the rest of the line as one value)
 *
 * The design emphasizes:
 * - **No dynamic memory** (all containers are statically allocated using ETL)
 * - **No STL dependencies**
 * - **Deterministic behavior** with tight memory bounds
 * - **Ease of use and clarity** for both humans and AI-based processing
 *
 * ---
 *
 * @section viatext_argparser_usage Example
 *
 * ```cpp
 * TextFragments<8, 32> input(" -m -rssi 92 -sf 7 -data 0x01~bob~alice~Hello ");
 * ArgParser args(input);
 *
 * if (args.has_flag("-m")) {
 *     auto snr = args.get_argument("-rssi");
 *     auto payload = args.get_argument("-data");
 * }
 * ```
 *
 * ---
 *
 * @section viatext_argparser_keyrules Format Rules
 *
 * - Arguments must follow this shell-style syntax:
 *   - `-key [value]`
 *   - A dash-prefixed key is always required. Double dashes "--" are not supported.
 *   - Value is optional (if missing, treated as flag)
 * - Keys and values must be separated by a space
 * - Multiple spaces are tolerated
 * - Tail-style keys like `-data` consume the remainder of the input
 * - Quoting is **not supported** (but values like `-data` may contain `~`)
 *
 * ---
 *
 * @section viatext_argparser_design Design Constraints
 *
 * To remain compatible with **low-RAM microcontrollers**, this parser:
 * - Uses fixed ETL containers:
 *   - `etl::map<token_t, token_t, MAX_ARGS>` for key-value arguments
 *   - `etl::vector<token_t, MAX_FLAGS>` for flags
 * - Accepts up to **16 tokens**, 12 key-value pairs, and 8 standalone flags
 * - Supports tail keys such as `-data` to preserve message payload integrity
 * - Avoids recursion, heap allocation, exceptions, or variable-size containers
 *
 * ---
 *
 * @section viatext_argparser_tailkeys Special Keys
 *
 * Tail keys are special argument types (like `-data`) that absorb all
 * remaining tokens into a single string value.
 *
 * This allows complex payloads like:
 *
 * ```bash
 * -data 0x01~alice~bob~ViaText is working fine
 * ```
 *
 * ...to be treated as a single `-data` argument value,
 * rather than being split and misinterpreted.
 *
 * ---
 *
 * @section viatext_argparser_role Role in ViaText
 *
 * The ArgParser is invoked by **all Core wrappers** — whether LoRa, CLI, or Linux daemon —
 * immediately after receiving raw input from serial, terminal, or socket.
 *
 * It is used to:
 * - Detect message operations (e.g., `-m`, `-ping`, `-set_id`)
 * - Extract metadata (e.g., `-rssi`, `-snr`, `-cr`)
 * - Preserve payloads in `-data` format
 *
 * Output is consumed by:
 * - The `Core` system for routing and decision-making
 * - The `Message` object, which parses the payload body using `~` delimiters
 *
 * ---
 *
 * @section viatext_argparser_limitations Limitations
 *
 * This parser is intentionally limited to preserve portability:
 * - No quoting or escaping
 * - No support for grouped flags (`-abc` is invalid; use `-a -b -c`)
 * - No long-form keys (`--key` is not supported; use `-key`)
 *
 * ---
 *
 * @section viatext_argparser_author Authors
 * @author Leo
 * @author ChatGPT
 * @date 2025-08-08
 */


#ifndef VIATEXT_ARG_PARSER_HPP
#define VIATEXT_ARG_PARSER_HPP

#include "text_fragments.hpp"
#include "etl/string.h"
#include "etl/vector.h"
#include "etl/map.h"
#include <stdint.h>

namespace viatext {
/**
 * @class ArgParser
 * @brief Lightweight shell-style argument parser for the ViaText protocol core.
 *
 * The `ArgParser` class tokenizes a `TextFragments<>` input stream into:
 * - A **directive** (the first token)
 * - A list of **flags** (standalone keys like `-m`)
 * - A map of **key-value arguments** (e.g., `-rssi 92`)
 *
 * It is designed to be fully compatible with **microcontrollers**, **embedded systems**, 
 * and **Linux**, with:
 * - No STL dependencies
 * - No heap allocation
 * - Fixed ETL containers for predictable memory usage
 *
 * ---
 *
 * ### Design Goals
 * - **Cross-platform** compatibility: Linux, ESP32, AVR, etc.
 * - **Human-readable format**: Inspired by traditional shell input
 * - **Deterministic parsing**: No surprises across platforms
 * - **Zero dynamic memory**: Safe for embedded use
 *
 * ---
 *
 * ### Example
 * @code
 * TextFragments<8, 32> input("-m -rssi 92 -data hello~world");
 * ArgParser args(input);
 *
 * if (args.has_flag("-m")) {
 *     auto rssi = args.get_argument("-rssi");
 *     auto data = args.get_argument("-data");
 * }
 * @endcode
 *
 * ---
 *
 * ### Supported Syntax
 * - `-key [value]` format
 * - Whitespace-separated tokens
 * - Keys must begin with a dash (`-`)
 * - Values are optional; missing values are treated as **flags**
 * - Special **tail keys** like `-data` absorb the remaining tokens into a single value
 *
 * ---
 *
 * ### Typical Use in ViaText
 * - Used in Core to interpret all incoming messages
 * - Common flags: `-m`, `-ping`, `-ack`, `-set_id`
 * - Common args: `-rssi`, `-snr`, `-data`, `-from`, `-to`
 *
 * ---
 *
 * @see TextFragments
 * @see Message
 * @see viatext::Core
 */
class ArgParser {
public:
    /**
     * @brief Maximum size (in characters) for any parsed token (key or value).
     * Applies to flags, argument keys, and values.
     */
    static constexpr size_t TOKEN_SIZE  = 32;

    /**
     * @brief Maximum number of standalone flags accepted per input.
     * Flags are keys with no values (e.g., `-m`).
     */
    static constexpr size_t MAX_FLAGS   = 8;

    /**
     * @brief Maximum number of key-value pairs allowed.
     * Each pair consists of a `-key` and its associated value.
     */
    static constexpr size_t MAX_ARGS    = 12;

    /**
     * @brief Maximum number of space-separated tokens parsed from input.
     * Includes flags, keys, values, and tail content.
     */
    static constexpr size_t MAX_TOKENS  = 16;

    /**
     * @brief Token type used for all keys and values.
     * Fixed-size ETL string for heap-free safety.
     */
    using token_t    = etl::string<TOKEN_SIZE>;

    /**
     * @brief Container for standalone flags.
     * Stores up to MAX_FLAGS entries like `-m`, `-debug`, etc.
     */
    using flag_list_t = etl::vector<token_t, MAX_FLAGS>;

    /**
     * @brief Container for parsed key-value arguments.
     * Each key maps to a single token value (e.g., `-rssi → 92`).
     */
    using arg_map_t   = etl::map<token_t, token_t, MAX_ARGS>;


    /**
     * @brief Construct and parse from a TextFragments stream.
     * @param fragments Fragmented input; iterator is reset.
     */
    ArgParser(TextFragments<>& fragments) {
        parse(fragments);
    }

    /**
     * @brief Get the full list of parsed standalone flags.
     *
     * Flags are keys that appear without a value (e.g., `-m`, `-verbose`).
     * This function returns a const reference to the internal ETL vector
     * holding all such flags found during parsing.
     *
     * @return Reference to the vector of parsed flags.
     */
    const flag_list_t& flags() const {
        return m_flags;
    }

    /**
     * @brief Get the full map of parsed key-value arguments.
     *
     * This returns a const reference to the internal ETL map, where each key
     * (e.g., `-rssi`) is mapped to its corresponding value (e.g., `"92"`).
     *
     * @return Reference to the argument map.
     */
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
     * @brief Get the main directive token (first token parsed).
     *
     * The directive is the very first token encountered in the input stream,
     * often used as a primary action indicator (e.g., `-m`, `-ping`, `-set_id`).
     *
     * It is not treated as a flag or argument — instead, it guides the core logic
     * to determine the type of message or operation.
     *
     * @return Reference to the directive token (empty if input was empty).
     */
    const token_t& directive() const {
        return m_directive;
    }

private:
    /** @brief First token from input; used as directive (e.g., `-m`, `-ping`). */
    token_t     m_directive;

    /** @brief List of flags parsed from input (keys with no values). */
    flag_list_t m_flags;

    /** @brief Map of key-value argument pairs parsed from input. */
    arg_map_t   m_args;

    /** @brief Shared empty token returned when a key is not found. */
    token_t     m_empty;  ///< Returned for missing lookups


    /**
     * @brief Parse the input fragments into flags, key-value pairs, and directive.
     *
     * This function processes the raw character stream from a `TextFragments<>` object
     * and tokenizes the input into structured components:
     *
     * - The **first token** is stored as the `directive` (e.g., `-m`, `-ping`)
     * - Keys followed by values are stored in the argument map (e.g., `-rssi 92`)
     * - Keys without values are stored as standalone flags (e.g., `-debug`)
     * - Keys designated as **tail keys** (like `-data`) consume the remainder of the input
     *   and are stored as a single value
     *
     * ### Parsing Behavior:
     * - Leading and multiple spaces are skipped
     * - Token boundaries are defined by space or end-of-input
     * - A maximum of `MAX_TOKENS` tokens will be parsed
     * - Tail keys are defined in a static list inside the method (currently just `-data`)
     *
     * ### Example Input:
     * @code
     * -m -rssi 92 -snr 4.5 -data 0x01~alice~bob~Hi Bob
     * @endcode
     *
     * Will result in:
     * - `directive = "-m"`
     * - `flags = [ "-m" ]`
     * - `arguments = { "-rssi" → "92", "-snr" → "4.5", "-data" → "0x01~alice~bob~Hi Bob" }`
     *
     * @note This function is called automatically in the constructor.
     * @param fragments A `TextFragments<>` stream that provides input characters one at a time.
     */
    void parse(TextFragments<>& fragments) {
        // Reset the input iterator to the beginning of the stream.
        fragments.reset_character_iterator();

        // ----------------------------------------------
        // Phase 1: Tokenization (space-separated chunks)
        // ----------------------------------------------
        // We'll collect up to MAX_TOKENS individual tokens into the `tokens` vector.
        etl::vector<token_t, MAX_TOKENS> tokens;
        token_t tok;

        while (true) {
            tok.clear();
            char c;

            // Skip any leading whitespace between tokens.
            do {
                c = fragments.get_next_character();
            } while (c == ' ');

            // End of input? Break the outer loop.
            if (c == 0) break;

            // Read a token one character at a time, until space or null.
            do {
                if (tok.size() < TOKEN_SIZE) {
                    tok.push_back(c); // Add character to token
                }
                c = fragments.get_next_character();
            } while (c != 0 && c != ' ');

            // Only store the token if it's not empty and we have room
            if (!tok.empty() && tokens.size() < MAX_TOKENS) {
                tokens.push_back(tok);
            }

            // End of input again? Break outer loop.
            if (c == 0) break;
        }

        // ---------------------------------------------------
        // Phase 2: Extract Directive (first token, if present)
        // ---------------------------------------------------
        size_t idx = 0;
        if (!tokens.empty()) {
            m_directive = tokens[0]; // First token is the "directive"
            idx = 1; // Begin parsing args and flags from token 1 onward
        }

        // --------------------------------------------------------
        // Phase 3: Tail Keys — special keys that absorb all input
        // --------------------------------------------------------
        // These are defined here as fixed strings. For now, only "-data".
        static const token_t tailKeys[] = { token_t("-data") };

        // --------------------------------------------
        // Phase 4: Parse remaining tokens (args/flags)
        // --------------------------------------------
        while (idx < tokens.size()) {
            token_t& key = tokens[idx];

            // -- Check for a tail key (like -data) --
            bool isTail = false;
            for (auto& tk : tailKeys) {
                if (key == tk) {
                    isTail = true;
                    break;
                }
            }

            // If it's a tail key, absorb all remaining tokens into a single string.
            if (isTail) {
                token_t rest;
                for (size_t j = idx + 1; j < tokens.size(); ++j) {
                    // Respect total token size limit
                    if (rest.size() + tokens[j].size() + 1 < TOKEN_SIZE) {
                        if (j > idx + 1) rest.push_back(' '); // Space separator
                        rest.append(tokens[j]);
                    }
                }
                m_args.insert(arg_map_t::value_type(key, rest));
                break; // Done parsing; tail consumed everything
            }

            // -- Standard key-value pair (e.g., "-rssi 92") --
            if (idx + 1 < tokens.size() && tokens[idx+1][0] != '-') {
                m_args.insert(arg_map_t::value_type(key, tokens[idx+1]));
                idx += 2; // Skip both key and value
            }
            else {
                // -- Standalone flag (e.g., "-m") --
                if (m_flags.size() < MAX_FLAGS) {
                    m_flags.push_back(key);
                }
                idx++; // Move to next token
            }
        }
    }
};

} // namespace viatext

#endif // VIATEXT_ARG_PARSER_HPP
