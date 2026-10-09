#include "neondoll.hpp"

namespace neondoll {

static Platform* g_platform = nullptr;

class Esp32Platform : public Platform {
public:
    void init() override {}
    void deinit() override {}
    void storage_init() override {}
    void entropy_init() override {}
    void clock_init() override {}
    void network_init() override {}
    void log_init() override {}
};

static Esp32Platform default_platform;

void set_platform(Platform* platform) {
    g_platform = platform;
}

void init(void) {
    if (!g_platform) {
        g_platform = &default_platform;
    }
    if (g_platform) {
        g_platform->init();
    }
}

void deinit(void) {
    if (g_platform) {
        g_platform->deinit();
    }
}

} // namespace neondoll