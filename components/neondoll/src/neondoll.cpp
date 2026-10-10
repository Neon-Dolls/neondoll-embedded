// Copyright 2026 Neon-Dolls/neondoll-embedded
// SPDX-License-Identifier: Apache-2.0

#include <neondoll/platform.hpp>

namespace neondoll {

} // namespace neondoll

extern "C" {

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

} // extern "C"