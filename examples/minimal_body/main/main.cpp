#include <neondoll/platform.hpp>
#include <stdio.h>

// ESP32-specific adapter classes implementing the platform interfaces
// These are minimal stubs for M0.2; they return success for init/deinit
// and appropriate error codes for other operations to avoid silently
// pretending unsupported operations succeeded.

namespace neondoll {

class Esp32Storage : public Storage {
public:
    int init() override { return 0; }
    int deinit() override { return 0; }
    std::ptrdiff_t read(const char* key, void* out, size_t max_size) override { return -1; }
    int write(const char* key, const void* in, size_t size) override { return -1; }
};

class Esp32EntropySource : public EntropySource {
public:
    int init() override { return 0; }
    int deinit() override { return 0; }
    int get_random(void* out, size_t len) override { return -1; }
};

class Esp32Clock : public Clock {
public:
    int init() override { return 0; }
    int deinit() override { return 0; }
    uint64_t now_ms() override { return 0; }
};

class Esp32NetworkAvailability : public NetworkAvailability {
public:
    int init() override { return 0; }
    int deinit() override { return 0; }
    int set_callback(Callback cb, void* arg) override { return -1; }
};

class Esp32Logger : public Logger {
public:
    int init() override { return 0; }
    int deinit() override { return 0; }
    int log(int level, const char* tag, const char* msg) override { return -1; }
};

} // namespace neondoll

// Global platform objects for the example
static neondoll::Esp32Storage storage;
static neondoll::Esp32EntropySource entropy;
static neondoll::Esp32Clock clock;
static neondoll::Esp32NetworkAvailability network;
static neondoll::Esp32Logger logger;

extern "C" void app_main(void)
{
    printf("Calling neondoll_init...\n");

    // Set up platform context
    neondoll::PlatformContext ctx;
    ctx.storage = &storage;
    ctx.entropy = &entropy;
    ctx.clock = &clock;
    ctx.network = &network;
    ctx.logger = &logger;

    int ret = neondoll_init(&ctx);
    if (ret != 0) {
        printf("neondoll_init failed with error %d\n", ret);
        return;
    }
    printf("neondoll_init returned %d.\n", ret);

    printf("Calling neondoll_deinit...\n");
    neondoll_deinit(&ctx);
    printf("neondoll_deinit returned.\n");
}