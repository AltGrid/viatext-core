// arg_parser.cpp
#include "arg_parser.hpp"
#include <sstream>
#include <algorithm>

namespace viatext {

ArgParser::ArgParser() {}

ArgParser::ArgParser(const std::string& input) {
    parse_args(input);
}

void ArgParser::parse_args(const std::string& input) {
    clear();
    parse_internal(input);
}

std::string ArgParser::get_message_type() const {
    if (!argument_order.empty())
        return argument_order[0];
    return "";
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
    argument_order.clear();
}

void ArgParser::parse_internal(const std::string& input) {
    std::istringstream stream(input);
    std::string token;
    std::string last_key;

    while (stream >> token) {
        if (token.size() > 1 && token[0] == '-' && std::isalpha(token[1])) {
            last_key = token;
            // Peek at next char: if space or end, it's a flag
            std::streampos pos = stream.tellg();
            std::string next_token;
            if (!(stream >> next_token)) {
                // No next token, so this key is a flag
                arguments[last_key] = "";
                if (std::find(argument_order.begin(), argument_order.end(), last_key) == argument_order.end()) {
                    argument_order.push_back(last_key);
                }
                break;
            }
            if (next_token.size() > 0 && next_token[0] == '-') {
                arguments[last_key] = "";
                if (std::find(argument_order.begin(), argument_order.end(), last_key) == argument_order.end()) {
                    argument_order.push_back(last_key);
                }
                // "rewind" for next loop iteration
                stream.seekg(pos);
                continue;
            }
            // Otherwise, next token is the value for this key
            arguments[last_key] = next_token;
            if (std::find(argument_order.begin(), argument_order.end(), last_key) == argument_order.end()) {
                argument_order.push_back(last_key);
            }
        }
        // Else: ignore unexpected values not attached to a key
    }
}


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