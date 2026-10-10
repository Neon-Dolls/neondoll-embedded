#include <neondoll/platform.hpp>
#include <neondoll/esp32_platform.hpp>
#include <stdio.h>

// ESP32-specific adapter classes are now in esp32_platform.hpp
// These are minimal stubs for M0.2; they return success for init/deinit
// and appropriate error codes for other operations to avoid silently
// pretending unsupported operations succeeded.

// Global platform objects for the example
static neondoll::Esp32Storage app_storage;
static neondoll::Esp32EntropySource app_entropy;
static neondoll::Esp32Clock app_clock;
static neondoll::Esp32NetworkAvailability app_network;
static neondoll::Esp32Logger app_logger;

extern "C" void app_main(void)
{
    printf("Calling neondoll_init...\\n");

    // Set up platform context
    neondoll::PlatformContext ctx;
    ctx.storage = &app_storage;
    ctx.entropy = &app_entropy;
    ctx.clock = &app_clock;
    ctx.network = &app_network;
    ctx.logger = &app_logger;

    int ret = neondoll_init(&ctx);
    if (ret != 0) {
        printf("neondoll_init failed with error %d\\n", ret);
        return;
    }
    printf("neondoll_init returned %d.\\n", ret);

    printf("Calling neondoll_deinit...\\n");
    neondoll_deinit(&ctx);
    printf("neondoll_deinit returned.\\n");
}