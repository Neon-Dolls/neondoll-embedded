# M2.1 — ESP32 network lifecycle

Based on master `82f9ffd` and the engineering design's M2 section. Scope is a
single owned ESP32 Wi-Fi station and the existing `NetworkAvailability` callback.
No WireGuard, Relay, pairing, or higher-level connection/session supervisor.

## Use and startup order

`Esp32NetworkAvailability` is declared in `neondoll/esp32_network.hpp`, also
included by `esp32_platform.hpp`. The application owns credentials and the
reconnect-attempt limit. Wi-Fi NVS persistence is disabled and storage is RAM.
The adapter never changes Body identity or the identity NVS namespace.

1. Wire the adapter into `PlatformContext`, then call `neondoll_init(&ctx)`.
   `network.init()` only allocates its bounded mailbox and reserves station
   ownership. It does not initialize the network stack, register handlers, or
   initialize/start Wi-Fi. First-boot bootstrap entropy therefore finishes first.
2. Register `set_callback(cb, arg)`, then call `start(ssid, password, retry_limit)`.
   This initializes netif/default event loop and Wi-Fi, configures station mode
   and RAM credentials, registers handlers, and starts Wi-Fi without associating.
3. Call `connect()` and call `poll()` regularly from the same application task.
   Only an acquired, nonzero IPv4 address means available; association alone does
   not. An unexpected disconnect emits offline and retries association up to the
   application-specified limit (default three). Each failure consumes an attempt;
   successful IP acquisition resets the counter. Synchronous connect errors and
   exhausted retries need an explicit application `connect()` retry. This slice
   has no timer/backoff/path-selection supervisor.
4. `disconnect()` suppresses automatic retries, marks offline, and disconnects.
   `connect()` can resume. `stop()` tears down the station while keeping the
   initialized adapter and callback for a later start. `deinit()` clears the
   callback, tears down resources, and releases the mailbox and owner slot. The
   destructor calls deinit. Repeated init/deinit and stop calls are supported.

The callback receives an initial snapshot on the next poll, then changes only.
For an IP change while available it receives `true` again; inspect `last_change()`
(`offline`, `available`, `changed`) and `ipv4_address()` inside the callback. This
preserves the existing bool callback interface without a portable runtime API
redesign. IPv4 addresses are in network byte order. Delayed lost-IP timers are
ignored if netif already has an address.

All public API calls, getters and destruction belong to one serialized owner
task. Callbacks run only in that task's `poll()`. They may inspect getters but
must post lifecycle requests back to the owner rather than reentering lifecycle
methods; those methods reject reentry. Do not destroy the adapter from its own
callback or concurrently with its public methods. Callback arguments must remain
alive until callback removal or deinit. IDF event handlers never invoke the
application callback and never retain an adapter pointer.

## Teardown and bounded behavior

- A 16-element mailbox receives copied IDF events without blocking. Each poll
  drains at most 16 events. Overflow drops the uncertain sequence, marks offline,
  requests disconnect, and returns `-4`; the application must explicitly reconnect.
- Generation tokens reject old handler invocations after stop/restart or object
  destruction. Ingress is disabled under a mutex that synchronizes with the
  event task before unregistering handlers or deleting the mailbox. No mutex is
  held across application callbacks, Wi-Fi calls or handler unregistration.
- Cleanup attempts both handler unregistrations, stop, driver deinit, default
  station destruction and owned event-loop deletion. If teardown fails, ingress
  remains disabled and residual handles stay in bounded process-lifetime storage.
  The next init/start retries cleanup before creating new resources; registrations
  cannot accumulate across failures. Old callbacks remain safe even if IDF refuses
  to unregister them.
- An existing application event loop is reused and never deleted. Keep it alive
  until adapter teardown/cleanup succeeds. An existing
  default station is rejected. The adapter owns a single station, not an existing
  application's Wi-Fi driver. A second adapter cannot take ownership concurrently.
  ESP-netif's global initialization is left intact because IDF 5.2.1 does not
  support global netif deinitialization.
