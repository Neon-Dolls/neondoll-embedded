// Copyright 2026 Neon-Dolls/neondoll-embedded
// SPDX-License-Identifier: Apache-2.0
#include <neondoll/identity_storage.hpp>
#include <neondoll/platform.hpp>
#include <utility>
#include <array>
#include <cstdio>
#include <limits>

namespace neondoll {
namespace {
constexpr size_t max_record = 1024;
constexpr size_t max_field = 64;
int hex(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}
bool body_id_valid(const std::string& id) {
    if (id.size() != 32) return false;
    for (char c : id) if (hex(c) < 0) return false;
    return true;
}
bool decimal(const std::string& s, uint64_t& out) {
    if (s.empty() || s.size() > 20) return false;
    out = 0;
    for (char c : s) {
        if (c < '0' || c > '9') return false;
        unsigned digit = static_cast<unsigned>(c - '0');
        if (out > (std::numeric_limits<uint64_t>::max() - digit) / 10) return false;
        out = out * 10 + digit;
    }
    return true;
}
uint32_t crc32(const std::string& s) {
    uint32_t crc = 0xffffffffU;
    for (unsigned char c : s) {
        crc ^= c;
        for (int bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (0xedb88320U & (0U - (crc & 1U)));
    }
    return ~crc;
}
std::string encode(const std::string& s) {
    static constexpr char digits[] = "0123456789abcdef";
    std::string out;
    for (unsigned char c : s) { out += digits[c >> 4]; out += digits[c & 15]; }
    return out;
}
bool decode(const std::string& s, std::string& out) {
    if (s.size() > max_field * 2 || s.size() % 2) return false;
    for (size_t i = 0; i < s.size(); i += 2) {
        int a = hex(s[i]), b = hex(s[i + 1]);
        if (a < 0 || b < 0) return false;
        out += static_cast<char>((a << 4) | b);
    }
    return true;
}
}

bool parse_identity(const std::string& data, IdentityState& state) {
    if (data.empty() || data.size() > max_record) return false;
    std::array<std::string, 9> fields;
    size_t start = 0, count = 0;
    for (;;) {
        if (count == fields.size()) return false;
        auto end = data.find(':', start);
        fields[count++] = data.substr(start, end == std::string::npos ? end : end - start);
        if (end == std::string::npos) break;
        start = end + 1;
    }
    IdentityState parsed{};
    if (fields[0] == "v1" && count == 4) {
        parsed.version = 1; // Legacy record: caller upgrades atomically, never changes ID.
    } else if (fields[0] == "v2" && count == 9) {
        const auto checksum_pos = data.rfind(':');
        if (fields[8].size() != 8) return false;
        uint32_t expected = 0;
        for (char c : fields[8]) {
            int n = hex(c); if (n < 0) return false;
            expected = (expected << 4) | static_cast<uint32_t>(n);
        }
        if (crc32(data.substr(0, checksum_pos)) != expected) return false;
        if (!decode(fields[4], parsed.identity.name) ||
            !decode(fields[5], parsed.identity.meta.implementation) ||
            !decode(fields[6], parsed.identity.meta.platform) ||
            !decode(fields[7], parsed.identity.meta.arch)) return false;
        parsed.version = CURRENT_IDENTITY_VERSION;
    } else return false;
    if (!body_id_valid(fields[1]) || !decimal(fields[2], parsed.identity.counter) ||
        !decimal(fields[3], parsed.identity.timestamp)) return false;
    parsed.identity.body_id = fields[1];
    state = std::move(parsed);
    return true;
}

std::string format_identity(const IdentityState& state) {
    const auto& id = state.identity;
    if (state.version != CURRENT_IDENTITY_VERSION || !body_id_valid(id.body_id) ||
        id.name.size() > max_field || id.meta.implementation.size() > max_field ||
        id.meta.platform.size() > max_field || id.meta.arch.size() > max_field) return {};
    std::string data = "v2:" + id.body_id + ":" + std::to_string(id.counter) + ":" +
        std::to_string(id.timestamp) + ":" + encode(id.name) + ":" +
        encode(id.meta.implementation) + ":" + encode(id.meta.platform) + ":" + encode(id.meta.arch);
    char checksum[10];
    std::snprintf(checksum, sizeof(checksum), ":%08lx", static_cast<unsigned long>(crc32(data)));
    return data + checksum;
}

int load_identity(Storage* storage, const std::string& key, IdentityState& out) {
    if (!storage) return -1;
    std::array<char, max_record> buffer{};
    const auto size = storage->read(key.c_str(), buffer.data(), buffer.size());
    if (size == -1) return -3;
    if (size == -2 || size > static_cast<std::ptrdiff_t>(buffer.size())) return -7;
    if (size == -4) return -4;
    if (size < 0) return -6;
    std::string data(buffer.data(), static_cast<size_t>(size));
    const auto colon = data.find(':');
    if (colon != std::string::npos && data[0] == 'v' &&
        data.substr(0, colon) != "v1" && data.substr(0, colon) != "v2") return -5;
    return parse_identity(data, out) ? 0 : -4;
}

int save_identity(Storage* storage, const std::string& key, const IdentityState& state) {
    if (!storage) return -1;
    const auto data = format_identity(state);
    if (data.empty()) return -4;
    return storage->write(key.c_str(), data.data(), data.size()) == 0 ? 0 : -3;
}
} // namespace neondoll
