/**
 * @file test-message-id.cpp
 * @brief CLI Test Tool for ViaText MessageID Structure
 *
 * This program allows users to test all supported constructors of the `MessageID` class,
 * used in the ViaText protocol as a compact 5-byte routing and control header.
 *
 * It leverages CLI11 to expose a user-friendly command-line interface for inspecting
 * how different constructor inputs affect the internal `MessageID` fields.
 *
 * @section usage Basic Usage
 *
 * Compile and run:
 * @code
 *   ./test-message-id <subcommand> [options...]
 * @endcode
 *
 * Example:
 * @code
 *   ./test-message-id raw --seq 42 --part 1 --total 3 --hops 5 --flags 6
 *
 *   Default constructor (all fields = 0)
 *   ./test-message-id default
 *
 *   Constructor from packed 5-byte integer (uint64_t)
 *   ./test-message-id int --value 192497174417
 *
 *   Constructor from hex string (10 hex digits)
 *   ./test-message-id hex --hex 2CF00507A1
 *
 *   Constructor from 5-byte buffer
 *   ./test-message-id buffer --buf 44 240 5 7 161
 *
 *   Constructor from raw field values
 *   ./test-message-id raw --seq 11248 --part 5 --total 7 --hops 10 --flags 3
 *
 *   Constructor from raw fields with individual flags
 *   ./test-message-id flags --seq 11248 --part 5 --total 7 --hops 10 --req_ack --ack --enc
 *
 * @endcode
 *
 * @section subcommands Available Subcommands
 *
 * Each subcommand maps directly to one of the C++ constructors for the `MessageID` struct:
 *
 * @subcommand default
 * - Uses the default constructor: `MessageID()`
 * - Initializes all fields to zero
 *
 * @subcommand int
 * - Constructs from a 40-bit packed integer:
 *   `MessageID(uint64_t packed_value)`
 * - Requires: `--value <uint64_t>`
 *
 * @subcommand hex
 * - Constructs from a 10-character hex string (e.g., "4F2B000131"):
 *   `MessageID(const char* hex_str)`
 * - Requires: `--hex <string>`
 * - Accepts optional "0x" prefix
 *
 * @subcommand buffer
 * - Constructs from a raw 5-byte buffer:
 *   `MessageID(const uint8_t* data, size_t len)`
 * - Requires: `--buf <b1> <b2> <b3> <b4> <b5>` (5 integers, 0–255)
 *
 * @subcommand raw
 * - Constructs from raw field values:
 *   `MessageID(uint16_t seq, uint8_t part, uint8_t total, uint8_t hops, uint8_t flags)`
 * - Requires: `--seq --part --total --hops --flags`
 * - Flags must be passed as a 4-bit bitmask (0–15)
 *
 * @subcommand flags
 * - Constructs from raw fields + flag booleans:
 *   `MessageID(seq, part, total, hops, req_ack, is_ack, encrypted, unused)`
 * - Requires: `--seq --part --total --hops`
 * - Optional flags:
 *     - `--req_ack` (bit 0)
 *     - `--ack`     (bit 1)
 *     - `--enc`     (bit 2)
 *     - `--unused`  (bit 3)
 *
 * @section output Printed Fields
 *
 * For every test, the following fields are printed:
 * - Sequence
 * - Part
 * - Total
 * - Hops
 * - Flags (hex)
 *
 * And the following flag checks are reported:
 * - `requests_acknowledgment()`
 * - `is_acknowledgment()`
 * - `is_encrypted()`
 *
 * Example Output:
 * @code
 * Sequence:    28
 * Part:        5
 * Total:       7
 * Hops:        10
 * Flags:       0xC
 *   Requests ACK:    false
 *   Is ACK:          true
 *   Is Encrypted:    true
 * @endcode
 *
 * @note This utility is intended for low-level testing of MessageID behavior
 *       prior to integration with the full ViaText core.
 *
 * @author Leo
 * @author ChatGPT
 */

