# NeonDoll Embedded

Reusable C++17 Body runtime with an ESP-IDF example for ESP32.

M1 implements persistent Body IDs with NVS storage, bootstrap hardware entropy,
versioned checksummed records and fail-closed loading. Network lifecycle begins
in M2. Physical power-cycle acceptance is pending.

Build, test and flash instructions, migration behavior, cryptographic scope and
hardware acceptance checklist: [M1 acceptance](docs/M1_ACCEPTANCE.md).
