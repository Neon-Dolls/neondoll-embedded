#include <neondoll/platform.hpp>
#include <stdio.h>

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