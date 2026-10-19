// Copyright 2026 Neon-Dolls/neondoll-embedded
// SPDX-License-Identifier: Apache-2.0
#ifndef NEONDOLL_ESP32_WIREGUARD_HPP
#define NEONDOLL_ESP32_WIREGUARD_HPP
#include <neondoll/wireguard_transport.hpp>
namespace neondoll {
/** One peer/one active instance. Enable with the verified WIREGUARD_SOURCE_DIR.
 * Single application owner task; never call from TCP/IP callbacks or while holding
 * its core lock. TCP/IP must outlive this object; station removal invalidates
 * its binding until restart. Construction does no I/O; start after M1 and station/IP
 * startup. Poll the M3.1 transport after M2 recovery.poll to handle loss/recovery.
 * No DNS, station management, default routes or application reconnect timers.
 */
class Esp32WireGuardBackend final : public WireGuardBackend {
public:
    Esp32WireGuardBackend() = default;
    ~Esp32WireGuardBackend() override;
    Esp32WireGuardBackend(const Esp32WireGuardBackend&) = delete;
    Esp32WireGuardBackend& operator=(const Esp32WireGuardBackend&) = delete;
    int start(const WireGuardConfig& config) override;
    int stop() override;
    State status() const override;
    int last_error() const { return error_; }
private:
    struct Impl;
    static Impl slot_;
    Impl* impl_ = nullptr;
    int error_ = 0;
    void release_locked();
};
}
#endif
