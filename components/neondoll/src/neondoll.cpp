#include "neondoll.hpp"

namespace neondoll {

static Platform* g_platform = nullptr;

void set_platform(Platform* platform) {
    g_platform = platform;
}

void init(void) {
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