# NeonDoll Embedded

Reusable C++17 Body runtime with an ESP-IDF example for ESP32.

M1 implements persistent Body IDs with NVS storage, bootstrap hardware entropy,
versioned checksummed records and fail-closed loading. M2.1 adds an ESP32 Wi-Fi
station lifecycle adapter. Physical power-cycle acceptance is pending.

Build, test and flash instructions, migration behavior, cryptographic scope and
hardware acceptance checklist: [M1 acceptance](docs/M1_ACCEPTANCE.md).

Wi-Fi lifecycle usage, callback ownership and AP-toggle validation:
[M2.1 acceptance](docs/M2_1_ACCEPTANCE.md).
