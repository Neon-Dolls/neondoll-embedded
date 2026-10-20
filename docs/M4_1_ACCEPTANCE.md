# M4.1 — Doll Link Transport Foundation

This is the requested M4.1 transport slice. `BUILD_PLAN.md` and the external
embedded engineering design call pairing M4 and Doll Link Core M5. This PR does
not renumber those milestones or implement pairing/M5 negotiation.

## Canonical contract and authentication gate

Reviewed public NeonDoll-Concepts revision
`23d3e42c589c7d7f07068c9256d6713cc1d1f8aa`:

- [Body Protocol v1](https://github.com/dividebyzero/NeonDoll-Concepts/blob/23d3e42c589c7d7f07068c9256d6713cc1d1f8aa/body-protocol.md):
  JSON-first, transport-independent; WebSocket JSON is the Core 2 reference
  transport. Canonical negotiation is `body.hello`, `core.hello`,
  `body.capabilities`, `body.ready`. Body identity is independent of network
  addresses and authentication credentials. Production pairing is not defined;
  authentication may be externally configured.
- [Doll Link draft, transport and §9](https://github.com/dividebyzero/NeonDoll-Concepts/blob/23d3e42c589c7d7f07068c9256d6713cc1d1f8aa/doll-link.md):
  transport is TBD; Body authentication lists proposals, not a selected wire
  mechanism. Its older message examples do not override Body Protocol v1.
- [Doll Network Protocol](https://github.com/dividebyzero/NeonDoll-Concepts/blob/23d3e42c589c7d7f07068c9256d6713cc1d1f8aa/doll-network-protocol.md):
  WireGuard connectivity does not replace Doll Link authentication; overlay
  addresses and Relay routes confer no Body identity/authority.

**Missing cross-repository decision:** select and publish Body-to-Core proof,
credential enrollment/trust, credential-to-Body binding, revocation, and reconnect
semantics. Also specify the interoperable WebSocket endpoint/subprotocol and
transport trust configuration before implementing a production adapter.

Accordingly this slice implements only a transport-neutral foundation. It adds
no wire framing, sockets, authentication scheme, JSON codec, credentials,
Body hello, negotiation, or capability execution. It cannot connect to a Core on
its own. `connected` means the injected transport reports a connection; it NEVER
means authenticated Body or Body Protocol ready. WireGuard authenticated peer
readiness is a prerequisite for beginning a connection, not Body authentication.
A future adapter must implement canonical WebSocket framing, not treat TCP reads
as messages. A future protocol layer must validate canonical envelopes and gate
semantic execution on authentication and negotiation.

## Ownership and integration

`DollLinkClient` exposes connect/disconnect/poll/send/receive/status.
`DollLinkConnection` implements lifecycle and fixed message queues using an
injected `DollLinkMessageTransport`, `Clock`, `NetworkConnection`, and existing
`TunnelTransport`. It never owns or changes M1 identity, entropy ordering,
network callbacks, Wi-Fi policy, WireGuard, routes, or credentials.

Create the client after successful M1 initialization. All dependencies outlive
it; all operations run on the same owner task, without reentrancy:

```cpp
// After station start, recovery.start(), tunnel.init(config), tunnel.start():
neondoll::DollLinkConnection link(network, tunnel, message_transport, clock);
link.connect(); // Arms intent; no I/O here.
// Each owner-task iteration:
recovery.poll();
tunnel.poll();
link.poll();
// Send/receive only when connected; these copy complete messages to/from queues.
// Shutdown: cancel link first; retry disconnect until successful, then stop
// tunnel and network. Never destroy dependencies while cleanup is pending.
```

Poll the client after each tunnel transition, including an explicit stop, before
restarting the tunnel. The existing tunnel interface has no generation counter;
a stop/start cycle hidden entirely between client polls cannot be detected.
Network availability epochs independently invalidate sessions even when the
sampled availability is again true. No interface contract or WireGuard code was
changed to hide this integration requirement.

- `connect()` is idempotent while armed. `disconnect()` cancels intent BEFORE
  cleanup. Reconnect after cancellation requires another explicit `connect()`.
- Tunnel/network loss closes the message transport and clears BOTH queues.
  Reacquisition permits a fresh connection, with no replay of stale messages.
- Failed begin, poll, send, receive, status or timeout cleans partial resources
  and schedules a fixed retry interval (default 1,000 ms). A poll starts at most
  one attempt; there are no sleeps, busy loops, timer tasks or backend retries.
- Failed close retains a cleanup obligation and blocks `connect()`. A cleanup
  poll never begins a connection in the same call. After successful cleanup,
  canceled intent stays canceled; armed recovery can continue on a later poll.
- Backend close must quiesce callbacks even on failure. Destruction attempts
  cleanup once; callers must successfully disconnect before destroying an
  adapter that can fail close. No callback retains the client.

## Bounds and timeouts

Two TX and two RX slots, each 1,024 bytes, are embedded in the object (4,096 bytes
of message storage plus lengths/state). There is no client heap allocation.
Prefer long-lived/static owner storage on ESP32 rather than a small task stack.
The 1,024-byte limit is a foundation profile, not a claim of full Body Protocol
or Doll Link file-transfer conformance.

`send()` copies one whole nonempty message; full TX returns `link_would_block`
without accepting data. Poll sends at most one and receives at most one message.
Full RX pauses adapter reads; the eventual adapter must also bound its own
buffers and propagate backpressure. Small caller RX buffers return required
length and leave the message queued. Oversize/empty adapter messages fail closed.
No silent truncation, partial-message acceptance, or unbounded queue exists.

Default connect timeout: 10 seconds. Optional inactivity timeout: disabled
(no invented heartbeat). Successful send/receive resets inactivity. Deadlines
use monotonic `Clock`, elapsed subtraction, and restart intervals on clock
regression/wrap rather than overflowing absolute timestamps. Zero connect timeout
or retry interval is rejected. Deadlines take precedence at their exact boundary.
An adapter must itself provide bounded nonblocking poll/begin/close work and
complete-message I/O; this layer cannot enforce a blocking third-party adapter.

## Validation

See the PR for executed commands/results. `host_test/test_doll_link.cpp` covers
readiness gates, idempotency/cancellation, FIFO limits/backpressure, no stale
replay, epoch changes, timeouts/backoff, partial begin, failed cleanup/restart,
I/O/status faults, malformed adapter lengths, repeated cycles/destruction,
clock boundaries, and integration with the existing WireGuard lifecycle.
The normal CI host and sanitizer jobs include the new target; ESP-IDF builds
compile the foundation as part of the existing component.

No physical hardware, real Doll Link peer, Body authentication, protocol
negotiation, or interoperability acceptance is claimed. **M3.2 physical WireGuard
hardware acceptance remains pending**, as recorded in `M3_2C_ACCEPTANCE.md`.
