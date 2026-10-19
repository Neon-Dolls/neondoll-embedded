# M3.1 — WireGuard transport foundation

Based on master `44da44a` (M2.2 including failed-shutdown cleanup protection).
Scope checked against `BUILD_PLAN.md` M3 and section 5/M3 of
`../NeonDoll_Embedded_Design_M1.2_to_M9.md` before implementation.
The design's full M3 requires an IPv6 overlay, retained on-device keys, a real
handshake, bidirectional traffic and resource measurements. This PR delivers the
requested M3.1 lifecycle/configuration slice. It does **not** complete that gate.

## Backend decision and deferred integration

No WireGuard crypto is implemented here. `WireGuardBackend` isolates the library
binding from the portable `TunnelTransport` interface and `WireGuardTransport`
lifecycle adapter. Both host CMake and the classic ESP32 ESP-IDF component compile
the same C++17 implementation. No new component dependency is downloaded.

Candidate review (2026-10-19):

| Candidate | Evidence | Decision for M3.1 |
| --- | --- | --- |
| [smartalock/wireguard-lwip](https://github.com/smartalock/wireguard-lwip) | Original lwIP C implementation; BSD-3-Clause; platform RNG/time hooks required | Reference lineage; not selected as an ESP-IDF binding |
| [trombik/esp_wireguard](https://github.com/trombik/esp_wireguard) | ESP32/ESP-NETIF integration; BSD-3-Clause; README warns about IPv6 and [receive path](https://github.com/trombik/esp_wireguard/blob/main/src/wireguardif.c) has incomplete IPv6 filtering | Cannot satisfy the design's IPv6 gate as reviewed |
| [esphome-libs/wireguard](https://github.com/esphome-libs/wireguard) | Maintained ESPHome fork; ESP-IDF component build and libsodium dependency; BSD-3-Clause; [current configuration](https://github.com/esphome-libs/wireguard/blob/dev/include/esp_wireguard.h) and [packet path](https://github.com/esphome-libs/wireguard/blob/dev/src/wireguardif.c) still need IPv6 overlay/routing review | Preferred M3.2 feasibility target; not accepted or linked yet |

`UnavailableWireGuardBackend` is the explicit production fallback: `start()`
returns `-6` (unsupported), allocates nothing and can never report `up`. Successful
configuration/init only means structurally valid configuration in RAM. A fake
backend in host tests proves lifecycle behavior, **not encrypted connectivity**.

M3.2 must pin and compile the selected backend on IDF 5.2.1/classic ESP32, verify
IPv6 output and source/destination filtering, route isolation, core-thread
locking, RNG and wall-clock requirements, asynchronous teardown, license notices,
crypto test evidence, stack/heap/flash use and repeated-cycle resource metrics.
If the preferred candidate cannot meet these, select another maintained backend;
do not substitute an IPv4-only tunnel or implement new cryptography.

## Configuration and secret lifetime

`WireGuardConfig` contains raw 32-byte private/peer public keys, a numeric IPv4 or
IPv6 UDP endpoint and port, up to two IPv6 tunnel addresses/prefixes, and up to four
IPv6 allowed-IP prefixes. Counts, key presence, port, address family, unicast
addresses, prefix lengths, duplicates and canonical route prefixes are checked.
Default routes, scoped/link-local endpoints, DNS, IPv4 overlay and zero/equal keys
are rejected. This is structural validation; cryptographic validity belongs to
the chosen backend. Application-supplied prefixes must be the assigned Doll
Network routes; there is no default-interface or general Internet routing API.

Configuration is copied into bounded RAM, never NVS, logs, Kconfig or firmware
constants. `stop()` retains the RAM configuration for explicit restart;
`deinit()` and each config copy's destructor wipe its private-key bytes using
volatile stores. The caller must also clear any external input/key buffers.
Backends must copy needed fields during `start()`, never retain the config
reference, never log private keys, and erase owned key/session copies on cleanup.
No key generation, rotation, durable enrollment, pairing or membership is added;
M1 identity and bootstrap entropy ordering remain unchanged. Persistent WG keys
remain part of the full M3 acceptance, outside this RAM-only foundation.

## Owner-task integration and lifecycle

Dependencies outlive the adapter; serialize methods/getters/destruction on one
owner task. There are no new tasks, timers, allocations, callbacks or packet
queues in this layer. Never reenter it from a backend operation.

```cpp
// Application retains the existing network availability callback.
// After successful neondoll_init(), network.start() and recovery.start():
neondoll::UnavailableWireGuardBackend backend; // Replace only after M3.2 proof.
neondoll::WireGuardTransport tunnel(network, backend);
// Populate config from application-owned RAM; check every return value.
int result = tunnel.init(config);
config.clear();
if (result == 0) result = tunnel.start();
// In the existing application tick (no additional polling task):
recovery.poll(); // Drains IDF events and applies M2 network policy first.
tunnel.poll();   // Handles loss/change and tunnel work using that snapshot.
// On application shutdown: cancel tunnel intent before stopping Wi-Fi.
tunnel.stop();   // Retry failures before destroying backend/dependencies.
recovery.stop();
```

The fallback's first backend attempt returns unsupported, as intended. The
minimal firmware retains its M1/M2 behavior and does not inject example keys or
claim a tunnel. Backend substitution does not change M2 or consume its callback.
Availability handling is bounded by the application's existing poll cadence.

- `init()` validates/copies without network I/O. Duplicate init returns `-2`.
- `start()` idempotently arms intent. Offline means `waiting_network`. It performs
  no synchronous connection; the next available poll calls backend start once.
- Backend start success means `starting`; only backend-observed authenticated
  peer activity may subsequently report `up`.
- Wi-Fi loss, driver stop or a changed acquisition epoch stops the old backend
  before another start. Epoch comparison also catches a coalesced loss/reacquire.
  Wi-Fi recovery may resume an armed tunnel; it never starts the station itself.
- `stop()` cancels intent **before** cleanup. Repeated stop is safe. Failed cleanup
  retains ownership, reports `cleanup_pending` and blocks start/init. Each poll
  can retry cleanup once; a successful cleanup never starts in the same poll.
- Partial backend start failure or unexpected backend down/failure cancels intent
  and schedules cleanup. Further attempts require an explicit `start()` after
  cleanup. No separate tunnel backoff/retry/session policy is introduced.
- `deinit()` cancels intent and wipes local config even if cleanup fails. Retain
  objects and retry cleanup before destruction; backend ownership includes any
  residual asynchronous handles. Destructor cleanup is best effort, not a way
  to discard a failed teardown obligation.

Return values: `0` accepted/waiting/success (inspect status), `-1` invalid config or
uninitialized start, `-2` duplicate init/cleanup pending, `-3` backend down/failure,
`-6` backend unavailable, otherwise backend action error. `last_error()` retains
failure evidence through cleanup until successful init/explicit start.

## Validation and outstanding acceptance

Host tests cover configuration bounds, IPv4/IPv6 endpoints, route validation,
IP acquisition/loss/change, coalesced epochs, idempotency, partial failures,
failed cleanup/recovery, explicit shutdown, destructor cleanup and 1,000 cycles.
An ESP-IDF-stub integration test drives the actual M2 adapter and supervisor,
preserves callback order and verifies a stopped tunnel stays stopped after Wi-Fi
restart. Existing M1/M2 tests remain enabled. CTest includes the new suite;
ASan/UBSan and both ESP-IDF 5.2.1 firmware configurations are required checks.

Local validation: all 116 host tests passed (21 new portable transport tests and
one new ESP-IDF-stub integration test), including ASan/UBSan. Classic ESP32
default and Wi-Fi-enabled builds passed in `espressif/idf:v5.2.1`. The existing
QEMU identity regression is run separately and is only an M1 regression check;
it provides no Wi-Fi or WireGuard proof.

Still outstanding for M3.2/full M3: linked backend, real ESP32/Core handshake,
bidirectional IPv6 packets, route-leak tests, AP outage/reacquisition, repeated
physical teardown with heap/stack/socket/netif measurements, and durable WG key
provisioning/reboot proof. No hardware or encrypted connectivity claim is made.
