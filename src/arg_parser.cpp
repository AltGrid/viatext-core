#include "arg_parser.hpp"
#include <sstream>
#include <algorithm>
#include <stdexcept>
#include <cctype> // For isspace, isalpha, etc.

namespace viatext {

ArgParser::ArgParser() {}

ArgParser::ArgParser(const std::string& input) {
    parse_args(input);
}

void ArgParser::parse_args(const std::string& input) {
    clear();
    parse_internal(input);
}

// Renamed and simplified. It just checks for the first key.
std::string ArgParser::get_first_arg_key() const {
    if (arguments.empty()) {
        return "";
    }
    // We can't guarantee order with an unordered_map, so this is
    // an unreliable function. It's better to refactor to a `vector<pair>`
    // to preserve order if that's a requirement. For now, we'll
    // just return a key if one exists.
    return arguments.begin()->first;
}


bool ArgParser::has_arg(const std::string& key) const {
    return arguments.find(key) != arguments.end();
}

std::string ArgParser::get_arg(const std::string& key) const {
    auto it = arguments.find(key);
    if (it != arguments.end()) {
        return it->second;
    }
    return "";
}

// --- NEW: Type-safe getters with error handling ---

int ArgParser::get_int_arg(const std::string& key, int default_value) const {
    std::string value = get_arg(key);
    if (value.empty()) {
        return default_value;
    }
    try {
        return std::stoi(value);
    } catch (const std::invalid_argument& e) {
        // Handle invalid format gracefully
        return default_value;
    } catch (const std::out_of_range& e) {
        // Handle value too large/small gracefully
        return default_value;
    }
}

float ArgParser::get_float_arg(const std::string& key, float default_value) const {
    std::string value = get_arg(key);
    if (value.empty()) {
        return default_value;
    }
    try {
        return std::stof(value);
    } catch (const std::invalid_argument& e) {
        return default_value;
    } catch (const std::out_of_range& e) {
        return default_value;
    }
}

bool ArgParser::get_bool_arg(const std::string& key) const {
    return has_arg(key);
}

// --- End of new additions ---


std::string ArgParser::get_message() const {
    return get_arg("-data");
}

std::vector<std::string> ArgParser::parse_message() const {
    std::string message = get_message();
    if (message.empty()) {
        return {};
    }
    return split(message, '~');
}

std::vector<std::pair<std::string, std::string>> ArgParser::get_all_args() const {
    std::vector<std::pair<std::string, std::string>> result;
    for (const auto& kv : arguments) {
        result.push_back(kv);
    }
    return result;
}

void ArgParser::clear() {
    arguments.clear();
    // argument_order.clear(); // This is no longer used.
}

// *** CRITICAL CHANGE: Refactored `parse_internal` for robustness ***
void ArgParser::parse_internal(const std::string& input) {
    std::string current_key;
    
    // Loop through the input string character by character.
    // This is more robust than using `stringstream` with whitespace-delimited tokens.
    for (size_t i = 0; i < input.length(); ++i) {
        // Skip leading spaces
        if (std::isspace(input[i])) {
            continue;
        }

        // Check for a key, which starts with a hyphen.
        if (input[i] == '-') {
            size_t key_start = i;
            size_t key_end = input.find_first_of(" \t\n", key_start);
            if (key_end == std::string::npos) {
                // This is the last token in the string. It must be a flag.
                current_key = input.substr(key_start);
                arguments[current_key] = ""; // Store as an empty string (flag)
                return; // End of string, so we are done parsing.
            }
            
            // Extract the key itself.
            std::string key = input.substr(key_start, key_end - key_start);

            // Find the start of the next token.
            size_t value_start = input.find_first_not_of(" \t\n", key_end);
            
            // Check if the next token is another key.
            if (value_start == std::string::npos || input[value_start] == '-') {
                // It's a flag, as there's no value or the next thing is another key.
                arguments[key] = "";
                current_key = ""; // Reset current key
                i = key_end; // Continue searching from after this key.
            } else {
                // The next token is a value. Find where it ends.
                size_t value_end = input.find_first_of(" \t\n", value_start);

                // If the value ends with another key, the value is just the text up to that point.
                // Otherwise, it's the rest of the string.
                if (value_end == std::string::npos) {
                    value_end = input.length();
                }

                // Extract the value.
                std::string value = input.substr(value_start, value_end - value_start);
                arguments[key] = value;
                current_key = ""; // Reset current key
                i = value_end - 1; // Continue searching from after this value.
            }
        }
        // If a token doesn't start with '-', we ignore it and continue.
    }
}

// The split function is perfectly fine and requires no changes.
std::vector<std::string> ArgParser::split(const std::string& input, char delimiter) {
    std::vector<std::string> result;
    std::string part;
    std::istringstream ss(input);
    while (std::getline(ss, part, delimiter)) {
        result.push_back(part);
    }
    return result;
}

} // namespace viatext