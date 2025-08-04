/**
 * @file parser.hpp
 * @brief JSON parsing and serialization layer for ViaText messages.
 * @details
 *   The parser module centralizes all conversion between raw text (JSON or ArduinoJson)
 *   and the @ref Message data structure. It provides a uniform API for:
 *     - Parsing incoming JSON strings into Message objects (@c from_json).
 *     - Serializing Message objects into JSON strings (@c to_json).
 *     - Constructing standard event and directive messages (@c event_json, @c directive_json).
 *     - Extracting the message “type” field without a full parse (@c get_type).
 *
 *   ## Dual Backend Support
 *   - **Desktop/Linux builds** (no @c ARDUINO defined) use the
 *     [nlohmann::json](https://github.com/nlohmann/json) library for full-featured
 *     JSON support.
 *   - **Embedded/Arduino builds** (@c ARDUINO defined) use the
 *     [ArduinoJson](https://arduinojson.org/) library with fixed-size
 *     @c StaticJsonDocument buffers for predictable memory usage.
 *
 *   ## Avoiding Unwanted Arduino Dependencies
 *   ArduinoJson can emit `#include <Arduino.h>` if its Arduino-specific extensions
 *   are enabled. To prevent this on non-embedded builds, we prepend:
 *     @code
 *     #include "ajson_config.hpp"
 *     @endcode
 *   which disables all ArduinoJson Arduino* features (String, Stream, Print, PROGMEM)
 *   when @c ARDUINO is not defined, ensuring desktop compilation never pulls in
 *   @c Arduino.h.
 *
 *   ## Error Suppression & Robustness
 *   All parsing functions:
 *    - Catch and suppress exceptions or parse errors.
 *    - Return @c std::nullopt (for @c from_json) or empty strings (for @c get_type)
 *      rather than throwing, making the parser safe to use with untrusted input.
 *
 *   @note
 *     For pipe-delimited wire formats (LoRa/serial), perform text↔Message conversion
 *     *before* and *after* calls to the parser, so core logic always deals in JSON.
 *
 *   @author Leo
 *   @author ChatGPT
 *   @date   2025-08-04
 */

#include "viatext/parser.hpp"

#ifdef ARDUINO

#include <ArduinoJson.hpp>
using ArduinoJson::StaticJsonDocument;
using ArduinoJson::deserializeJson;
using ArduinoJson::JsonObject;

#else

#include <json.hpp>
using nlohmann::json;

#endif

