// Copyright 2026 Neon-Dolls/neondoll-embedded
// SPDX-License-Identifier: Apache-2.0

#ifndef NEONDOLL_IDENTITY_STORAGE_HPP
#define NEONDOLL_IDENTITY_STORAGE_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <system_error>

#include "identity.hpp"

namespace neondoll {

/** @brief Loads persisted identity from storage.
 *  @param storage  Storage backend to read from
 *  @param key      Storage key under which identity is persisted
 *  @param[out] out Receives the loaded identity state
 *  @return 0 on success, negative error code on failure:
 *          -1: null storage pointer
 *          -2: storage init failed
 *          -3: key not found in storage
 *          -4: persisted data is corrupt or incomplete
 *          -5: persisted data has incompatible version
 *          -6: storage read I/O error
 *          -7: persisted record exceeds buffer limit
 */
int load_identity(Storage* storage, 
                 const std::string& key,
                 IdentityState& out);

/** @brief Persists identity to storage.
 *  @param storage  Storage backend to write to
 *  @param key      Storage key under which to persist identity
 *  @param identity Identity state to persist
 *  @return 0 on success, negative error code on failure:
 *          -1: null storage pointer
 *          -2: storage init failed
 *          -3: storage write I/O error
 *          -4: invalid identity or field exceeds 64 bytes
 */
int save_identity(Storage* storage,
                 const std::string& key,
                 const IdentityState& identity);

// For testing only
bool parse_identity(const std::string& data, IdentityState& state);
std::string format_identity(const IdentityState& identity);

} // namespace neondoll

#endif // NEONDOLL_IDENTITY_STORAGE_HPP