- Public results: `0` success, `-1` invalid input/not initialized/not started,
  `-2` ownership conflict/already started/reentry, `-3` IDF failure (inspect
  `last_error()`), `-4` mailbox overflow. SSIDs are 1–32 bytes, passwords 0–64;
  IDF performs the remaining Wi-Fi configuration validation. No credentials are
  logged. This adapter reports local IPv4 availability, not Internet reachability.

## Build and test

Host (GoogleTest installed, or pass `-DGTEST_SOURCE_DIR=/path/to/googletest`):

```sh
cmake -S . -B build/host
cmake --build build/host
ctest --test-dir build/host --output-on-failure
```

ESP-IDF 5.2.1, classic ESP32:

```sh
. /path/to/esp-idf/export.sh
idf.py -C examples/minimal_body build
python integration/qemu_identity.py examples/minimal_body/build
idf.py -C examples/minimal_body -B "$PWD/examples/minimal_body/build/wifi" \
  -D SDKCONFIG="$PWD/examples/minimal_body/build/sdkconfig.wifi" \
  -D SDKCONFIG_DEFAULTS="$PWD/examples/minimal_body/sdkconfig.wifi-ci.defaults" build
```

The last profile uses dummy credentials and proves compilation/linking of the
Wi-Fi-enabled example. Default Wi-Fi is disabled so QEMU continues to exercise
M1 identity persistence without pretending to emulate a real access point. IDF
5.2.1's FreeRTOS/event headers require GNU variadic-macro behavior, so firmware
uses explicit GNU C++17; portable host code remains strict ISO C++17.

For an actual board, use `idf.py -C examples/minimal_body menuconfig` → NeonDoll
example, enable Wi-Fi and configure the AP credentials; build/flash/monitor using
that same build/configuration. Credentials enter the local config and firmware;
keep real credentials out of source control.

## Evidence and remaining hardware gate

Observed validation (2026-10-18):

- 61/61 host tests passed through CMake/CTest: 24 portable identity/lifecycle,
  15 ESP32 storage/entropy fault tests and 22 new network lifecycle/fault tests.
- All 61 tests also passed with AddressSanitizer and UndefinedBehaviorSanitizer
  using the container's GCC 11 toolchain. The normal host build uses GCC 16 with
  `-Wall -Wextra -Werror -pedantic-errors`.
- Official `espressif/idf:v5.2.1`: both classic ESP32 builds passed, including the
  Wi-Fi-enabled example. Default image: `0x639d0` bytes; Wi-Fi-enabled image:
  `0xc0b50` bytes (25% of the default app partition remains free).
- Compiled default firmware retained the same Body ID across three QEMU process
  restarts using one emulated flash image. This remains a persistence regression,
  not a radio/connectivity test.

Host tests cover initial offline, association vs IP acquisition, IP changes,
IP loss and delayed timers, unexpected disconnect/reconnect, retry exhaustion,
synchronous connection/disconnection failures, startup failure at each IDF API,
allocation failure, ownership, 100 repeated cycles, stale callbacks, unregister
and teardown failures/recovery, queue overflow, event delivery racing shutdown,
and real runtime identity/entropy ordering with the actual network adapter.

Physical Wi-Fi validation is **NOT YET TESTED**. On a real ESP32:

1. Capture stable Body ID, then network_available after association and DHCP.
2. Toggle the AP off/on (including outages longer than the retry budget). Capture
   offline and restored availability; explicitly reconnect after budget exhaustion.
3. Change the DHCP address. Confirm network_changed or offline → available in
   order, depending on whether the connection itself was lost.
4. Test wrong credentials and unreachable AP: bounded retries, no busy loop, same ID.
5. Repeat stop/start and init/deinit while the AP flaps; verify no callbacks after
   deinit, no resource growth, and the same Body ID after reboot.

QEMU persistence is a regression check for M1, not proof of real RF or DHCP.
