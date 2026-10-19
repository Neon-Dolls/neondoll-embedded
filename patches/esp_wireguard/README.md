# NeonDoll ESPHome WireGuard patch series

Upstream: `esphome-libs/wireguard` v0.4.8,
`9b7660fed27151abdcbf32b43861044cf4a32497`.

`series` is the application order. `prepare.py ARCHIVE EMPTY_DESTINATION` verifies
an immutable archive SHA-256, safely extracts it, checks/applies each patch with
`git apply`, and verifies that only the four declared interface/network files
changed. Cryptographic source files must remain byte-for-byte identical.
No dependency is vendored into or linked to the production NeonDoll component.
The local Git attribute preserves unified-diff context whitespace in patch files.

The patch adds IPv6 addressing/output/receive filtering and deliberately disables
upstream default-interface APIs. New helper code is BSD-3-Clause, matching the
upstream library; upstream notices and LICENSE remain in the prepared dependency.

Reproduction, test/build commands, API differences and remaining blockers:
[M3.2a acceptance](../../docs/M3_2A_IPV6.md).
