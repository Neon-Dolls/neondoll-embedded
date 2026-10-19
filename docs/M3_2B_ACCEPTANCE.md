# M3.2b — ESP32 WireGuard backend

This implements the production adapter behind the unchanged M3.1 `WireGuardBackend`
contract. It does **not** establish full M3.2 acceptance: no physical ESP32/Linux
peer handshake or bidirectional IPv6 interoperability has been demonstrated here.

## Dependency and build

ESPHome WireGuard v0.4.8, revision
`9b7660fed27151abdcbf32b43861044cf4a32497`, plus the reproducible NeonDoll patch
series in `patches/esp_wireguard`. The second patch adds device/temporary-key and
DRBG cleanup, explicit underlay binding without nested ESP-NETIF IPC, and stale
underlay protection. Cryptographic primitive sources remain byte-identical.
The component manifest pins `esphome/libsodium` to `1.10021.1`; vendor and consumer
share four AllowedIPs slots through the vendor target's public compile definition.

With ESP-IDF **5.2.1**, classic ESP32, prepare the source using the archive/checksum
instructions in [M3.2a](M3_2A_IPV6.md), then:

```sh
idf.py -C examples/minimal_body -B /tmp/neondoll-wg-build \
  -D SDKCONFIG=/tmp/neondoll-wg-build/sdkconfig \
  -D "SDKCONFIG_DEFAULTS=$PWD/examples/minimal_body/sdkconfig.wifi-ci.defaults;$PWD/examples/minimal_body/sdkconfig.wireguard.defaults" \
  -D WIREGUARD_SOURCE_DIR=/tmp/wireguard build
```

Without `WIREGUARD_SOURCE_DIR`, the example retains its existing build. With it,
the real backend is linked and the transport is polled after M2 recovery. The
example contains no keys, endpoint or automatic tunnel startup. The application
must supply a validated RAM configuration and call transport `init`/`start`.
No provisioning service or persistent key storage is introduced.

## Ownership and behavior

One application task owns the backend and transport. Do not invoke backend methods
from TCP/IP callbacks or while holding the lwIP core lock. Construction performs
no I/O; initialize M1 first, then M2 station/IP, then the tunnel. Keep TCP/IP alive
until tunnel shutdown; call transport `poll` after recovery `poll`.

ESP-NETIF IPC resolves the station name before taking the core lock. All raw lwIP
interface, UDP, peer, timer, status and cleanup operations hold that lock, serializing
against receive/timer callbacks. Explicit vendor binding bypasses its fallback
ESP-NETIF lookup under the lock. The pinned upstream already bound its UDP PCB;
the patch now makes that binding explicit immediately after `udp_bind`, before
device publication, input registration or timers. The output guard rejects a
missing/zero-index binding.

A core-serialized extended netif callback latches underlay removal by clearing the
cached underlay and detaching the UDP receive callback. Even a replacement with
the same pointer **and** index cannot inherit the old tunnel's input/output or
readiness. Only stop/start creates and binds a new PCB. Shutdown unregisters the
removal listener before freeing any device state; partial-start failures use the
same path. No queued backend callback can outlive the object. ESP-IDF 5.2.1 enables
`LWIP_NETIF_EXT_STATUS_CALLBACK`; the backend requires it at compile time.

There is one statically owned netif slot and one peer. Repeated `start` while active,
or another owner's `start`, returns `-2` without reconfiguration. Invalid config or
missing/unsuitable station returns `-1`; partial initialization failure returns
`-3` with the underlying error in `last_error()`. `stop` is synchronous, idempotent
and returns zero: it cancels the protocol timeout and UDP receiver before removing
the netif, wiping/freeing the device and releasing the DRBG. Failure paths use the
same cleanup. Temporary encoded and decoded private keys are erased on every exit;
backend-owned device/session keys and platform RNG contexts are erased at teardown.
Caller-owned configuration remains the caller's responsibility (`clear()`). No
private keys are logged. RNG/seed failures fail closed; a runtime RNG error aborts
rather than allowing uninitialized ephemeral material into the protocol.

Wi-Fi/IP availability permits initialization; it never means authenticated readiness.
`status()` becomes `up` only after a valid received AEAD packet/keepalive recorded
by the existing backend, with an unexpired receiving key and recent authenticated
receive time. Merely creating a keypair or interface stays `starting`. Inactivity
or key expiry returns to `starting`; M3.1 owns network-loss teardown/restart. The
library retains its normal WireGuard handshake/rekey timers, without owning Wi-Fi
or adding a competing M2 reconnect policy. Explicit transport shutdown clears intent.

Tunnel addresses remain IPv6, with preferred infinite *dynamic* lifetimes and
no autoconfiguration (avoids lwIP's implicit static-address /64 route). All incoming
sources pass the patched explicit AllowedIPs checks, including partial-byte prefixes.
The outer endpoint may be numeric IPv4 or IPv6. No default interface or global
route is modified. Applications must bind tunnel traffic to the configured IPv6
source/interface; unrelated traffic must retain its normal route. DNS resolution
and scoped/link-local endpoint provisioning are outside this adapter.

## Validation and outstanding acceptance

Host suites compile the actual patched interface/crypto with lwIP 2.1.3 and libsodium;
ESP-NETIF, allocator/timer and entropy boundaries are stubbed. Tests cover ten
initialization failure points, cleanup/recovery, 1,000 lifecycle cycles, two-owner
exclusion, IPv6 addresses/prefixes, IPv4 outer endpoint, actual initial handshake
output on a selected IPv4 underlay that differs from the default, removal and all
pointer/index reuse combinations, receive detachment and restart rebinding,
M3.1 network-loss/explicit-stop behavior, authenticated receive vs tampering, and
readiness expiry across clock wrap. Separate tests exercise the real patched
platform cleanup with injected mbedTLS failures and repeated erasure. These tests
do not complete a real-peer handshake or perform a concurrency stress test.

Recorded validation for this implementation:

- **156 host tests in eight CTest suites passed**, including existing M1–M3 and
  20 patched IPv6 packet tests; the same suites passed ASan/UBSan with leak detection.
- ESP-IDF 5.2.1 classic ESP32 builds passed for default, Wi-Fi-only and production
  WireGuard configurations. Backend image: 901,600 bytes (14% app partition free).
- QEMU identity regression passed: the same Body ID across three emulated boots.
  QEMU provides no RF/tunnel interoperability evidence.
- Patch preparation verified the pinned archive and exact eight-file delta;
  cryptographic primitive sources were unchanged.

Required before full M3.2 acceptance on physical classic ESP32:

- Set an appropriate wall clock before handshakes; validate ESP entropy while Wi-Fi
  is active and the pinned mbedTLS/libsodium configuration on device.
- Exchange real handshakes and bidirectional IPv6 packets with a Linux WireGuard
  peer over an IPv4 outer endpoint; check source binding, MTU, AllowedIPs rejection,
  authenticated readiness, rekey and route isolation.
- Exercise RF outages, IP changes, repeated station destruction/recreation and
  concurrent timer/receive teardown under load; monitor heap and stack over time.
- Confirm zero retained backend secrets on all physical failure/teardown paths.

No Relay, pairing, provisioning, new cryptographic primitives or global routes.
