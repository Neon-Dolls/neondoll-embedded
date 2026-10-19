# M2.2 — Network recovery and reconnection policy

Based on master `564a925`, including the merged M2.1 stop/restart cleanup fix.
Scope: a small, portable network supervisor above the existing station adapter.
No WireGuard, Relay, pairing, endpoint selection, or higher-level sessions.

## Ownership and integration

`NetworkSupervisor` takes a `NetworkConnection` port, the existing `Clock`, and
`NetworkRecoveryPolicy`. It allocates no memory, creates no tasks/timers, registers
no callbacks and stores no credentials. `Esp32NetworkAvailability` implements the
control port with its existing lifecycle methods and read-only status getters.
Its existing `NetworkAvailability` callback remains owned by the application.

The application still wires the adapter into `PlatformContext`, calls
`neondoll_init()` to load/create the durable identity, and only then starts the
Wi-Fi station. Supervisor construction does no I/O; `start()` reads the clock and
arms an immediate attempt for the next `poll()`. The example now uses:

```cpp
// After neondoll_init(&ctx) succeeds and the application registers its callback:
network.start(ssid, password, 0); // Check return values in application code.
neondoll::NetworkSupervisor recovery(network, clock, {1000, 30000, 30000});
recovery.start();
// In the existing application tick/task, call recovery.poll() regularly.
```

The zero adapter retry limit gives all association retry timing to this policy.
Standalone M2.1 behavior is unchanged. If an application chooses a nonzero native
retry limit, that finite burst is still handled by M2.1; the supervisor detects
its exhaustion and backs off. The one-connect-per-poll bound applies to supervisor
requests, not the optional adapter burst.

While supervision is active, issue connection/disconnection/shutdown requests
through the supervisor. Do not independently drive the adapter's lifecycle.
Application configuration and station startup/restart remain outside the policy.
A stopped station requires explicit application `network.start()` followed by
`recovery.start()`; a recovery request alone never resurrects a stopped driver.

All methods, getters and destruction belong to one serialized owner task.
Dependencies must outlive the supervisor. Application callbacks run only in the
underlying adapter poll, with the same initial snapshot/offline/available/changed
semantics as M2.1. Callbacks may inspect state but must defer lifecycle requests
back to the owner task; reentrant methods return `-2`. Do not destroy either object
from its callback. The supervisor destructor stops a connection it owns.

## Recovery behavior

- Initial connection is attempted once on the first poll. Successful API return
  means association is pending, not online; only acquired IP makes it online.
- Synchronous connect failure, exhausted adapter retries, unexpected network loss
  or connection timeout cancel the pending association and schedule backoff.
  Default waits: 1, 2, 4, 8, 16, 30, 30… seconds. The delay is capped, and recovery
  continues at that bounded rate throughout an extended outage.
- The default connection timeout is 30 seconds, including DHCP acquisition.
  A hung association/DHCP attempt therefore cannot suppress recovery indefinitely.
- Successful IP acquisition resets the failure count, next delay and error.
  A read-only adapter acquisition epoch observes even an IP acquired and lost
  within one adapter poll, without intercepting/coalescing application callbacks.
- `disconnect()` is an intentional pause: cancel pending requests and timers, then
  disconnect. No automatic reconnect occurs, even if disconnect reports failure.
- `request_recovery()` coalesces requests for one immediate attempt on the next
  poll from paused/backoff. It does not reset the failure streak, interrupt a
  connecting/healthy station, or reenable a stopped supervisor.
- `stop()` disables policy and cancels pending manual recovery before driver
  teardown. Even teardown failure leaves the supervisor stopped; repeated stop
  may retry cleanup, but polling never reconnects. M2.1 retains responsibility for
  residual handles, generation tokens and safe resource cleanup.
- A failed cancellation must succeed before another connect is issued. Recovery
  failures schedule another delay rather than spinning on cleanup.

`poll()` invokes at most one supervisor connect attempt. It has no sleep, retry
loop or deadline wait, and a large clock jump never causes catch-up bursts. The
application's normal task scheduling determines poll cadence; the example keeps
its existing FreeRTOS task yield.

