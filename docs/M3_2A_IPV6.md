# M3.2a — IPv6 WireGuard network interface patch set

Base: master `100d8b1`. Read M3.1 acceptance, the unchanged `WireGuardBackend`
contract, and the embedded design M3 gate before implementation. The M3.2
[feasibility report from PR #11](https://github.com/Neon-Dolls/neondoll-embedded/blob/a0adcd91e4837597dbab8909daf2e5b94ecc89c4/docs/M3_2_FEASIBILITY.md)
was reviewed from that commit because it was not yet on master at branch creation.

This is a separately maintained network-interface patch, **not M3.2 completion**.
No production backend is connected, no keys are provisioned, and no cryptographic
primitive or M3.1 interface changes. Existing M1/M2/transport production code stays
unchanged. The available production backend still reports unsupported.

## Upstream and patch boundary

- ESPHome WireGuard **v0.4.8**, commit
  `9b7660fed27151abdcbf32b43861044cf4a32497`, BSD-3-Clause.
- Git archive SHA-256:
  `1c0839424dd01c7a89e51242f1377c0b7b683338c40303c0cb93f06815d3bc6a`.
- ESP-IDF probe pins `esphome/libsodium` **1.10021.1**; host tests link the real
  distribution libsodium and use actual lwIP **2.1.3** buffers/address routines.
- `patches/esp_wireguard/series` lists the patch order. The only changed upstream
  files are `include/esp_wireguard.h`, `src/esp_wireguard.c`,
  `src/wireguardif.c`, and new `src/neondoll_ip6.h`.
- `prepare.py` rejects the wrong archive and nonempty destinations, checks patch
  application, and compares the complete extracted tree against the archive.
  Changes outside those four files, including crypto changes/deletions, fail.
  No moving branch is fetched by the build; downloading dependencies is explicit.

## Network behavior and compatibility

IPv6 interface creation passes zero IPv4 address/netmask/gateway to lwIP, then
assigns the configured IPv6 address as preferred with infinite **dynamic**
lifetimes. This avoids lwIP's implicit static `/64` on-link route. The address is
an exact local assignment; the netmask is validated but not installed as a global
route. IPv6 autoconfiguration is disabled on this interface. Builds require
`LWIP_IPV6_ADDRESS_LIFETIMES` to preserve that routing boundary.

The future backend/application must explicitly bind tunnel egress to the tunnel
netif/source, or provide separately reviewed narrow route selection. This patch
does not install a default route, router advertisement, global route hook, or OS
network interface. Upstream `set_default` and `restore_default` return
`ESP_ERR_INVALID_STATE`; disconnect no longer restores an old default interface.
This is an intentional upstream API behavior change in the isolated patch set.
The host network and Wi-Fi underlay default are never changed by these calls.

The upstream C configuration gains `allowed_ip` and `allowed_ip_mask` for the
first explicit peer prefix, independent of local `address`/`netmask`. Both are
required for IPv6 overlay creation. Masks must be contiguous, same-family and
nonzero. Rebuild all upstream C API consumers with the patched header and
initialize configs with `ESP_WIREGUARD_CONFIG_DEFAULT()` (or zero-initialize);
the extended C struct is not binary-compatible with the unpatched library.
IPv4-only legacy configuration still gets its prior `/32` peer default;
there is no automatic substitution for an IPv6 request. Additional explicit
prefixes use the existing `esp_wireguard_add_allowed_ip()` API, with the same
mask checks. The host tests and IDF probe configure `CONFIG_WIREGUARD_MAX_SRC_IPS=4`;
upstream defaults to one slot unless the consuming project sets that macro.
One peer is in scope; multi-peer longest-prefix ownership is not claimed.

`netif->output_ip6` validates the IPv6 fixed header and total size, checks the
supplied destination, enforces MTU before encryption allocation, and selects the
peer using destination AllowedIPs. Prefix comparison uses all 128 network-order
bits and supports non-byte-aligned masks. Existing UDP binding/send code is
unchanged, preserving IPv4 **outer endpoints** with IPv6 **inner packets**.

On receive, authenticated plaintext must have a complete version-6, 40-byte base
header. Total length is `40 + payload_length`; overflow, truncation, excess
padding and unsupported jumbograms are rejected. Up to 15 bytes of WireGuard
padding are removed before delivery. A declared empty payload is supported;
the maximum non-jumbo payload is 65,535 bytes, plus the 40-byte base header.
The helper validates that protocol length separately from the caller's lwIP
16-bit buffer capacity and tunnel MTU; those transport limits still apply.
IPv6 extension/transport header semantics remain the responsibility of lwIP's
IP input path, not a new protocol stack here. Peer AllowedIPs match the inner
**source**, not its destination. Shared IPv4 receive handling also checks source
rather than retaining upstream's destination-address filtering mistake.
Replay and authentication use unchanged upstream code. No new crypto is written.

## Reproduction and tests

Requires Git, Python 3, curl, CMake, a C/C++ compiler, GTest and the host libsodium
library (e.g. `libgtest-dev libsodium-dev` on Ubuntu). Prepare outside the repo:

```sh
curl -fL https://codeload.github.com/esphome-libs/wireguard/tar.gz/9b7660fed27151abdcbf32b43861044cf4a32497 -o /tmp/wireguard.tar.gz
python3 patches/esp_wireguard/prepare.py /tmp/wireguard.tar.gz /tmp/wireguard
curl -fL https://codeload.github.com/lwip-tcpip/lwip/tar.gz/refs/tags/STABLE-2_1_3_RELEASE -o /tmp/lwip.tar.gz
echo '6add930f7c0c7a8adbe3fef393ecf47ce32ed8dadd00d23042ff61db05b194ef  /tmp/lwip.tar.gz' | sha256sum -c -
mkdir /tmp/lwip
tar -xzf /tmp/lwip.tar.gz -C /tmp/lwip --strip-components=1
cmake -S . -B /tmp/neondoll-wg-host -DWIREGUARD_SOURCE_DIR=/tmp/wireguard -DLWIP_SOURCE_DIR=/tmp/lwip
cmake --build /tmp/neondoll-wg-host -j4
ctest --test-dir /tmp/neondoll-wg-host --output-on-failure
```

The packet harness compiles the actual patched interface/API C files together
with unchanged upstream WireGuard/AEAD code, real libsodium, and actual lwIP pbufs.
UDP/netif/clock/RNG delivery is simulated. Tests use synthetic session keys rather
than a handshake. The tiny sodium test header declares the real library ABI; it
is not a crypto stub. No test key is included in the production firmware.

Local results: **136 tests passed** (116 existing + 20 packet/header tests). Cases cover
actual interface setup, chained transmit buffers, IPv4 endpoint selection,
AEAD receive and ciphertext loopback, tampered authentication/replay, source vs
destination filtering, every prefix length `/1` through `/128`, invalid masks,
truncated fixed headers/payloads, unsupported jumbograms, padding, MTU bounds,
and repeated create/cleanup. ASan/UBSan checks use both C and C++ instrumentation:

```sh
cmake -S . -B /tmp/neondoll-wg-asan -DWIREGUARD_SOURCE_DIR=/tmp/wireguard -DLWIP_SOURCE_DIR=/tmp/lwip \
  -DCMAKE_C_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' \
  -DCMAKE_CXX_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer'
cmake --build /tmp/neondoll-wg-asan -j4
UBSAN_OPTIONS=halt_on_error=1 ASAN_OPTIONS=detect_leaks=1 ctest --test-dir /tmp/neondoll-wg-asan --output-on-failure
```

With ESP-IDF **5.2.1** exported, build the isolated classic ESP32 compile probe:

```sh
mkdir /tmp/wg-ipv6-probe
cp -a integration/wireguard_ipv6/esp32/. /tmp/wg-ipv6-probe/
idf.py -C /tmp/wg-ipv6-probe -B /tmp/wg-ipv6-build -D WIREGUARD_SOURCE_DIR=/tmp/wireguard build
```

The probe links real upstream lifecycle/status symbols but starts no network.
Default and Wi-Fi-enabled production firmware also build. CI runs the patched
packet suite and compile probe in addition to existing checks. Without the two
source-directory options, the ordinary host build has no external dependency.

## Actual interoperability evidence and outstanding blockers

**No Linux-peer or physical ESP32 IPv6 interoperability demonstrated.** The
observed IPv6 evidence is a host ciphertext round trip through patched output,
unchanged AEAD, and the actual UDP receive dispatcher, using a simulated underlay
and pre-established keys. This is not bidirectional traffic with an independent
WireGuard peer. The session has no physical ESP32/AP, no effective network-admin
capability, and no `/dev/net/tun`; host networking was not modified.

M3.2 still needs a real `WireGuardBackend`, serialized lwIP core integration,
review of asynchronous ownership/DNS/timers/RNG/error handling, backend-owned key
wiping on all teardown/failure paths (the M3.2 key-erasure blocker remains), and
narrow socket/route wiring. This patch does not prove extension-header behavior
in a real IP stack, hardware resource bounds, or cleanup under physical outages.
Before completion, demonstrate a real handshake, bidirectional IPv6 packets
against Linux/Core over Wi-Fi with IPv4 UDP, route isolation, repeated outage/
restart cleanup and heap/stack/netif/socket stability. No Relay, provisioning or
pairing is added.

M3.2b extends the prepared patch series with platform/lifetime/build hardening; see [M3.2b](M3_2B_ACCEPTANCE.md). The M3.2a evidence above does not claim physical interoperability.
