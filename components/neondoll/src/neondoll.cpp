#include <neondoll/platform.hpp>

namespace neondoll {

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

// Global pointers to the platform objects, initialized to the production implementations.
// NOTE: These are only for backward compatibility with the old API.
// New code should use neondoll_init with explicit PlatformContext.
namespace neondoll {
static Esp32Storage storage_impl;
static Esp32EntropySource entropy_impl;
static Esp32Clock clock_impl;
static Esp32NetworkAvailability network_impl;
static Esp32Logger logger_impl;

static Storage* g_storage = &storage_impl;
static EntropySource* g_entropy = &entropy_impl;
static Clock* g_clock = &clock_impl;
static NetworkAvailability* g_network = &network_impl;
static Logger* g_logger = &logger_impl;
} // namespace neondoll

// Backward compatibility function (to be removed in future versions)
extern "C" void neondoll_set_platform(neondoll::Storage* storage, neondoll::EntropySource* entropy, neondoll::Clock* clock, neondoll::NetworkAvailability* network, neondoll::Logger* logger) {
    if (storage) neondoll::g_storage = storage;
    if (entropy) neondoll::g_entropy = entropy;
    if (clock) neondoll::g_clock = clock;
    if (network) neondoll::g_network = network;
    if (logger) neondoll::g_logger = logger;
}

extern "C" {

int neondoll_init(const neondoll::PlatformContext* ctx) {
    // Use provided context or fall back to global pointers for backward compatibility
    neondoll::Storage* storage = ctx ? ctx->storage : neondoll::g_storage;
    neondoll::EntropySource* entropy = ctx ? ctx->entropy : neondoll::g_entropy;
    neondoll::Clock* clock = ctx ? ctx->clock : neondoll::g_clock;
    neondoll::NetworkAvailability* network = ctx ? ctx->network : neondoll::g_network;
    neondoll::Logger* logger = ctx ? ctx->logger : neondoll::g_logger;
    
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
    
    return 0; // Success
}

void neondoll_deinit(const neondoll::PlatformContext* ctx) {
    // Use provided context or fall back to global pointers for backward compatibility
    neondoll::Storage* storage = ctx ? ctx->storage : neondoll::g_storage;
    neondoll::EntropySource* entropy = ctx ? ctx->entropy : neondoll::g_entropy;
    neondoll::Clock* clock = ctx ? ctx->clock : neondoll::g_clock;
    neondoll::NetworkAvailability* network = ctx ? ctx->network : neondoll::g_network;
    neondoll::Logger* logger = ctx ? ctx->logger : neondoll::g_logger;
    
    // Deinitialize in reverse order
    if (logger) logger->deinit();
    if (network) network->deinit();
    if (clock) clock->deinit();
    if (entropy) entropy->deinit();
    if (storage) storage->deinit();
}

} // extern "C"