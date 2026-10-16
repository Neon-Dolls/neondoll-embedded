// Copyright 2026 Neon-Dolls/neondoll-embedded
// SPDX-License-Identifier: Apache-2.0

#include <neondoll/identity_storage.hpp>
#include <neondoll/platform.hpp>
#include <neondoll/identity.hpp>
#include <sstream>
#include <iomanip>
#include <cctype>
#include <cstdint>
#include <algorithm>

namespace neondoll {

// Helper to check if a character is a hex digit
static inline bool is_hex(char c) {
    return std::isxdigit(static_cast<unsigned char>(c));
}

// Helper to convert a hex char to its integer value
static inline int hex_to_int(char c) {
    if (std::isdigit(static_cast<unsigned char>(c)))
        return c - '0';
    c = std::tolower(static_cast<unsigned char>(c));
    return c - 'a' + 10;
}

// Helper to parse a hexadecimal string of fixed length
// Returns true if successful, false otherwise.
// On success, advances pos by len and sets out to the parsed string.
static bool parse_hex_string_fixed(const std::string& data, size_t& pos, size_t len, std::string& out) {
    if (pos + len > data.size()) {
        return false;
    }
    out.assign(data.substr(pos, len));
    // Validate that it's all hex digits
    for (size_t i = 0; i < len; ++i) {
        if (!is_hex(data[pos + i])) {
            return false;
        }
    }
    pos += len;
    return true;
}

// Helper to parse a decimal number (unsigned)
// Returns true if successful, false otherwise.
// On success, advances pos past the number and sets out to the parsed value.
// Includes overflow protection for uint64_t.
static bool parse_decimal(const std::string& data, size_t& pos, uint64_t& out) {
    size_t start = pos;
    while (pos < data.size() && std::isdigit(data[pos])) {
        ++pos;
    }
    if (start == pos) {
        return false; // no digits
    }
    out = 0;
    for (size_t i = start; i < pos; ++i) {
        // Check for overflow before multiplying and adding
        if (out > (UINT64_MAX - (data[i] - '0')) / 10) {
            return false; // overflow would occur
        }
        out = out * 10 + (data[i] - '0');
    }
    return true;
}

// Helper to format an unsigned integer as decimal string
static std::string format_decimal(uint64_t value) {
    if (value == 0) {
        return "0";
    }
    std::string result;
    while (value > 0) {
        result.insert(result.begin(), static_cast<char>('0' + (value % 10)));
        value /= 10;
    }
    return result;
}

// Helper to format a byte array as hex string
static std::string format_hex(const uint8_t* data, size_t len) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (size_t i = 0; i < len; ++i) {
        ss << std::setw(2) << static_cast<int>(data[i]);
    }
    return ss.str();
}

// Parse identity from colon-separated string: v1:{body_id_hex}:{counter}:{timestamp}
// Parse identity from colon-separated string: v1:{body_id_hex}:{counter}:{timestamp}
bool parse_identity(const std::string& data, IdentityState& state) {
    size_t pos = 0;

    // Check version prefix: "v1:"
    if (pos + 3 > data.size() || data.substr(pos, 3) != "v1:") {
        return false;
    }
    pos += 3;

    // Parse body_id_hex: 32 hex characters (16 bytes)
    if (!parse_hex_string_fixed(data, pos, 32, state.identity.body_id)) {
        return false;
    }

    // Expect colon
    if (pos >= data.size() || data[pos] != ':') {
        return false;
    }
    ++pos;

    // Parse counter (unsigned integer)
    if (!parse_decimal(data, pos, state.identity.counter)) {
        return false;
    }

    // Expect colon
    if (pos >= data.size() || data[pos] != ':') {
        return false;
    }
    ++pos;

    // Parse timestamp (unsigned integer)
    if (!parse_decimal(data, pos, state.identity.timestamp)) {
        return false;
    }

    // Expect end of string (no signature field anymore)
    if (pos != data.size()) {
        return false;
    }

    // Set the version
    state.version = CURRENT_IDENTITY_VERSION;

    return true;
}

// Format identity to colon-separated string: v1:{body_id_hex}:{counter}:{timestamp}
// Format identity to colon-separated string: v1:{body_id_hex}:{counter}:{timestamp}
std::string format_identity(const IdentityState& identity) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    ss << "v1:"
       << std::setw(32) << identity.identity.body_id << ":"
       << std::dec << identity.identity.counter << ":"
       << std::dec << identity.identity.timestamp;
    return ss.str();
}

int load_identity(Storage* storage,
                 const std::string& key,
                 IdentityState& out) {
    if (!storage) {
        return -1; // null storage pointer
    }

    // Read the persisted data
    constexpr size_t MAX_IDENTITY_SIZE = 1024;
    std::string buffer(MAX_IDENTITY_SIZE, '\0');
    ssize_t bytes_read = storage->read(key.c_str(), buffer.data(), buffer.size());

    if (bytes_read < 0) {
        if (bytes_read == -1) {
            return -3; // key not found (genuinely missing key - permits first boot)
        } else {
            return -6; // storage read I/O error
        }
    }

    // Resize buffer to actual data read
    buffer.resize(static_cast<size_t>(bytes_read));

    // Treat empty buffer as corrupt data (never first boot)
    if (buffer.empty()) {
        return -4; // persisted data is corrupt or incomplete
    }

    // Parse the identity data
    if (!parse_identity(buffer, out)) {
        return -4; // persisted data is corrupt or incomplete
    }

    // Check version compatibility
    if (out.version != CURRENT_IDENTITY_VERSION) {
        return -5; // persisted data has incompatible version
    }

    return 0; // success
}

int save_identity(Storage* storage,
                 const std::string& key,
                 const IdentityState& identity) {
    if (!storage) {
        return -1; // null storage pointer
    }

    // Serialize the identity
    std::string data = format_identity(identity);

    // Write the data
    int result = storage->write(key.c_str(), data.data(), data.size());
    if (result < 0) {
        return -3; // storage write I/O error
    }

    return 0; // success
}

} // namespace neondoll