# M1 identity implementation and acceptance

Supported profile: classic ESP32, ESP-IDF 5.2.1, default NVS partition,
C++17. The example initializes identity before RF, ADC or I2S startup.

## Identity and cryptography decision

A Body ID is 16 bytes of hardware entropy encoded as 32 hex digits. It is an
identifier, not a signing credential. No signature is generated or claimed.
Per section 4.2 of `../NeonDoll_Embedded_Design_M1.2_to_M9.md`, the on-device
WireGuard keypair belongs to M3, where the selected vetted WireGuard library
will generate and retain its key material. Any additional Body signing identity
requires a public pairing/authentication contract decision. M1 does not invent
one. The older BUILD_PLAN's broad cryptographic identity requirement therefore
remains deferred to that explicitly identified slice; M1 identity persistence
alone must not be described as cryptographic enrollment.

## Durable record and failure behavior

- Logical storage key `neondoll/identity` maps to blob key `identity` in the
  `neondoll` NVS namespace (NVS keys have a 15-character limit).
- Current format is `v2:<id>:<counter>:<timestamp>:<hex-name>:<hex-implementation>:<hex-platform>:<hex-arch>:<crc32>`.
  CRC32/IEEE covers every byte before the final colon. CRC is accidental
  corruption detection, not authentication. Fields are bounded to 64 bytes each;
  reads are bounded to 1024 bytes. Counters/timestamps are bounded uint64 values.
- Only a genuinely absent identity permits generation. Empty, malformed,
  oversized, incompatible, wrong-storage-type or unreadable state fails closed.
  Public `load_identity` errors distinguish these cases; runtime initialization
  reports `-10` for identity failure. NVS initialization errors return runtime
  `-4`; IDF error values for initialization and writes are logged without data.
- First identity write commits an `identity_init` marker (`NDI1` plus NUL) before
  writing the identity. Missing identity with an existing marker fails closed.
  This intentionally requires owner recovery if power fails after the marker
  but before a complete first identity. An uncertain write poisons the handle
  until close/reopen. No partial or uncertain identity is exposed as initialized.
- Valid M1.1 `v1:<id>:<counter>:<timestamp>` records upgrade on first initialization
  with the same ID/counter/timestamp. Legacy records lack a checksum: syntactically
  valid legacy corruption cannot be detected retroactively. New v2 writes add
  integrity checks. A failed upgrade fails initialization; it never regenerates.
- Successful reboots do not rewrite records. No error path erases NVS. Factory
  reset is an explicit owner operation, separate from normal startup.
- Loss of all identity records or whole-partition erasure cannot be distinguished from a factory-clean device;
  this is outside the accidental record-corruption guarantee. NVS is not
  authenticated or confidential without device security configuration. Flash
  encryption and secure boot are deployment choices, not enabled by this example.
- Bootstrap entropy brackets `esp_fill_random` with
  `bootloader_random_enable/disable`, before application RF/ADC/I2S use. The
  adapter requires serialized exclusive access. Do not invoke it while those
  subsystems are running; M2 must preserve startup ordering, and M3 must select
  an entropy path appropriate for live-radio key generation.

## Host checks

With CMake, a C++17 compiler, and GoogleTest installed:

```sh
cmake -S . -B build/host
cmake --build build/host
ctest --test-dir build/host --output-on-failure
```

Alternatively pass `-DGTEST_SOURCE_DIR=/path/to/googletest` (tested with 1.15.2).
The first executable tests portable lifecycle/storage, legacy upgrade, field
bounds, every single-byte corruption/truncation, positive/short entropy returns,
and 100 repeated boots with one entropy call and one write. The second compiles
the actual ESP32 adapter against injected IDF functions: missing/type/size/I/O
errors, init/full storage failures, commit failures, interrupted upgrades and
first-write marker failures, entropy source activation and handle ownership.
These fault models do not establish actual NVS power-loss behavior on hardware.

## Observed validation (2026-10-18)

- 34/34 host tests passed (24 portable/lifecycle + 10 actual adapter fault tests),
  through CMake/CTest. Compiled with GCC 16.1.1, explicit `-std=c++17`,
  `-Wall -Wextra -Werror -pedantic-errors`.
- Official `espressif/idf:v5.2.1` container: ESP32 firmware compiled and linked,
  image size `0x389a0` bytes (231,840 bytes), 78% of default app partition free.
  Compiler commands confirm final `-std=c++17` for runtime and example.
- `integration/qemu_identity.py` ran the compiled firmware three times against
  one emulated flash image. All three reported Body ID
  `be5d944f6480ba8fa87296d98b830376`. This exercises the real IDF NVS implementation
  across emulator process restarts. It does not prove hardware entropy quality,
  real flash reliability, or physical power-cycle behavior.
- AddressSanitizer was unavailable in the local host toolchain (missing
  `libasan.so.8.0.0`); no sanitizer pass is claimed.

Reproduce the emulator check in an IDF environment with Espressif QEMU installed:

```sh
python integration/qemu_identity.py examples/minimal_body/build
```

This generates a disposable `qemu_identity_flash.bin` inside the build directory
and saves three boot logs there. CI runs this check after the firmware build.

## Firmware build and physical acceptance

```sh
. /path/to/esp-idf/export.sh
idf.py -C examples/minimal_body set-target esp32
idf.py -C examples/minimal_body build
idf.py -C examples/minimal_body -p /dev/ttyUSB0 flash monitor
```

To run the fresh-device check, use a disposable test board or explicitly
owner-authorized erased board; erasing deletes all prior NVS identity/state.

1. Boot a clean device. Save the serial `NeonDoll Body ID: ...` line.
2. Disconnect power, reconnect, and capture the identical ID. Repeat at least
   ten times, including reset-button and software-restart cycles.
3. Test an existing empty/malformed/truncated identity blob, invalid checksum,
   wrong NVS type and unsupported version. Capture initialization failure and
   confirm repeated boots do not replace the state.
4. Inject NVS full/init/read/write/commit failures. Capture failure without erase.
5. Cut power around marker and identity writes; confirm valid committed identity
   survives, or initialization fails closed. Marker-only state must not create
   another ID. Retain logs and the exact board/flash/IDF version.

Physical serial/power-cycle/fault evidence: **NOT YET TESTED** in this session.
Host fault tests and firmware compilation cannot substitute for these checks.
M1 hardware acceptance remains pending until a board transcript is recorded.
