#include <neondoll/platform.hpp>
#include <neondoll/esp32_platform.hpp>
#include <cstdio>

static neondoll::Esp32Storage app_storage;
static neondoll::Esp32EntropySource app_entropy;
static neondoll::Esp32Clock app_clock;
static neondoll::Esp32NetworkAvailability app_network;
static neondoll::Esp32Logger app_logger;
static neondoll::PlatformContext ctx;

extern "C" void app_main(void) {
    // Bootstrap entropy must run before application Wi-Fi/ADC/I2S startup.
    ctx.storage = &app_storage;
    ctx.entropy = &app_entropy;
    ctx.clock = &app_clock;
    ctx.network = &app_network;
    ctx.logger = &app_logger;
    const int ret = neondoll_init(&ctx);
    if (ret != 0) {
        std::printf("NeonDoll initialization failed: %d; durable state preserved\n", ret);
        return;
    }
    const auto* identity = neondoll_get_identity(&ctx);
    std::printf("NeonDoll Body ID: %s\n", identity->identity.body_id.c_str());
    std::printf("Identity schema: %d; identity ready (network not started)\n", identity->version);
}
