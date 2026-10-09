#include <neondoll/platform.hpp>

namespace neondoll {

bool s_initialized = false;

// Production implementations for ESP32
class Esp32Storage : public Storage {
public:
    int init() override { return 0; }
    int deinit() override { return 0; }
    std::ptrdiff_t read(const char* key, void* out, size_t max_size) override { return -1; } // not implemented
    int write(const char* key, const void* in, size_t size) override { return -1; } // not implemented
};

class Esp32EntropySource : public EntropySource {
public:
    int init() override { return 0; }
    int deinit() override { return 0; }
    int get_random(void* out, size_t len) override { return -1; } // not implemented
};

class Esp32Clock : public Clock {
public:
    int init() override { return 0; }
    int deinit() override { return 0; }
    uint64_t now_ms() override { return 0; } // not implemented
};

class Esp32NetworkAvailability : public NetworkAvailability {
public:
    int init() override { return 0; }
    int deinit() override { return 0; }
    int set_callback(Callback cb, void* arg) override { return -1; } // not implemented
};

class Esp32Logger : public Logger {
public:
    int init() override { return 0; }
    int deinit() override { return 0; }
    int log(int level, const char* tag, const char* msg) override { return 0; } // discard
};

} // namespace neondoll

extern "C" {

int neondoll_init(const neondoll::PlatformContext* ctx) {
    // Validate context
    if (!ctx) {
        return -1; // Invalid argument
    }

    // Prevent double initialization
    if (neondoll::s_initialized) {
        return -1; // Already initialized
    }

    // Extract pointers from context
    neondoll::Storage* storage = ctx->storage;
    neondoll::EntropySource* entropy = ctx->entropy;
    neondoll::Clock* clock = ctx->clock;
    neondoll::NetworkAvailability* network = ctx->network;
    neondoll::Logger* logger = ctx->logger;

    // Track initialization status for cleanup on failure
    bool storage_init = false;
    bool entropy_init = false;
    bool clock_init = false;
    bool network_init = false;
    bool logger_init = false;

    int ret;

    // Initialize storage
    if (storage) {
        ret = storage->init();
        if (ret != 0) {
            return ret;
        }
        storage_init = true;
    }

    // Initialize entropy
    if (entropy) {
        ret = entropy->init();
        if (ret != 0) {
            if (storage_init) storage->deinit();
            return ret;
        }
        entropy_init = true;
    }

    // Initialize clock
    if (clock) {
        ret = clock->init();
        if (ret != 0) {
            if (entropy_init) entropy->deinit();
            if (storage_init) storage->deinit();
            return ret;
        }
        clock_init = true;
    }

    // Initialize network
    if (network) {
        ret = network->init();
        if (ret != 0) {
            if (clock_init) clock->deinit();
            if (entropy_init) entropy->deinit();
            if (storage_init) storage->deinit();
            return ret;
        }
        network_init = true;
    }

    // Initialize logger
    if (logger) {
        ret = logger->init();
        if (ret != 0) {
            if (network_init) network->deinit();
            if (clock_init) clock->deinit();
            if (entropy_init) entropy->deinit();
            if (storage_init) storage->deinit();
            return ret;
        }
        logger_init = true;
    }

    // Mark as initialized only after all succeeded
    neondoll::s_initialized = true;
    return 0; // Success
}

void neondoll_deinit(const neondoll::PlatformContext* ctx) {
    if (!ctx) {
        return;
    }

    // Extract pointers from context
    neondoll::Storage* storage = ctx->storage;
    neondoll::EntropySource* entropy = ctx->entropy;
    neondoll::Clock* clock = ctx->clock;
    neondoll::NetworkAvailability* network = ctx->network;
    neondoll::Logger* logger = ctx->logger;

    // Deinitialize in reverse order
    if (logger) logger->deinit();
    if (network) network->deinit();
    if (clock) clock->deinit();
    if (entropy) entropy->deinit();
    if (storage) storage->deinit();

    // Mark as uninitialized
    neondoll::s_initialized = false;
}

} // extern "C"