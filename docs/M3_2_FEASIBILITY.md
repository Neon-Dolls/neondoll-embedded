# M3.2 — ESP32 backend feasibility: IPv6 gate blocked

Base: master `100d8b1` (merged M3.1). Reviewed M3.1 acceptance, the existing
`WireGuardBackend`/`WireGuardTransport` contracts, `BUILD_PLAN.md` M3, and the
embedded design section 5/M3. The required tunnel carries **IPv6 overlay packets**,
even when the outer UDP endpoint is IPv4. IPv6 endpoint parsing alone is not enough.

**Decision:** reject the preferred ESPHome candidate as-is. Stop at this small
feasibility PR, as requested for an incompatible backend. No production backend
is implemented, no keys are provisioned, and M3.2 is **not complete**. M1–M3.1
production behavior is unchanged; the existing unavailable backend remains.

## Evaluated dependency

- Repository: [esphome-libs/wireguard](https://github.com/esphome-libs/wireguard).
- Tag: `v0.4.8`, commit `9b7660fed27151abdcbf32b43861044cf4a32497`.
- `library.json` says `0.4.8`; upstream `idf_component.yml` still says `0.4.6`.
  The immutable Git revision above identifies the source actually evaluated.
- License: BSD-3-Clause. Nothing is vendored into production.
- Isolated build dependency: `esphome/libsodium` pinned to `1.10021.1`, satisfying
  the candidate's `^1.10021.1` ESP-IDF dependency.
- Target: classic ESP32, ESP-IDF **5.2.1**, IPv6 enabled. The isolated build also
  enables PPP support (ESP-NETIF layout compatibility) and TCP/IP core locking.
  Enabling these options is not proof that upstream calls obey lwIP threading.

## Precise incompatibilities

All links refer to the evaluated immutable revision, rather than a moving branch.

1. **No IPv6 transmit path.**
   [`wireguardif.c:201`](https://github.com/esphome-libs/wireguard/blob/9b7660fed27151abdcbf32b43861044cf4a32497/src/wireguardif.c#L201)
   defines an IPv4 output callback. Interface initialization at line 1052 assigns
   `netif->output` but never assigns `netif->output_ip6`. An initialized interface
   therefore does not supply the IPv6 output path needed for bidirectional overlay
   traffic. Turning on `CONFIG_LWIP_IPV6` cannot add the missing implementation.
2. **Tunnel address creation uses IPv4 fields.**
   [`esp_wireguard.c:169`](https://github.com/esphome-libs/wireguard/blob/9b7660fed27151abdcbf32b43861044cf4a32497/src/esp_wireguard.c#L169)
   parses a generic IP address but passes it through `ip_2_ip4()` into `netif_add()`.
   It never assigns the requested IPv6 overlay address. Passing `fd00::2` as a
   string does not configure a proper IPv6 tunnel interface.
3. **IPv6 receive filtering and length handling are incomplete.**
   [`wireguardif.c:349`](https://github.com/esphome-libs/wireguard/blob/9b7660fed27151abdcbf32b43861044cf4a32497/src/wireguardif.c#L349)
   accepts an inner IPv6 packet without checking its configured allowed prefix,
   and reads its length using the IPv4 `IPH_LEN` macro. IPv6 has a different header
   layout and payload-length meaning. This does not enforce the required IPv6
   cryptokey route boundary, even if output/address setup were added externally.
4. **Teardown does not erase the private key held in the device allocation.**
   [`wireguardif_fini()`](https://github.com/esphome-libs/wireguard/blob/9b7660fed27151abdcbf32b43861044cf4a32497/src/wireguardif.c#L1122)
   frees the device without wiping it.
   [`wireguard_device_init()`](https://github.com/esphome-libs/wireguard/blob/9b7660fed27151abdcbf32b43861044cf4a32497/src/wireguard.c#L1004)
   copies the private key into that device. Clearing only an adapter's config
   would not meet the backend-owned secret-erasure contract.

These are source-level findings, not an assertion that every possible maintained
WireGuard implementation is unsuitable. They prevent accepting this candidate
without substantive network-stack changes and tests. A thin C++ wrapper would
conceal the blockers rather than resolve them. This PR adds no crypto or vendor
network-stack patches, and does not silently substitute IPv4 overlay support.

## Reproduction

Download the exact source outside the project (no production dependency changes):

```sh
curl -fL https://codeload.github.com/esphome-libs/wireguard/tar.gz/9b7660fed27151abdcbf32b43861044cf4a32497 -o /tmp/wireguard.tar.gz
mkdir -p /tmp/wireguard
tar -xzf /tmp/wireguard.tar.gz -C /tmp/wireguard --strip-components=1
python3 integration/wireguard_feasibility/audit.py /tmp/wireguard
```

The audit verifies SHA-256 hashes of both reviewed C files and reports the four
known blockers. Normal exit status is **1: gate blocked**. `--expect-blocked`
returns 0 only when those pinned findings are reproduced, useful for checking
this feasibility result. Hash mismatch or changed findings returns 2 and requires
manual review. The audit is not a general WireGuard conformance test or a feature
detector for newer revisions. It never executes upstream code or reads keys.

For an isolated compile/link check, with ESP-IDF 5.2.1 exported:

```sh
mkdir -p /tmp/neondoll-wg-probe
cp -a integration/wireguard_feasibility/. /tmp/neondoll-wg-probe/
idf.py -C /tmp/neondoll-wg-probe -B /tmp/neondoll-wg-build \
  -D WIREGUARD_SOURCE_DIR=/tmp/wireguard build
```

Use a dependency directory named `wireguard` so IDF resolves the component name.
The probe links real upstream init/connect/disconnect/status functions and
requires `LWIP_IPV6`. It never calls them or installs routes, starts Wi-Fi, embeds
keys, or reports tunnel connectivity. Component Manager fetches only the isolated
probe's pinned libsodium dependency. Build output and managed components stay
outside the production example. Reproduction downloads require network access.

## Validation boundary and next step

The source audit reproduced all four blockers. Existing 116 M1–M3.1 host tests and
ASan/UBSan regressions pass. There is **no new backend** whose lifecycle/fault
tests could be claimed: existing M3.1 fake-backend tests remain foundation tests.
The isolated upstream probe compiled and linked on ESP-IDF 5.2.1/classic ESP32
with IPv6 enabled; the resulting ELF retains all four real upstream lifecycle/
status symbols. Both default and Wi-Fi-enabled production firmware builds passed.
Compile/link success cannot resolve the IPv6 blockers.
The QEMU M1 regression passed three restarts with the same Body ID on an idle
rerun. The first run timed out before the ID print during concurrent upstream
compilation; the eight-second emulator deadline and production code were unchanged.

No real ESP32/peer handshake or encrypted packet exchange was attempted with
this rejected candidate. No physical board/AP/test peer was available in this
session. QEMU identity restarts test M1 persistence only. Runtime resource cost,
partial-init cleanup, authenticated-up reporting, key erasure and repeated
physical teardown remain unproven for any real backend.

Next implementation needs an upstream or separately reviewed maintained backend
with IPv6 interface addressing/output, correct inner packet validation and allowed
prefix enforcement, narrow route installation, and complete secret/resource
teardown. Pin it and test these before implementing `WireGuardBackend`. Then
verify M2-driven outage/reacquisition, no autonomous restarts after stop, actual
ESP32/Linux-peer authentication, bidirectional IPv6 packets, route isolation and
heap/stack/socket/netif stability. Do not declare M3.2 complete on an IPv4-only
handshake or a simulated backend.
