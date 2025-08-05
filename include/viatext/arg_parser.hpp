/**
 * @file arg_parser.hpp
 * @brief ViaText Argument and Command String Parser
 * @details
 *    The **ArgParser** class provides a simple, portable way to break down
 *    command-line-style argument strings into named key/value pairs and flags.
 *
 *    **Purpose:**
 *    --------------------------
 *    In the ViaText system, messages can be sent in a variety of formats—
 *    sometimes as raw text, but often using structured argument strings like
 *    those found in Linux commands, radio firmware, or IoT protocols.
 *    The ArgParser is designed to understand these formats and make it easy to
 *    extract useful information, regardless of where the message comes from.
 *
 *    **How It Works:**
 *    --------------------------
 *    - Any incoming string is accepted—ArgParser only engages its parsing logic if
 *      it detects the argument pattern "-<letter> " (a dash followed by a letter and a space).
 *    - It treats tokens that start with a dash (like "-snr") as *argument keys*.
 *    - If a key is immediately followed by a value (like "-snr 4.5"), the value is stored.
 *    - If a key is *not* followed by a value (like "-m"), it is treated as a *flag*
 *      and stored with an empty string.
 *    - Values can be any text, including spaces, numbers, symbols, or special
 *      payloads (such as LoRa packet data).
 *
 *    **Supported Input Examples:**
 *    --------------------------
 *      - "-m -rssi 92 -snr 4.5 -data 0x4F2B000131~shrek~donkey~Shut Up"
 *        → "-m" is a flag, "-rssi" has value "92", "-snr" has value "4.5",
 *           "-data" has a message payload as its value.
 *
 *      - "hello world" (no arguments)
 *        → Nothing is parsed; all keys/values are empty.
 *
 *      - "-sf 7 -bw 125 -cr 4/5"
 *        → Three key/value pairs: "-sf":"7", "-bw":"125", "-cr":"4/5".
 *
 *    **Why This is Useful in ViaText:**
 *    --------------------------
 *    The ArgParser lets ViaText nodes (on Linux, ESP32, or elsewhere)
 *    interpret messages in a standard way, whether the message arrives via
 *    CLI, serial, LoRa, or another medium. It enables easy extraction of
 *    commands, flags, settings, or message content—no matter where or how
 *    the message was created.
 *
 *    **Core Features:**
 *    --------------------------
 *      - Parse any string for arguments and flags
 *      - Quickly check if a key/flag is present
 *      - Retrieve argument values as strings
 *      - Special support for message payloads (e.g. "-data" key)
 *      - Simple and portable—works on both ESP32 and Linux
 *      - Friendly, readable code for learners and maintainers
 *
 *    @author   Leo
 *    @author   ChatGPT
 *    @date     2025-08-05
 */

#pragma once
#include <string>
#include <unordered_map>
#include <vector>

namespace viatext{

