# NeonDoll Embedded — Build Plan

## Purpose

`neondoll-embedded` is an independent reference Body that lets a Doll inhabit an ESP32 microcontroller.

It is intentionally small, inspectable, and useful for protocol/conformance work on constrained devices.

It MUST be implementable from the public NeonDoll specifications without access to Doll Core internals.

The Embedded Body is **not** a second Core and is **not** implicitly a remote shell.

> An Embedded Body means the Doll can interact through an embedded device. Host shell/process execution is a separate explicit Body capability.

## Sources of truth

Protocol and semantics are defined by the public NeonDoll specifications, especially:

- `body-contract.md`
- `body-protocol.md`
- `doll-link.md`
- `doll-network.md`
- `doll-network-protocol.md`
- `doll-relay.md`
- `doll-relay-protocol.md`

The `Neon-Dolls/neondoll` repository may be inspected as a compatibility oracle and rendezvous target. Its `Body/` implementation and `DollNetwork/` wire types are **not** a private API dependency for this repository.

If Embedded Body needs Core-internal knowledge to interoperate, stop and report a protocol/specification hole rather than importing Core internals or inventing wire semantics.

## Implementation shape

Use C++17 and the ESP-IDF framework to produce a library and example:

```
neondoll-embedded/
├── docs/
├── examples/
│   └── minimal/
├── host_test/
├── include/
│   └── neondoll/
├── src/
│   └── neondoll/
├── test/
├── BUILD_PLAN.md
├── README.md
└── CMakeLists.txt
```

Keep reusable behavior out of `main`. Prefer small files; roughly 300 lines is comfortable and 500 lines is a soft ceiling.

Persist durable Body state under an explicit configurable state directory. Secrets/private keys MUST NOT be logged.

## Architectural boundaries

- Body identity is stable across process, transport, endpoint, and session changes.
- Network reachability is not Body identity.
- Doll Network membership is not Doll Link authorization.
- Capability availability is not execution authority.
- WireGuard private keys remain Body-owned and never cross the wire.
- Pairing creates/authorizes the Body relationship; transport changes do not create a new Body.
- The Embedded Body does not perform Doll cognition.
- Body Reflex, if any, remains deterministic.
- Terminal input/output is embodiment interaction, not host command authority.
- Do not implement Core 5 semantic Body Resolver or model/session lifecycle work early.

## M0 — Project Setup and Skeleton

Set up the ESP-IDF + C++17 project/library skeleton, portable platform interfaces, minimal ESP32 example, host-test setup, and CI.

Deliver:

- A CMake-based ESP-IDF project that compiles for ESP32
- Portable C++17 interfaces for Body functionality (identity, terminal, etc.) in `include/neondoll/`
- ESP32-specific implementations in `src/neondoll/` behind thin adapters
- A minimal ESP32 example in `examples/minimal/` that blinks an LED and initializes the Body skeleton
- Host-based unit tests in `host_test/` that can run without ESP-IDF
- CI configuration (GitHub Actions) that builds the example and runs host tests

Acceptance proof:

- `idf.py build` succeeds in the minimal example
- Host tests pass (`make test` or equivalent)
- `gofmt`/`go vet` equivalent: `clang-format` and `cpplint` clean (or equivalent C++ formatting/linting)
- CI builds and tests on push to any branch

Do not implement Body identity, Wi-Fi/network lifecycle, WireGuard, pairing, Doll Link, or capabilities yet.

## Non-goals

- Second Doll Core
- Model/provider integration inside Body
- Doll Mind or autonomous cognition inside Body
- Auris/GUI work
- Flutter/mobile work
- General-purpose VPN product
- NAT hole punching / STUN / TURN for the Core 4 reference path
- Implicit arbitrary shell access
- Core 5 Body Resolver
- Silently compensating for missing public protocol

## Working rule for every milestone

Each milestone gets:

1. focused implementation
2. tests
3. `clang-format` (or equivalent)
4. `cpplint` (or equivalent)
5. `idf.py build` (for ESP32 targets)
6. host test execution
7. PR
8. green CI
9. concise note of any public-protocol assumptions or holes

Do not start the next milestone in the same PR.

## Closing principle

The Embedded Body should be simple enough that it can answer a very useful architectural question:

> Could somebody implement a real NeonDoll Body from the public protocols alone on an ESP32?

If the answer becomes “only if they know how Core works internally,” stop and fix the contract rather than teaching the Body private knowledge.