namespace viatext {
namespace parser {

/**
 * @brief Parse a JSON (or protocol) string into a Message object.
 * @param jsonStr  JSON-formatted input (desktop) or raw text for ArduinoJson (embedded).
 * @return std::optional<Message> containing the parsed Message on success, or std::nullopt on failure.
 *
 * @details
 *   - On desktop builds (no ARDUINO), uses nlohmann::json::parse(),
 *     verifies required fields (“stamp”, “from”, “to”, “payload”), then populates a Message.
 *   - On Arduino builds, uses ArduinoJson::deserializeJson() into a StaticJsonDocument,
 *     checks for required keys, and extracts values with default fallbacks.
 *   - All exceptions or parse errors are caught and result in std::nullopt,
 *     making this function safe against malformed or malicious input.
 */
std::optional<Message> from_json(const std::string& jsonStr) {
#ifdef ARDUINO
    // Embedded path: use fixed-size buffer to bound memory usage
    StaticJsonDocument<256> doc;
    auto error = deserializeJson(doc, jsonStr);
    if (error) {
        // Parsing failed (invalid JSON or buffer overflow)
        return std::nullopt;
    }

    JsonObject obj = doc.as<JsonObject>();
    // Ensure required keys are present
    if (!obj.containsKey("stamp") ||
        !obj.containsKey("from")  ||
        !obj.containsKey("to")    ||
        !obj.containsKey("payload")) {
        return std::nullopt;
    }

    // Populate Message fields, using '|' operator to supply defaults
    Message msg;
    msg.stamp     = obj["stamp"].as<std::string>();
    msg.from      = obj["from"].as<std::string>();
    msg.to        = obj["to"].as<std::string>();
    msg.payload   = obj["payload"].as<std::string>();
    msg.ttl       = obj["ttl"]   | 0;
    msg.encrypted = obj["encrypted"] | false;

    return msg;
#else
    // Desktop path: full-featured JSON parsing with exception safety
    try {
        auto j = json::parse(jsonStr);

        // Verify the presence of essential fields
        if (!j.contains("stamp") ||
            !j.contains("from")  ||
            !j.contains("to")    ||
            !j.contains("payload")) {
            return std::nullopt;
        }

        // Construct Message and extract each field with defaults
        Message msg;
        msg.stamp     = j.value("stamp", "");
        msg.from      = j.value("from", "");
        msg.to        = j.value("to", "");
        msg.payload   = j.value("payload", "");
        msg.ttl       = j.value("ttl", 0);
        msg.encrypted = j.value("encrypted", false);

        return msg;
    }
    catch (...) {
        // Any parse exception results in an empty optional
        return std::nullopt;
    }
#endif
}


/**
 * @brief Serialize a Message object to a JSON string.
 * @param msg  The Message to serialize.
 * @return A JSON-formatted std::string representing the message.
 *
 * @details
 *   - On embedded/Arduino builds, uses ArduinoJson with a fixed-size StaticJsonDocument
 *     to bound memory usage. Fields are written into the document, then serialized
 *     into the output string via serializeJson().
 *   - On desktop/Linux builds, uses nlohmann::json to construct a JSON object
 *     and calls dump() to obtain the string form.
 *   - In the Arduino path, we reserve the output string capacity to match the document
 *     capacity, minimizing reallocations.
 */
std::string to_json(const Message& msg) {
#ifdef ARDUINO
    // Embedded path: fixed-capacity JSON document for predictable RAM usage
    StaticJsonDocument<256> doc;
    auto obj = doc.to<JsonObject>();

    // Populate the JSON object with message fields
    obj["type"]      = "viatext";
    obj["stamp"]     = msg.stamp;
    obj["from"]      = msg.from;
    obj["to"]        = msg.to;
    obj["payload"]   = msg.payload;
    obj["ttl"]       = msg.ttl;
    obj["encrypted"] = msg.encrypted;

    // Serialize into a std::string, reserving space up-front
    std::string out;
    out.reserve(doc.capacity());
    serializeJson(obj, out);
    return out;
#else
    // Desktop path: full-featured JSON construction
    json j;
    j["type"]      = "viatext";
    j["stamp"]     = msg.stamp;
    j["from"]      = msg.from;
    j["to"]        = msg.to;
    j["payload"]   = msg.payload;
    j["ttl"]       = msg.ttl;
    j["encrypted"] = msg.encrypted;
    return j.dump();
#endif
}


/**
 * @brief Build a JSON string for an event (error, ack, status).
 * @param type    The event type (e.g., "error", "ack", "status").
 * @param detail  A human-readable description or payload for the event.
 * @return A JSON-formatted std::string with fields { "type": type, "detail": detail }.
 *
 * @details
 *   - On embedded/Arduino builds, uses ArduinoJson with a StaticJsonDocument of fixed size (128 bytes)
 *     to ensure predictable memory consumption. Fields are placed into the document, which is then
 *     serialized into the output string with reserve(doc.capacity()) to avoid reallocations.
 *   - On desktop/Linux builds, constructs a nlohmann::json object and calls dump().
 *   - This helper is used by Core::post_event() to emit structured status and error messages
 *     without duplicating JSON boilerplate in core logic.
 */
std::string event_json(const std::string& type,
                       const std::string& detail) {
#ifdef ARDUINO
    // Embedded path: fixed-capacity JSON doc for event serialization
    StaticJsonDocument<128> doc;
    auto obj = doc.to<JsonObject>();

    // Populate JSON object for the event
    obj["type"]   = type;
    obj["detail"] = detail;

    // Serialize with reserved capacity to minimize dynamic allocations
    std::string out;
    out.reserve(doc.capacity());
    serializeJson(obj, out);
    return out;
#else
    // Desktop path: simple JSON construction
    json j;
    j["type"]   = type;
    j["detail"] = detail;
    return j.dump();
#endif
}


/**
 * @brief Build a JSON string for a directive message.
 * @param from     Sender node ID.
 * @param to       Destination node ID.
 * @param stamp    Unique message stamp.
 * @param command  Directive command string (e.g., “reboot”, “set-id”).
 * @return A JSON-formatted std::string containing:
 *   {
 *     "type":"directive",
 *     "from":<from>,
 *     "to":<to>,
 *     "stamp":<stamp>,
 *     "command":<command>
 *   }
 *
 * @details
 *   - On embedded/Arduino builds, uses an ArduinoJson StaticJsonDocument
 *     sized to 192 bytes to accommodate all fields. Fields are inserted
 *     into the document and then serialized into a std::string using
 *     reserve(doc.capacity()) to minimize reallocations.
 *   - On desktop/Linux builds, constructs a nlohmann::json object and
 *     calls dump() for the output string.
 *   - Used by wrapper code or Core logic to send control directives
 *     (e.g., configuration changes, system commands) through the mesh.
 */
std::string directive_json(const std::string& from,
                           const std::string& to,
                           const std::string& stamp,
                           const std::string& command) {
#ifdef ARDUINO
    StaticJsonDocument<192> doc;
    auto obj = doc.to<JsonObject>();

    obj["type"]    = "directive";
    obj["from"]    = from;
    obj["to"]      = to;
    obj["stamp"]   = stamp;
    obj["command"] = command;

    std::string out;
    out.reserve(doc.capacity());
    serializeJson(obj, out);
    return out;
#else
    json j;
    j["type"]    = "directive";
    j["from"]    = from;
    j["to"]      = to;
    j["stamp"]   = stamp;
    j["command"] = command;
    return j.dump();
#endif
}

/**
 * @brief Extract the `type` field from a JSON string without full parsing.
 * @param jsonStr  The JSON-formatted input string.
 * @return The value of the `"type"` field, or an empty string if missing,
 *         parse fails, or the field is not a string.
 *
 * @details
 *   - On embedded/Arduino builds, uses ArduinoJson to deserialize into a
 *     StaticJsonDocument<64>, then reads the `"type"` member.
 *   - On desktop/Linux builds, uses nlohmann::json::parse() and value()
 *     to fetch the `"type"` field, catching any exceptions and returning
 *     `""` on errors.
 *   - This lightweight helper avoids throwing or allocating large structures
 *     when only the message type is needed, improving performance in hot paths.
 */
std::string get_type(const std::string& jsonStr) {
#ifdef ARDUINO
    StaticJsonDocument<64> doc;
    auto err = deserializeJson(doc, jsonStr);
    if (err) return "";
    return doc["type"].as<std::string>();
#else
    try {
        auto j = json::parse(jsonStr);
        return j.value("type", "");
    } catch (...) {
        return "";
    }
#endif
}


} // namespace parser
} // namespace viatext
