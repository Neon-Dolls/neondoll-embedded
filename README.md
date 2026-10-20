# NeonDoll Embedded

Reusable C++17 Body runtime with an ESP-IDF example for ESP32.

M1 implements persistent Body IDs with NVS storage, bootstrap hardware entropy,
versioned checksummed records and fail-closed loading. M2.1 adds an ESP32 Wi-Fi
station lifecycle adapter; M2.2 adds clock-driven network recovery and capped
backoff. Physical power-cycle acceptance is pending.

Build, test and flash instructions, migration behavior, cryptographic scope and
hardware acceptance checklist: [M1 acceptance](docs/M1_ACCEPTANCE.md).

Wi-Fi lifecycle usage, callback ownership and AP-toggle validation:
[M2.1 acceptance](docs/M2_1_ACCEPTANCE.md).

Recovery policy, intentional pause/shutdown and clock behavior:
[M2.2 acceptance](docs/M2_2_ACCEPTANCE.md).

WireGuard transport configuration/lifecycle foundation and deferred backend gate:
[M3.1 acceptance](docs/M3_1_ACCEPTANCE.md). No encrypted connectivity is claimed.

Isolated IPv6 interface patch against pinned ESPHome WireGuard v0.4.8:
[M3.2a acceptance](docs/M3_2A_IPV6.md). M3.2 and real-peer proof remain incomplete.

Pinned ESPHome backend evaluation and unresolved IPv6 blockers:
[M3.2 feasibility](docs/M3_2_FEASIBILITY.md). M3.2 remains incomplete.

M3.2b production ESP-IDF backend: [acceptance and hardware requirements](docs/M3_2B_ACCEPTANCE.md).

M3.2c opt-in ESP32/Linux interoperability lab: [run instructions and pending physical acceptance](docs/M3_2C_ACCEPTANCE.md).
