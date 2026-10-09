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
    int log(int level, const char* tag, const char* format, ...) override { return 0; } // discard
};

// Global pointers to the platform objects, initialized to the production implementations.
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

// Allow test to inject mocks.
extern "C" void neondoll_set_platform(Storage* storage, EntropySource* entropy, Clock* clock, NetworkAvailability* network, Logger* logger) {
    if (storage) g_storage = storage;
    if (entropy) g_entropy = entropy;
    if (clock) g_clock = clock;
    if (network) g_network = network;
    if (logger) g_logger = logger;
}

extern "C" {

void neondoll_init(void) {
    g_storage->init();
    g_entropy->init();
    g_clock->init();
    g_network->init();
    g_logger->init();
}

void neondoll_deinit(void) {
    g_logger->deinit();
    g_network->deinit();
    g_clock->deinit();
    g_entropy->deinit();
    g_storage->deinit();
}

} // extern "C"

} // namespace neondoll