// Copyright 2026 Neon-Dolls/neondoll-embedded
// SPDX-License-Identifier: Apache-2.0

#include <neondoll/platform.hpp>
#include <neondoll/identity.hpp>
#include <neondoll/identity_storage.hpp>

int neondoll_init(neondoll::PlatformContext* ctx) {
    if (!ctx) {
        return -1; // null context
    }

    if (!ctx->storage || !ctx->entropy || !ctx->clock || !ctx->network || !ctx->logger) {
        return -2; // null platform pointer
    }

    if (ctx->initialized) {
        return -3; // already initialized
    }

    // Initialize platforms in order, cleaning up on failure
    int ret = ctx->storage->init();
    if (ret != 0) {
        return -4; // storage init failed
    }

    ret = ctx->entropy->init();
    if (ret != 0) {
        ctx->storage->deinit();
        return -5; // entropy init failed
    }

    ret = ctx->clock->init();
    if (ret != 0) {
        ctx->entropy->deinit();
        ctx->storage->deinit();
        return -6; // clock init failed
    }

    ret = ctx->network->init();
    if (ret != 0) {
        ctx->clock->deinit();
        ctx->entropy->deinit();
        ctx->storage->deinit();
        return -7; // network init failed
    }

    ret = ctx->logger->init();
    if (ret != 0) {
        ctx->network->deinit();
        ctx->clock->deinit();
        ctx->entropy->deinit();
        ctx->storage->deinit();
        return -8; // logger init failed
    }

    // Initialize identity after basic platforms are up
    // Using a fixed storage key for identity - in a real system this might be configurable
    constexpr const char* IDENTITY_STORAGE_KEY = "neondoll/identity";

    // Use the identity_state in the PlatformContext for persistence across init/deinit
    ret = neondoll::load_identity(ctx->storage, IDENTITY_STORAGE_KEY, ctx->identity_state);
    if (ret == 0) {
        // Upgrade the legacy envelope without generating a new Body ID.
        if (ctx->identity_state.version == 1) {
            ctx->identity_state.version = neondoll::CURRENT_IDENTITY_VERSION;
            if (neondoll::save_identity(ctx->storage, IDENTITY_STORAGE_KEY, ctx->identity_state) != 0) {
                ctx->logger->deinit(); ctx->network->deinit(); ctx->clock->deinit();
                ctx->entropy->deinit(); ctx->storage->deinit();
                return -10;
            }
        }
        // Successfully loaded existing identity - validation passed
        // Identity is now ready for use by the application
    } else if (ret == -3) {
        // Key not found - first boot, create new identity
        neondoll::BodyMetadata meta{"neondoll-embedded", "", ""};
#ifdef ESP_PLATFORM
        meta.platform = "esp32";
#if defined(__XTENSA__)
        meta.arch = "xtensa";
#elif defined(__riscv)
        meta.arch = "riscv";
#endif
#endif

        ret = neondoll::new_identity_state(ctx->entropy, "", meta, ctx->identity_state);
        if (ret != 0) {
            // Clean up initialized platforms on failure
            ctx->logger->deinit();
            ctx->network->deinit();
            ctx->clock->deinit();
            ctx->entropy->deinit();
            ctx->storage->deinit();
            return -10; // failed to create identity
        }

        // Persist the new identity
        ret = neondoll::save_identity(ctx->storage, IDENTITY_STORAGE_KEY, ctx->identity_state);
        if (ret != 0) {
            // Clean up initialized platforms on failure
            ctx->logger->deinit();
            ctx->network->deinit();
            ctx->clock->deinit();
            ctx->entropy->deinit();
            ctx->storage->deinit();
            return -10; // failed to save identity
        }
        // Identity created and persisted successfully
    } else {
        // Failed to load identity due to corruption, version mismatch, or I/O error
        // Reject without generating a replacement (per M1.1 requirement)
        // Clean up initialized platforms
        ctx->logger->deinit();
        ctx->network->deinit();
        ctx->clock->deinit();
        ctx->entropy->deinit();
        ctx->storage->deinit();
        // Propagate the error from load_identity
        return -10;  // failed to load identity (corrupt, version mismatch, or I/O error)
    }
    ctx->initialized = true;
    return 0;
}

int neondoll_deinit(neondoll::PlatformContext* ctx) {
    if (!ctx) {
        return -1; // null context
    }

    if (!ctx->initialized) {
        return -2; // not initialized
    }

    // Deinitialize in reverse order
    int ret = ctx->logger->deinit();
    if (ret != 0) {
        // Continue deinitializing others even if one fails
    }

    ret = ctx->network->deinit();
    if (ret != 0) {
        // Continue
    }

    ret = ctx->clock->deinit();
    if (ret != 0) {
        // Continue
    }

    ret = ctx->entropy->deinit();
    if (ret != 0) {
        // Continue
    }

    ret = ctx->storage->deinit();
    if (ret != 0) {
        // Continue
    }

    ctx->initialized = false;
    return 0;
}

const neondoll::IdentityState* neondoll_get_identity(const neondoll::PlatformContext* ctx) {
    if (!ctx || !ctx->initialized) {
        return nullptr;
    }
    return &ctx->identity_state;
}
