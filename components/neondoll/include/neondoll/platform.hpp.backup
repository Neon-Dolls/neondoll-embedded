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
 *        Replaced variadic API with bounded, non-variadic message interface.
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
    /// @param msg     Null-terminated message string (max 255 bytes including null).
    /// @return 0 on success, negative error code on failure.
    virtual int log(int level, const char* tag, const char* msg) = 0;
};

} // namespace neondoll

// Runtime context structure - replaces global pointers with explicit ownership
namespace neondoll {

/**
 * @brief Runtime context holding platform dependencies.
 *        Owned by the caller, lifetime must exceed neondoll_init/deinit calls.
 */
struct PlatformContext {
    Storage*       storage       = nullptr;  ///< Not owned by NeonDoll
    EntropySource* entropy       = nullptr;  ///< Not owned by NeonDoll
    Clock*         clock         = nullptr;  ///< Not owned by NeonDoll
    NetworkAvailability* network = nullptr;  ///< Not owned by NeonDoll
    Logger*        logger        = nullptr;  ///< Not owned by NeonDoll
};

} // namespace neondoll

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the NeonDoll runtime with explicit platform dependencies.
 *        Caller owns the PlatformContext and platform objects.
 * @param ctx   Platform context containing non-owning pointers to platform objects.
 *              Must remain valid for the lifetime of the NeonDoll runtime.
 * @return 0 on success, negative error code on failure.
 *         On failure, any successfully initialized platforms are cleaned up.
 */
int neondoll_init(const neondoll::PlatformContext* ctx);

/**
 * @brief Deinitialize the NeonDoll runtime.
 *        Caller must ensure no further NeonDoll API calls are made after this.
 * @param ctx   Platform context passed to neondoll_init (must match).
 */
void neondoll_deinit(const neondoll::PlatformContext* ctx);

#ifdef __cplusplus
}
#endif

#endif // NEONDOLL_PLATFORM_HPP