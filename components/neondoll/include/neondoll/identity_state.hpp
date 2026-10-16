// Copyright 2026 Neon-Dolls/neondoll-embedded
// SPDX-License-Identifier: Apache-2.0

#ifndef NEONDOLL_IDENTITY_STATE_HPP
#define NEONDOLL_IDENTITY_STATE_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <array>
#include <neondoll/identity.hpp>

namespace neondoll {

/** @brief Persistable form of Body identity with versioning envelope.
 *         Allows runtime to evolve file format without changing wire shape.
 */
struct IdentityState {
    int version;                 ///< Format version
    BodyIdentity identity;       ///< The actual identity data
};

} // namespace neondoll

#endif // NEONDOLL_IDENTITY_STATE_HPP