## Clock and bounds

`initial_delay_ms` and `connection_timeout_ms` must be positive;
`maximum_delay_ms` must be at least the initial delay. Backoff doubling saturates
at the cap and the diagnostic failure count saturates at `UINT32_MAX`. State and
storage remain fixed-size regardless of outage duration or recovery count.

Elapsed milliseconds are accumulated with saturating arithmetic, avoiding
`now + delay` deadline overflow. Frozen clocks do not cause retries. A backward
jump or uint64 wrap contributes zero elapsed time for that sample and rebases the
next sample, preserving already accrued time without making a retry instantly
overdue. The injected clock normally remains monotonic as required by `Clock`.

The policy is deterministic. Jitter and multi-transport/path supervision belong
to later work; M2.2 does not use bootstrap entropy after RF startup.

Results: `0` waiting/progress/success (inspect state or availability callback),
`-1` invalid policy/station not running, `-2` duplicate start or reentry,
port errors for failed network actions/loss (normally `-3`/`-4`), and `-5` for a
connection timeout. `last_error()` retains the latest recovery error until IP
success or explicit policy reset; failures are returned when the action occurs,
not on every tick during an otherwise quiet backoff.

## Validation

Host checks are included in the existing CMake/CTest build:

```sh
cmake -S . -B build/host
cmake --build build/host
ctest --test-dir build/host --output-on-failure
```

The portable policy tests cover deferred initial connection, successful recovery,
retry exhaustion, capped progression, 1,000 failed outage attempts, 1,000 recovery
cycles, intentional pause, explicit request coalescing, shutdown during backoff,
connection timeout, cancellation/shutdown failure, repeated start, clock freeze,
large jumps, backward jumps, wrap and saturating arithmetic. Adapter-stub tests
check native retry exhaustion, 100 real-adapter recovery cycles with stable
handler/queue/interface counts, callback ordering and IP-change semantics, stale
events after shutdown, explicit driver restart and transient IP acquisition.
Existing M1 and M2.1 fault tests remain in the suite.

Use the firmware/QEMU commands in [M2.1 acceptance](M2_1_ACCEPTANCE.md) to build
both profiles with ESP-IDF 5.2.1 and run the identity regression. The Wi-Fi-enabled
profile now compiles/links the supervisor. The default QEMU profile keeps RF
disabled and checks stable identity across three emulator process restarts.

Observed validation (2026-10-19):

- 92/92 host tests passed: all 64 baseline tests plus 24 portable policy tests
  and four additional ESP32 adapter/supervisor integration tests.
- All 92 tests passed under AddressSanitizer and UndefinedBehaviorSanitizer using
  the IDF container's host compiler. The normal host build also passed strict
  C++17 with `-Wall -Wextra -Werror -pedantic-errors`.
- Official ESP-IDF 5.2.1 / classic ESP32: both firmware builds passed. Default
  image: `0x64490` bytes; Wi-Fi-enabled image: `0xc1110` bytes.
- QEMU: compiled default firmware retained the same Body ID across three process
  restarts against one emulated flash image.

## Hardware-only checks (not yet observed)

On a physical ESP32, capture the same Body ID before/after these operations:

1. Cold boot → identity initialization → Wi-Fi association/DHCP → available.
2. Turn the AP off for several minutes. Verify capped retries without CPU/log
   spinning, then restore it and observe recovery without reboot or re-pairing.
3. Test wrong credentials and association without DHCP; verify timeout/backoff.
4. Pause intentionally while recovering; AP restoration must not reconnect until
   an explicit recovery request. Shutdown during backoff must remain stopped even
   after clocks progress and the AP returns.
5. Exercise repeated recoveries, explicit station restarts and physical reboot;
   check stable identity, handler/task counts, heap and RF behavior over time.

Host fault models and QEMU do not establish real radio, DHCP timing, power-cycle
or long-duration hardware resource acceptance.
