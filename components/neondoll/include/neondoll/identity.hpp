// Copyright 2026 Neon-Dolls/neondoll-embedded
// SPDX-License-Identifier: Apache-2.0

#ifndef NEONDOLL_IDENTITY_HPP
#define NEONDOLL_IDENTITY_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <array>
#include "platform.hpp"

namespace neondoll {

/** @brief Opaque Body identifier - independent of process, connection, session, etc. */
using BodyID = std::string;

/** @brief Generates a fresh high-entropy Body ID from the entropy source.
 *  @param entropy  Entropy source to use for randomness
 *  @param[out] out BodyID to receive the generated identifier
 *  @return 0 on success, negative error code on failure:
 *          -1: null entropy pointer
 *          -2: entropy init failed
 *          -3: failed to get random bytes
 */
int generate_body_id(EntropySource* entropy, BodyID& out);

/** @brief Body metadata that helps Core reason about compatibility.
 *         This is the M1 surface; protocol may add fields without changing identity semantics.
 */
struct BodyMetadata {
    std::string implementation;  ///< e.g., "neondoll-embedded"
    std::string platform;        ///< e.g., "esp32", "linux", "windows"
    std::string arch;            ///< e.g., "xtensa", "x86_64", "arm"
};

/** @brief Complete Body identity: opaque ID plus descriptive metadata.
 *         Portable and survives restarts.
 */
struct BodyIdentity {
    BodyID body_id;              ///< Opaque Body ID (e.g., "body_"+16-byte-hex)
    std::string name;            ///< Optional human-readable name
    BodyMetadata meta;           ///< Implementation/platform/arch metadata
    uint64_t counter;            ///< Monotonically increasing counter for identity versions
    uint64_t timestamp;          ///< Timestamp of identity creation/update
    std::string signature;       ///< Cryptographic signature of the identity (64 hex chars)
};

/** @brief Current format version of the persistable identity envelope.
 *         Bump this (and teach load_identity/save_identity) before changing shape.
 */
constexpr int CURRENT_IDENTITY_VERSION = 1;

/** @brief Persistable form of Body identity with versioning envelope.
 *         Allows runtime to evolve file format without changing wire shape.
 */
struct IdentityState {
    int version;                 ///< Format version
    BodyIdentity identity;       ///< The actual identity data
};

/** @brief Builds a fresh identity state with new Body ID and given metadata/name.
 *  @param entropy  Entropy source for generating the Body ID
 *  @param name     Optional human-readable name (can be empty)
 *  @param meta     Body metadata (implementation/platform/arch)
 *  @param[out] out Receives the newly created identity state
 *  @return 0 on success, negative error code on failure:
 *          -1: null entropy pointer
 *          -2: entropy init failed
 *          -3: failed to get random bytes for Body ID
 */
int new_identity_state(EntropySource* entropy, 
                      const std::string& name,
                      const BodyMetadata& meta,
                      IdentityState& out);

} // namespace neondoll

#endif // NEONDOLL_IDENTITY_HPP