#include <iostream>
#include <vector>
#include <string>
#include "CLI/CLI11.hpp"
#include "viatext/message_id.hpp"

using namespace viatext;

void print_fields(const MessageID& msg) {
    std::cout << "Sequence:    " << msg.sequence << "\n"
              << "Part:        " << static_cast<int>(msg.part) << "\n"
              << "Total:       " << static_cast<int>(msg.total) << "\n"
              << "Hops:        " << static_cast<int>(msg.hops) << "\n"
              << "Flags:       0x" << std::hex << static_cast<int>(msg.flags) << std::dec << "\n"
              << "  Requests ACK:    " << (msg.requests_acknowledgment() ? "true" : "false") << "\n"
              << "  Is ACK:          " << (msg.is_acknowledgment() ? "true" : "false") << "\n"
              << "  Is Encrypted:    " << (msg.is_encrypted() ? "true" : "false") << "\n"
              << "  Message ID:      " << (msg.to_hex_string()) << "\n";
}

int main(int argc, char** argv) {

    CLI::App app{"ViaText MessageID Constructor Tester"};

    // Subcommand: default
    auto cmd_default = app.add_subcommand("default", "Use default constructor");

    // Subcommand: int
    uint64_t int_value;
    auto cmd_int = app.add_subcommand("int", "Use 5-byte integer constructor");
    cmd_int->add_option("--value", int_value, "40-bit packed integer")->required();

    // Subcommand: hex
    std::string hex_value;
    auto cmd_hex = app.add_subcommand("hex", "Use hex string constructor");
    cmd_hex->add_option("--hex", hex_value, "10-digit hex string (e.g., 4F2B000131)")->required();

    // Subcommand: buffer
    std::vector<int> buf_bytes;
    auto cmd_buf = app.add_subcommand("buffer", "Use 5-byte buffer constructor");
    cmd_buf->add_option("--buf", buf_bytes, "List of 5 integers (0–255)")->expected(5);

    // Subcommand: raw
    uint16_t seq; uint8_t part, total, hops, flags;
    auto cmd_raw = app.add_subcommand("raw", "Use raw field constructor");
    cmd_raw->add_option("--seq", seq)->required();
    cmd_raw->add_option("--part", part)->required();
    cmd_raw->add_option("--total", total)->required();
    cmd_raw->add_option("--hops", hops)->required();
    cmd_raw->add_option("--flags", flags)->required();

    // Subcommand: flags
    bool req_ack = false, is_ack = false, encrypted = false, unused = false;
    auto cmd_flags = app.add_subcommand("flags", "Use flags-based constructor");
    cmd_flags->add_option("--seq", seq)->required();
    cmd_flags->add_option("--part", part)->required();
    cmd_flags->add_option("--total", total)->required();
    cmd_flags->add_option("--hops", hops)->required();
    cmd_flags->add_flag("--req_ack", req_ack);
    cmd_flags->add_flag("--ack", is_ack);
    cmd_flags->add_flag("--enc", encrypted);
    cmd_flags->add_flag("--unused", unused);

    CLI11_PARSE(app, argc, argv);

    if (*cmd_default) {
        MessageID msg;
        print_fields(msg);
    } else if (*cmd_int) {
        MessageID msg(int_value);
        print_fields(msg);
    } else if (*cmd_hex) {
        MessageID msg(hex_value.c_str());
        print_fields(msg);
    } else if (*cmd_buf) {
        uint8_t data[5];
        for (size_t i = 0; i < 5; ++i) data[i] = static_cast<uint8_t>(buf_bytes[i]);
        MessageID msg(data, 5);
        print_fields(msg);
    } else if (*cmd_raw) {
        MessageID msg(seq, part, total, hops, flags);
        print_fields(msg);
    } else if (*cmd_flags) {
        MessageID msg(seq, part, total, hops, req_ack, is_ack, encrypted, unused);
        print_fields(msg);
    } else {
        std::cout << "Please provide a valid subcommand. Use --help for options.\n";
    }

    return 0;
}
