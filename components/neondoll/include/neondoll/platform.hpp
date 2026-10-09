#ifndef NEONDOLL_PLATFORM_HPP
#define NEONDOLL_PLATFORM_HPP

#include <cstddef>
#include <cstdint>

namespace neondoll {

/**
 * @brief Persistent key/value storage interface.
 */
class Storage {
public:
    virtual ~Storage() = default;

    /// Initialize the storage backend.
    /// @return 0 on success, negative error code on failure.
    virtual int init() = 0;

    /// Deinitialize the storage backend.
    /// @return 0 on success, negative error code on failure.
    virtual int deinit() = 0;

    /// Read a value by key.
    /// @param key      Null-terminated key string.
    /// @param[out] out Buffer to store the value.
    /// @param[in]  max_size Maximum number of bytes to read (including null terminator).
    /// @return Number of bytes stored in @p out (excluding null terminator) on success,
    ///         negative error code on failure.
    virtual std::ptrdiff_t read(const char* key, void* out, size_t max_size) = 0;

    /// Write a value by key.
    /// @param key   Null-terminated key string.
    /// @param[in] in  Buffer containing the value to write.
    /// @param[in] size Number of bytes in @p in.
    /// @return 0 on success, negative error code on failure.
    virtual int write(const char* key, const void* in, size_t size) = 0;
};

/**
 * @brief Cryptographic entropy source interface.
 */
class EntropySource {
public:
    virtual ~EntropySource() = default;

    /// Initialize the entropy source.
    /// @return 0 on success, negative error code on failure.
    virtual int init() = 0;

    /// Deinitialize the entropy source.
    /// @return 0 on success, negative error code on failure.
    virtual int deinit() = 0;

    /// Obtain random bytes.
    /// @param[out] out Buffer to fill with random data.
    /// @param[in]  len Number of random bytes to generate.
    /// @return 0 on success, negative error code on failure.
    virtual int get_random(void* out, size_t len) = 0;
};

/**
 * @brief Monotonic clock interface.
 */
class Clock {
public:
    virtual ~Clock() = default;

    /// Initialize the clock.
    /// @return 0 on success, negative error code on failure.
    virtual int init() = 0;

    /// Deinitialize the clock.
    /// @return 0 on success, negative error code on failure.
    virtual int deinit() = 0;

    /// Get the current monotonic timestamp in milliseconds.
    /// @return Timestamp in milliseconds.
    virtual uint64_t now_ms() = 0;
};

/**
 * @brief Network availability notification interface.
 */
class NetworkAvailability {
public:
    virtual ~NetworkAvailability() = default;

    /// Initialize the network availability monitor.
    /// @return 0 on success, negative error code on failure.
    virtual int init() = 0;

    /// Deinitialize the network availability monitor.
    /// @return 0 on success, negative error code on failure.
    virtual int deinit() = 0;

    /// Register a callback to be invoked on network state changes.
    /// @param cb   Function pointer to call on state change.
    /// @param arg  User argument to pass to the callback.
    /// @return 0 on success, negative error code on failure.
    using Callback = void (*)(void* arg, bool available);
    virtual int set_callback(Callback cb, void* arg) = 0;
};

/**
 * @brief Logging interface.
 */
class Logger {
public:
    virtual ~Logger() = default;

    /// Initialize the logger.
    /// @return 0 on success, negative error code on failure.
    virtual int init() = 0;

    /// Deinitialize the logger.
    /// @return 0 on success, negative error code on failure.
    virtual int deinit() = 0;

    /// Log a message at the given level.
    /// @param level   Logging level (higher is more severe).
    /// @param tag     Null-terminated tag string.
    /// @param format  printf-style format string.
    /// @param ...     Arguments for the format string.
    /// @return 0 on success, negative error code on failure.
    virtual int log(int level, const char* tag, const char* format, ...) = 0;
};

} // namespace neondoll

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Inject mock platform objects for testing.
 *        This function is only intended for use in host tests.
 *        In production, the platform objects are the ESP32-specific implementations.
 * @param storage   Pointer to a Storage object (or nullptr to use default).
 * @param entropy   Pointer to an EntropySource object (or nullptr to use default).
 * @param clock     Pointer to a Clock object (or nullptr to use default).
 * @param network   Pointer to a NetworkAvailability object (or nullptr to use default).
 * @param logger    Pointer to a Logger object (or nullptr to use default).
 */
void neondoll_set_platform(neondoll::Storage* storage,
                           neondoll::EntropySource* entropy,
                           neondoll::Clock* clock,
                           neondoll::NetworkAvailability* network,
                           neondoll::Logger* logger);

/**
 * @brief Initialize the NeonDoll runtime.
 *        This calls init() on all platform objects.
 */
void neondoll_init(void);

/**
 * @brief Deinitialize the NeonDoll runtime.
 *        This calls deinit() on all platform objects.
 */
void neondoll_deinit(void);

#ifdef __cplusplus
}
#endif

#endif // NEONDOLL_PLATFORM_HPP