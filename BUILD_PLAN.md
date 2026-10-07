# NeonDoll Embedded — Build Plan

## Goal

Build a small reusable NeonDoll Body runtime for constrained embedded devices.

The first supported platform is **ESP32**, but platform-specific networking, storage, timing, and hardware must remain behind narrow interfaces so the protocol/runtime itself is not unnecessarily ESP32-specific.

A device using this library should eventually need to provide little more than:

- platform/network configuration
- its Body metadata
- the capabilities implemented by the device
- handlers for those capabilities and device-generated events

The embedded runtime owns the NeonDoll machinery.

## Architectural Rule

**The embedded device is a Body, not a miniature Core.**

It does not perform Doll cognition. It provides physical capabilities, input/output, sensors, actuators, displays, audio, or other forms of embodiment to a Doll Core.

The runtime should remain small, deterministic, observable, and boring.

## M0 — Repository and Platform Skeleton

Establish a buildable embedded library without implementing NeonDoll protocol behavior yet.

Target:

- ESP32 using ESP-IDF as the primary platform.
- C/C++ implementation.
- Host-buildable protocol/runtime components where practical so most tests do not require hardware.
- Clear separation between portable NeonDoll code and ESP32-specific adapters.

Suggested shape:

```
components/
  neondoll/
    include/
    src/

platform/
  esp32/

examples/
  minimal_body/

tests/
docs/
```

Define narrow platform interfaces for:

- persistent storage
- entropy/randomness
- clock/timers
- network availability
- logging

Acceptance:

- library builds for ESP32
- minimal firmware links against it
- portable pieces can be tested without flashing hardware
- CI builds the library/example

## M1 — Persistent Body Identity

An ESP32 becomes a persistent NeonDoll Body.

Implement:

- Body identity generation
- persistent Body ID
- persistent cryptographic identity required by the public Body/Doll Network contracts
- Body metadata
- clean first-boot vs restart behavior

ESP32 persistence should use an appropriate durable store such as NVS behind the storage abstraction.

Acceptance:

1. Flash clean device.
2. Boot.
3. Body identity is generated and persisted.
4. Reboot.
5. Exact same identity returns.
6. Firmware restart must never silently create a new Body.

Provide a host test for persistence semantics where possible.

## M2 — Network Lifecycle

Give the runtime a generic notion of network availability.

ESP32 first implementation:

- Wi-Fi station mode
- connect/disconnect observation
- IP acquisition/loss
- reconnect handling
- runtime receives network-state transitions

Do not mix Wi-Fi credentials or Wi-Fi policy into Doll Network protocol code.

Application/platform owns how connectivity is configured.

Runtime only needs to know:

```
offline
network available
network changed
```

Acceptance:

- device can lose Wi-Fi and regain it without rebooting
- NeonDoll runtime is notified deterministically
- no Body identity/state is lost during network changes

## M3 — Embedded Doll Network / WireGuard

Establish the private Doll Network from an ESP32.

Investigate and select the smallest viable ESP32 WireGuard implementation rather than writing WireGuard ourselves.

Requirements:

- persistent WG keypair
- Body WG identity survives reboot
- configure Core peer
- IPv6 support sufficient for Doll Network addressing
- tunnel start/stop/restart
- observable tunnel state

Keep WireGuard behind a transport/platform interface so the rest of NeonDoll Embedded does not depend directly on one ESP32 WG library.

Acceptance:

ESP32 can establish a real WireGuard tunnel to a test peer and exchange traffic across the tunnel.

Document memory/flash cost.

Important: the existing public-protocol direct endpoint discovery hole remains a protocol hole. Tests/configuration may inject an explicit endpoint. Do not invent discovery semantics.

## M4 — Pairing

Implement NeonDoll Body pairing using the public protocol only.

Pairing should establish and persist the durable relationship required by later boots:

- Body identity
- Doll Network identity/address
- Core peer identity
- Core WG public key
- canonical membership information supplied by the protocol

The application must not need to understand the pairing payload.

Expose a small API conceptually similar to:

```
pair(invitation)
paired()
membership()
```

Acceptance:

- clean ESP32 pairs
- membership persists
- reboot does not require pairing again
- corrupt/incomplete persisted membership fails closed
- accidental re-pair does not silently replace an existing relationship
- private key material never leaves the Body

## M5 — Doll Link Core

Implement the smallest portable Doll Link runtime.

