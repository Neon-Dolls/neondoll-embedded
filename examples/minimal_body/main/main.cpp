#include <neondoll/platform.hpp>
#include <neondoll/esp32_platform.hpp>
#include <cstdio>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <sdkconfig.h>

static neondoll::Esp32Storage app_storage;
static neondoll::Esp32EntropySource app_entropy;
static neondoll::Esp32Clock app_clock;
static neondoll::Esp32NetworkAvailability app_network;
static neondoll::Esp32Logger app_logger;
static neondoll::PlatformContext ctx;
#ifdef CONFIG_NEONDOLL_WIFI_ENABLE
static neondoll::NetworkSupervisor app_recovery{app_network, app_clock};
#endif

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
#ifdef CONFIG_NEONDOLL_WIFI_ENABLE
    // M1 has completed. Only now may the application initialize/start RF.
    app_network.set_callback([](void*, bool available) {
        const char* state = !available ? "offline" :
            app_network.last_change() == neondoll::Esp32NetworkAvailability::Change::changed ?
            "network_changed" : "network_available";
        std::printf("NeonDoll network: %s\n", state);
    }, nullptr);
    int network_result = app_network.start(CONFIG_NEONDOLL_WIFI_SSID, CONFIG_NEONDOLL_WIFI_PASSWORD, 0);
    if (network_result == 0) network_result = app_recovery.start();
    if (network_result != 0) {
        std::printf("Wi-Fi startup failed: status %d, IDF error %d\n", network_result, app_network.last_error());
        app_network.stop();
        return;
    }
    while (true) {
        app_recovery.poll();
        vTaskDelay(pdMS_TO_TICKS(100));
    }
#endif
}
