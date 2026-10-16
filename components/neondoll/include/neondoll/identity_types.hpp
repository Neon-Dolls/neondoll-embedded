// Copyright 2026 Neon-Dolls/neondoll-embedded
// SPDX-License-Identifier: Apache-2.0

#ifndef NEONDOLL_IDENTITY_TYPES_HPP
#define NEONDOLL_IDENTITY_TYPES_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <array>

namespace neondoll {

/** @brief Opaque Body identifier - independent of process, connection, session, etc. */
using BodyID = std::string;

/** @brief Body metadata that helps Core reason about compatibility.
 *         This is the M1 surface; protocol may add fields without changing identity semantics.
 */
struct BodyMetadata {
    std::string implementation;  ///< e.g., \"neondoll-embedded\"\n    std::string platform;        ///< e.g., \"esp32\", \"linux\", \"windows\"\n    std::string arch;            ///< e.g., \"xtensa\", \"x86_64\", \"arm\"\n};

/** @brief Complete Body identity: opaque ID plus descriptive metadata.
 *         Portable and survives restarts.
 */
struct BodyIdentity {
    BodyID body_id;              ///< Opaque Body ID (e.g., \"body_\"+16-byte-hex)\n    std::string name;            ///< Optional human-readable name\n    BodyMetadata meta;           ///< Implementation/platform/arch metadata\n    uint64_t counter;            ///< Monotonically increasing counter for identity versions\n    uint64_t timestamp;          ///< Timestamp of identity creation/update\n    std::string signature;       ///< Cryptographic signature of the identity (64 hex chars)\n};

} // namespace neondoll

#endif // NEONDOLL_IDENTITY_TYPES_HPP
