#ifndef NEONDOLL_ESP32_PLATFORM_HPP
#define NEONDOLL_ESP32_PLATFORM_HPP

#include <neondoll/platform.hpp>

namespace neondoll {

/** Minimal ESP32 storage adapter for M0.2. */
class Esp32Storage : public Storage {
public:
    int init() override { return 0; }
    int deinit() override { return 0; }
    std::ptrdiff_t read(const char* key, void* out, size_t max_size) override { return -1; } // not implemented
    int write(const char* key, const void* in, size_t size) override { return -1; } // not implemented
};

/** Minimal ESP32 entropy source adapter for M0.2. */
class Esp32EntropySource : public EntropySource {
public:
    int init() override { return 0; }
    int deinit() override { return 0; }
    int get_random(void* out, size_t len) override { return -1; } // not implemented
};

/** Minimal ESP32 clock adapter for M0.2. */
class Esp32Clock : public Clock {
public:
    int init() override { return 0; }
    int deinit() override { return 0; }
    uint64_t now_ms() override { return 0; } // not implemented
};

/** Minimal ESP32 network availability adapter for M0.2. */
class Esp32NetworkAvailability : public NetworkAvailability {
public:
    int init() override { return 0; }
    int deinit() override { return 0; }
    int set_callback(Callback cb, void* arg) override { return -1; } // not implemented
};

/** Minimal ESP32 logger adapter for M0.2. */
class Esp32Logger : public Logger {
public:
    int init() override { return 0; }
    int deinit() override { return 0; }
    int log(int level, const char* tag, const char* msg) override { return -1; } // discard
};

} // namespace neondoll

#endif // NEONDOLL_ESP32_PLATFORM_HPP