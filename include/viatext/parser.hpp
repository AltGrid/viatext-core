#pragma once

#include <string>
#include <optional>
#include "viatext/message.hpp"

#ifdef ARDUINO
  #include "ArduinoJson.hpp"
#else
  #include "ajson_config.hpp"
  #include "ArduinoJson.hpp"
#endif


namespace viatext {
namespace parser {

/**
 * @brief Parse a JSON (or protocol) string into a Message object.
 * @param jsonStr  Input text (JSON on desktop; JSON or ArduinoJson on embedded).
 * @return std::optional<Message> containing the parsed Message on success, or std::nullopt on failure.
 */
std::optional<Message> from_json(const std::string& jsonStr);

/**
 * @brief Serialize a Message object to a JSON string.
 * @param msg  The Message to serialize.
 * @return A JSON-formatted std::string.
 */
std::string to_json(const Message& msg);

/**
 * @brief Build a JSON string for an event (error, ack, status).
 * @param type    Type of event (e.g. "error", "ack").
 * @param detail  Human-readable detail string.
 * @return A JSON-formatted std::string containing { "type": ..., "detail": ... }.
 */
std::string event_json(const std::string& type, const std::string& detail);

/**
 * @brief Build a JSON string for a directive message.
 * @param from     Sender node ID.
 * @param to       Destination node ID.
 * @param stamp    Message stamp.
 * @param command  Directive command string.
 * @return A JSON-formatted std::string containing { "type":"directive", ... }.
 */
std::string directive_json(const std::string& from,
                           const std::string& to,
                           const std::string& stamp,
                           const std::string& command);

/**
 * @brief Extract the "type" field from a JSON string.
 * @param jsonStr  The JSON-formatted input string.
 * @return The value of the "type" field, or an empty string if missing or parse fails.
 */
std::string get_type(const std::string& jsonStr);

} // namespace parser
} // namespace viatext
