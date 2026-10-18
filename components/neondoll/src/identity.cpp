// Copyright 2026 Neon-Dolls/neondoll-embedded
// SPDX-License-Identifier: Apache-2.0

#include <neondoll/identity.hpp>
#include <neondoll/platform.hpp>
#include <utility>
#include <cstring> // for memcpy
#include <cstdint>
#include <array>

namespace neondoll {

int generate_body_id(EntropySource* entropy, BodyID& out) {
    if (!entropy) {
        return -1; // null entropy pointer
    }

    // Generate 16 random bytes (128 bits of entropy)
    std::array<uint8_t, 16> buf{};
    int ret = entropy->get_random(buf.data(), buf.size());
    if (ret != 0) {
        return -3; // failed to get random bytes
    }

    out.clear();
    out.reserve(32);
    // Create BodyID as hex encoding (32 characters for 16 bytes)
    static const char hex_chars[] = "0123456789abcdef";
    for (uint8_t b : buf) {
        out.push_back(hex_chars[b >> 4]);      // high nibble
        out.push_back(hex_chars[b & 0x0F]);    // low nibble
    }

    return 0; // success
}

int new_identity_state(EntropySource* entropy, 
                      const std::string& name,
                      const BodyMetadata& meta,
                      IdentityState& out) {
    if (!entropy) {
        return -1; // null entropy pointer
    }

    // Generate a new Body ID
    BodyID body_id;
    int ret = generate_body_id(entropy, body_id);
    if (ret != 0) {
        return ret; // propagate error from generate_body_id
    }

    // Build the identity state
    out.version = CURRENT_IDENTITY_VERSION;
    out.identity.body_id = std::move(body_id);
    out.identity.name = name; // copy name (can be empty)
    out.identity.meta = meta; // copy metadata
    out.identity.counter = 0; // Initialize counter to 0
    out.identity.timestamp = 0; // Initialize timestamp to 0 (will be set by caller if needed)
    out.identity.signature = ""; // Signature is validated during loading, not pre-initialized

    return 0; // success
}

} // namespace neondoll