Include only behavior already defined by the public protocol:

- framing
- serialization/deserialization
- hello
- capability advertisement/negotiation
- ready state
- protocol/version validation
- clean failure on malformed/unsupported messages

Design for constrained memory.

Avoid:

- large DOM-style message representations when streaming/fixed structures suffice
- unnecessary dynamic allocation
- unbounded queues
- unbounded strings/messages

Acceptance:

Host tests prove protocol behavior.

ESP32 can complete the currently defined Doll Link handshake against a test peer using an injected network path.

Do not invent missing Interaction Session semantics.

## M6 — Embedded Capability API

This is the primary API presented to embedded Body applications.

An application should be able to register capabilities without knowing Doll Link internals.

Conceptually:

```cpp
body.registerCapability(display);
body.registerCapability(buttons);
body.registerCapability(haptics);
```

The runtime should own:

- capability advertisement
- dispatch
- request IDs
- success/failure response mechanics where canonically defined
- lifecycle

The application owns hardware behavior.

Also provide a path for device-originated events where the public protocol permits them.

Do not define new canonical capability names or operation schemas merely because a particular ESP32 device needs them. If the public specification lacks a required capability contract, document the hole.

## M7 — Reconnect and Mobility

An embedded Body must survive ordinary physical-device networking.

Handle:

- Wi-Fi loss
- Wi-Fi reacquisition
- IP change
- WireGuard restart
- Doll Link loss
- Core temporarily unavailable
- device reboot

Durable state:

- Body identity
- WG identity
- membership/Core relationship

Ephemeral state:

- Wi-Fi connection
- sockets
- tunnel state
- active Doll Link connection
- selected route
- reconnect/backoff state

Reconnect must be bounded and observable. No tight reconnect loops.

Acceptance:

```
connected
→ Wi-Fi disappears
→ connection dies
→ Wi-Fi returns
→ tunnel reconstructed
→ Doll Link reconstructed
```

without re-pairing or changing identity.

Where current protocol discovery holes prevent full automatic reconstruction, test using injected endpoint information and explicitly record the limitation.

## M8 — Resource Discipline

Treat constrained hardware as a first-class architectural requirement.

Measure:

- firmware size
- static RAM
- heap after boot
- heap after network initialization
- heap after WireGuard
- heap after Doll Link
- heap after reconnect cycles
- stack high-water marks where useful

Stress:

- repeated connect/disconnect
- malformed packets
- repeated Doll Link reconnect
- capability requests
- long uptime
- persistence across many restarts

Requirements:

- no unbounded queues
- no unbounded retry growth
- no obvious reconnect leaks
- predictable maximum message sizes
- explicit limits documented

Produce a small resource-budget document for supported ESP32 targets.

## M9 — Embedded Conformance

Repeat the lesson learned from Shell Body.

Create an embedded conformance inventory covering:

- identity
- pairing
- Doll Network
- WireGuard
- Doll Link
- capabilities
- reconnect
- restart
- failure behavior

Classify each as:

- implemented + tested
- hardware-tested
- externally conformance-tested
- blocked by public protocol contract
- not implemented

Never turn a missing public contract into an ESP32-specific convention merely to finish a milestone.

## First Reference Body

Only after the runtime reaches a useful baseline, create the first reference device.

A simple reference Body should contain:

- ESP32
- small display
- buttons
- optional LED/haptic output

Its purpose is to prove that an application can be mostly hardware code while `neondoll-embedded` owns the NeonDoll relationship.

TamaFi or a TamaFi-derived device is a strong candidate, but the reusable runtime must not depend on its hardware layout.

## Non-goals for v1

Do not put these into the initial embedded runtime:

- local LLM inference
- autonomous cognition
- Core behavior
- generic shell execution
- arbitrary plugin loading
- heavyweight scripting runtime
- device-specific UI framework
- Home Assistant-style device ecosystem
- invented protocol extensions

Those can be separate layers later.

## Guiding Acceptance Test

At the end of this build:

> A fresh ESP32 can become a persistent NeonDoll Body, pair with a Core, join its Doll Network, establish Doll Link, advertise hardware capabilities, lose power or networking, and return as the same Body without the application implementing NeonDoll networking itself.

And, critically:

> Everything necessary to implement that behavior must be derivable from NeonDoll's public contracts.

If that second statement fails, record the protocol hole rather than working around it privately.
