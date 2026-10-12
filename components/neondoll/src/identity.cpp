// Copyright 2026 Neon-Dolls/neondoll-embedded
// SPDX-License-Identifier: Apache-2.0

#include <neondoll/identity.hpp>
#include <cstring> // for memcpy

namespace neondoll {

int generate_body_id(EntropySource* entropy, BodyID& out) {
    if (!entropy) {
        return -1; // null entropy pointer
    }
    
    // Initialize entropy source
    int ret = entropy->init();
    if (ret != 0) {
        return -2; // entropy init failed
    }
    
    // Generate 16 random bytes (128 bits of entropy)
    std::array<uint8_t, 16> buf{};
    ret = entropy->get_random(buf.data(), buf.size());
    if (ret < 0) {
        entropy->deinit(); // Clean up on failure
        return -3; // failed to get random bytes
    }
    
    // Deinitialize entropy source
    entropy->deinit();
    
    // Create BodyID with "body_" prefix + hex encoding
    out = "body_";
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
    
    return 0; // success
}

} // namespace neondoll