    /**
 * @class ArgParser
 * @brief Command/argument string parser for ViaText and related applications.
 *
 * The ArgParser class allows any program or device to **analyze and extract key-value pairs or flags**
 * from a string that may contain arguments, commands, or message content—similar to Linux or radio firmware style.
 *
 * How to Use
 * 
 * 1. **Create an instance** (optionally passing an argument string).
 *    - Example: `ArgParser parser("-m -foo 5 -bar test -data Hello~world");`
 * 2. **Parse any string** with `parse_args(input_string)`.
 *    - You can parse multiple strings by calling `parse_args()` again.
 * 3. **Check for arguments or flags** using `has_arg("-foo")`.
 * 4. **Get the value for any argument** with `get_arg("-bar")`.
 * 5. **Retrieve the main message payload** (if present) using `get_message()` or `parse_message()`.
 *
 * Typical Use Cases
 * 
 * - Parsing LoRa or serial messages with argument-style prefixes.
 * - Extracting radio parameters (like `-rssi 92` or `-sf 7`).
 * - Detecting flags (like `-m` for mode) or simple presence/absence switches.
 * - Quickly breaking out and splitting complex message content using a key (like `-data`).
 *
 * Special Behaviors
 * 
 * - **Flags:** Any key present without a following value (e.g., `-m`) is stored as a flag (its value is an empty string).
 * - **Values:** Argument values can contain any text, including spaces and special characters.
 * - **Message Splitting:** The special method `parse_message()` splits the message payload from `-data` into a vector by `~`.
 * - **Orphan Values:** Any value not directly following a key is ignored.
 *
 * Design Goals
 * 
 * - **Simplicity:** Friendly for new C++ users—no tricky code or obscure naming.
 * - **Portability:** Works out-of-the-box on Linux and ESP32 (Arduino/ESP-IDF).
 * - **Efficiency:** Minimal memory and CPU usage; no unnecessary dependencies.
 * - **Clarity:** Every variable and function is clearly named and documented.
 *
 * Example
 * 
 * @code
 *   ArgParser parser("-m -rssi 92 -snr 4.5 -data 0xABC~foo~bar~msg");
 *   if (parser.has_arg("-rssi")) {
 *       std::string signal = parser.get_arg("-rssi"); // "92"
 *   }
 *   auto parts = parser.parse_message(); // ["0xABC", "foo", "bar", "msg"]
 * @endcode
 *
 * @author   Leo
 * @author   ChatGPT
 * @date     2025-08-05
 */
class ArgParser {

public:
    ArgParser();
    
    /**
     * @brief Constructor that immediately parses an input string.
     * @param input The argument string to parse (e.g. "-foo 5 -bar test").
     *
     * This lets you create and initialize an ArgParser in one line.
     * Example:
     *   ArgParser parser("-m -data Hello~world");
     */
    explicit ArgParser(const std::string& input);

    /**
     * @brief Parse an argument string and store the key-value pairs.
     * @param input The string to parse, in argument style (e.g. "-rssi 92 -foo bar").
     *
     * You can call this multiple times to re-use the parser for new input.
     * Previous arguments are cleared before parsing the new input.
     */
    void parse_args(const std::string& input);

    /**
     * @brief Check if a specific argument or flag exists in the input.
     * @param key The argument name to check (e.g. "-m" or "-foo").
     * @return true if the argument or flag was found; false otherwise.
     *
     * Useful for detecting flags (like "-m") or optional arguments.
     */
    bool has_arg(const std::string& key) const;

    /**
     * @brief Retrieve the value for a given argument key.
     * @param key The argument name (e.g. "-snr" or "-data").
     * @return The value as a string, or an empty string if not found or if it’s a flag.
     *
     * Example: get_arg("-rssi") might return "92".
     * For flags (like "-m") this returns an empty string.
     */
    std::string get_arg(const std::string& key) const;

    /**
     * @brief Shortcut for retrieving the message payload from the "-data" key.
     * @return The value of the "-data" argument, or an empty string if not present.
     *
     * This is handy if your argument string includes a full message after "-data".
     */
    std::string get_message() const;

    /**
     * @brief Split the message payload (from "-data") into parts, using '~' as a delimiter.
     * @return A vector of strings, each representing a message part.
     *
     * For example, if "-data" is "foo~bar~baz", this returns {"foo", "bar", "baz"}.
     * Returns an empty vector if "-data" is not present.
     */
    std::vector<std::string> parse_message() const;

    /**
     * @brief Get all parsed arguments and their values for debugging or inspection.
     * @return A vector of key-value pairs (argument, value), as stored in the parser.
     *
     * Useful for printing all arguments, checking what was parsed, or logging.
     */
    std::vector<std::pair<std::string, std::string>> get_all_args() const;

    /**
     * @brief Remove all currently stored arguments and values.
     *
     * Use this before parsing a new string if you want to fully reset the parser.
     * This is automatically called by parse_args().
     */
    void clear();


private:
    std::unordered_map<std::string, std::string> arguments;
    void parse_internal(const std::string& input);
    static std::vector<std::string> split(const std::string& input, char delimiter);
};
}  // namespace viatext