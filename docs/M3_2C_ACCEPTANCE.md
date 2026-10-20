# M3.2c — interoperability harness and physical acceptance

**Hardware acceptance: PENDING. Real ESP32/Linux interoperability evidence: NONE.**
No USB/serial ESP32 or remote board was available during implementation. This
change supplies a runnable lab harness and acceptance procedure. Builds, host
packet tests, sanitizers and CI do not establish a real handshake, RF behavior,
resource stability on silicon, or full M3.2 completion.

Read [M3.2b](M3_2B_ACCEPTANCE.md) for the unchanged backend/ownership contract and
[M3.2a](M3_2A_IPV6.md) for dependency preparation. The embedded design's real IPv6
and route-isolation gate remains in force. These are dedicated **test peer keys**;
never use an active Body/Core key for a probe (it could roam the live endpoint).
No crypto source, production interface, Relay, pairing or provisioning is changed.

## What is runnable

`integration/wireguard_interop/esp32` is a separate, explicitly enabled classic
ESP32 application. It completes M1 identity/bootstrap entropy first, then waits
for a bounded, flat JSON line over UART0 at 115200 baud. Wi-Fi passwords and raw
WireGuard keys arrive only over this trusted local lab connection; they are not
in sdkconfig, source, firmware, NVS, logs or CI artifacts. The backend and transport
hold their normal RAM copies. JSON/input buffers and backend-owned key copies are
wiped on cleanup; `shutdown` also clears the retained Wi-Fi credentials. The host
private JSON file and Python/OS buffers remain sensitive and are not promised to
be securely erased. UART is plaintext: use an isolated, trusted lab machine and
do not record transmit traffic or enable serial echo/debug/coredumps.

The `tunnel` state codes are 0 uninitialized, 1 stopped, 2 waiting for network,
3 starting, 4 up and 5 cleanup pending; `ready` is sampled independently from
the authenticated backend. Nonzero command results are failures to investigate.

The owner task polls M2 then M3, binds an IPv6 UDP socket explicitly to the
WireGuard netif **and** assigned tunnel source on port 4242, and exchanges bounded
patterned probes in both directions.
Only the existing backend's authenticated state can report `ready=1`. Wi-Fi alone
is insufficient. The app does not install routes or change the default netif.
Read-only metrics include default, source-hint and unbound IPv6 route selection.
`source_route_peer_wg` (formerly `bound_peer_wg`) describes `ip6_route`, not the
socket's enforced interface; it may be zero when an underlay RA shadows that hint.
`probe_ifindex` is the requested interface index after successful socket-option
setup, not kernel readback or evidence of physical transmission.
`resource_sample_valid=0` means TCP/IP has not yet been initialized by a successful
station start; zero netif/UDP fields at that stage are not cleanup evidence. Once
initialized, sampling remains enabled after stop/shutdown so retained resources
cannot be hidden by a stopped station flag.

The Linux helper creates only a dedicated `ndwgtest` interface, two `/128` lab
addresses and one explicit ESP32 `/128` route. It never uses wg-quick, changes
forwarding/firewall/sysctl policy, or installs a default route. Review the script
before running it with sudo. Do not run it where these lab addresses/interface
name or UDP port 51820 are already in use.

## Exact socket routing mechanism and limits

ESP-IDF 5.2.1's `components/lwip/lwip/src/core/ipv6/ip6.c:ip6_route` considers
on-link and router-announced routes **before** its unscoped source-address fallback.
Binding only the ULA source therefore does not force the WG netif. In the regression
fixture, an underlay RA prefix wins; UDP then rejects the source/netif mismatch
with `ERR_RTE`. Do not treat source binding as guaranteed egress or add a static
WG address (which would create an unwanted implicit `/64`) to work around it.

The shared test-app helper `bind_probe_interface` finds exactly one up/link-up
`wg` netif with the configured, valid IPv6 source under the TCP/IP core lock.
It resolves the actual lwIP interface name, releases the lock, and calls
`lwip_setsockopt(fd, SOL_SOCKET, SO_BINDTODEVICE, &ifreq, sizeof(ifreq))`. This
SDK expects **`struct ifreq`**, unlike Linux's bare name-buffer convention. The
socket implementation (`src/api/sockets.c:lwip_setsockopt_impl`) calls
`udp_bind_netif`, recording the interface index in the PCB. `src/core/udp.c`'s
send path checks that index first, bypassing `ip6_route` for that socket. Both
probes and replies use the same bound FD. Missing/ambiguous netif or option failure
closes the FD before any packet is sent; there is no unbound fallback.

`IPV6_MULTICAST_IF` only controls multicast and is not a unicast fix. A ULA's
`sin6_scope_id` is not a replacement for this device binding. No global routing
hook, prefix, default router or default-interface change is installed. Other
sockets retain their normal routing. Socket-option setup acquires the SDK's core
lock internally, so it is called **outside** the helper's lookup lock.

This is an index binding, not a lifetime lease or authenticated-readiness check.
The application/backend remain owned by one task: it opens the FD only when the
transport is up, closes it on stop/loss/epoch teardown, and creates/rebinds a new
FD after recovery. Do not retain it across netif removal/index reuse or invoke the
helper concurrently with backend destruction. `SO_BINDTODEVICE` also narrows UDP
receive delivery to that interface. It does not bypass WireGuard AllowedIPs or
provide path-MTU discovery. Physical egress still requires the checklist below.

Host regression `interop_socket_egress` compiles unmodified socket, netconn, UDP,
IPv6, ND6 and netif code, including the **exact ESP-IDF 5.2.1 lwIP sources in CI**.
It captures actual `netif->output_ip6` calls and checks a competing RA prefix,
source-only failure, enforced probe/reply egress, unrelated IPv4/IPv6 traffic, no implicit
WG `/64` or default changes, missing/stale interfaces and restart rebinding. Only
the synchronous OS port and final NIC output boundary are simulated; there is
no stubbed route, socket option, UDP send or IPv6 output function. Removing the
socket-option call in a temporary negative-control build made the real probe
send assertion fail. This is a host routing regression, not RF/peer acceptance.

Run that regression separately with IDF 5.2.1 exported:

```sh
cmake -S integration/wireguard_interop/routing -B /tmp/ndwg-routing \
  -D INTEROP_LWIP_SOURCE_DIR="$IDF_PATH/components/lwip/lwip" \
  -DCMAKE_C_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer" \
  -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer"
cmake --build /tmp/ndwg-routing
UBSAN_OPTIONS=halt_on_error=1 ASAN_OPTIONS=detect_leaks=1 ctest --test-dir /tmp/ndwg-routing --output-on-failure
```

The ordinary host build also runs it against the supplied `LWIP_SOURCE_DIR`;
`INTEROP_LWIP_SOURCE_DIR` can select a separate exact SDK tree. Its OS test port
uses lwIP's synchronous unit-test semaphores/mailboxes with normal invalid-handle
initialization, so it tests packet routing rather than FreeRTOS scheduling.

## Build and flash (ESP-IDF 5.2.1)

Prepare the exact archive with the checked patch series, in a directory whose
basename is `wireguard`, following M3.2a. From the repository root with IDF 5.2.1
exported:

```sh
idf.py -C integration/wireguard_interop/esp32 -B /tmp/ndwg-interop \
  -D SDKCONFIG=/tmp/ndwg-interop/sdkconfig \
  -D NEONDOLL_WG_INTEROP=ON -D WIREGUARD_SOURCE_DIR=/tmp/wireguard build
idf.py -C integration/wireguard_interop/esp32 -B /tmp/ndwg-interop \
  -p /dev/ttyUSB0 flash
```

The explicit CMake opt-in is required. Defaults select classic ESP32, IPv6/core
locking, an 8 KiB owner task and disable inner IPv6 fragmentation/reassembly and
coredumps. Tunnel MTU remains the backend's **1420**, not a substituted IPv4 tunnel.
No credentials are needed to build/flash. Do not erase NVS to make identity errors
go away. The test keys are reloaded from the same private host file after reboot;
this harness does not add durable WireGuard key storage.

## Prepare a real Linux peer and private host configuration

Install WireGuard tools, iproute2, Python 3 and pyserial (serial host only). Use a
LAN-reachable Linux IPv4 address for the endpoint; `192.0.2.1` in the example is a
placeholder, not a usable endpoint. Permit inbound UDP 51820 on the lab host through
your existing firewall policy. No public Internet or Relay is required.

Create files **outside the checkout**, without printing private keys:

```sh
umask 077
LAB="$HOME/.local/share/neondoll-wg-lab"
mkdir -p "$LAB"
wg genkey > "$LAB/linux.key"
wg pubkey < "$LAB/linux.key" > "$LAB/linux.pub"
wg genkey > "$LAB/esp.key"
wg pubkey < "$LAB/esp.key" > "$LAB/esp.pub"
```

Each side needs only its own private key and the opposite public key. When hosts
differ, generate the Linux keypair on the Linux peer and the ESP keypair on the
serial host (run only the corresponding commands above on each). Copy only `esp.pub` to the Linux peer and only
`linux.pub` to the serial host; retain each private file on its owning lab host.
Generate `device.local.json` on the serial host using the example's public values:

```sh
python3 - "$LAB" <<'PY'
import getpass, json, os, pathlib, sys
lab = pathlib.Path(sys.argv[1])
cfg = json.loads(pathlib.Path('integration/wireguard_interop/config.example.json').read_text())
cfg.update(wifi_ssid=getpass.getpass('Lab Wi-Fi SSID: '),
           wifi_password=getpass.getpass('Lab Wi-Fi password: '),
           private_key=(lab/'esp.key').read_text().strip(),
           peer_public_key=(lab/'linux.pub').read_text().strip(),
           endpoint_ipv4=input('Linux LAN IPv4: ').strip())
fd = os.open(lab/'device.local.json', os.O_WRONLY|os.O_CREAT|os.O_EXCL, 0o600)
with os.fdopen(fd,'w') as out:
    json.dump(cfg,out)
PY
```

Keep the supplied IPv6 addressing for the first reproduction:

| Purpose | Value |
|---|---|
| ESP tunnel source | `fd42:4e44:2::2/128` |
| Linux allowed source | `fd42:4e44:1::1/128` |
| ESP peer AllowedIPs | `fd42:4e44:1::/65` |
| Linux forbidden source | `fd42:4e44:1:0:8000::1/128` (outside that `/65`) |
| Linux peer AllowedIPs | `fd42:4e44:2::2/128` |

On the Linux peer, capture routes first and create the dedicated interface:

```sh
ip -j route show table all > "$LAB/routes4.before.json"
ip -j -6 route show table all > "$LAB/routes6.before.json"
sudo bash integration/wireguard_interop/linux_peer.sh up "$LAB/linux.key" "$LAB/esp.pub"
sudo tcpdump -ni YOUR_LAN_INTERFACE udp port 51820 -w "$LAB/outer.pcap"
```

Run tcpdump in a separate terminal. It captures outer encrypted traffic; protect
captures/logs as lab evidence. Never collect `wg showconf`, `wg show all dump` or
private configuration in an artifact: those commands expose private keys.

## Execute and collect evidence

Start the Linux peer program before loading the ESP configuration:

```sh
sudo python3 integration/wireguard_interop/peer.py --duration 1260 > "$LAB/peer.log"
```

On the serial host, start the client and press the ESP32 EN/reset button if needed
so it sees `await_config`. It sends configuration without printing it, injects the
host's current Unix time for WireGuard TAI64N, and saves only device result events:

```sh
python3 integration/wireguard_interop/serial_client.py \
  --port /dev/ttyUSB0 --config "$LAB/device.local.json" --duration 1200 \
  --at 420:tunnel_stop --at 425:tunnel_start \
  --at 480:tunnel_stop --at 485:tunnel_start \
  --at 540:wifi_pause --at 550:wifi_resume \
  --at 660:station_restart --at 720:metrics > "$LAB/device.log"
```

Schedules are relative to configuration transmission. All lifecycle calls stay
on the same owner task. `tunnel_stop` cancels tunnel intent; `wifi_pause` intentionally
pauses M2; `wifi_resume` explicitly requests recovery. `station_restart` stops the
tunnel and station before rebuilding both, exercising the removal/rebinding path.
The client sends `shutdown` at the end and drains cleanup metrics. Startup errors
terminate the client; neither tool emits an automatic hardware-acceptance verdict.
For 50+ repeated cycles, pass additional `--at seconds:command` pairs with time for
handshake/traffic between them; compare equivalent stopped snapshots, not startup
against a live session. Device counters are cumulative across these cycles.

Also physically power off/isolate the AP for at least 60 seconds during a stable
window, then restore it. This is distinct from intentional pause. Verify M2
backoff, tunnel teardown, reacquisition and resumed bidirectional packets. Keep an
uninterrupted traffic window of at least 360 seconds before perturbations to
observe rekeying independently of restarts. Linux keepalive is 15 seconds; it
permits the backend to observe authenticated received traffic even while idle.

## Physical acceptance checklist — all items still pending

Record board/chip revision, MAC-free lab identifier, power supply, ESP-IDF version,
firmware commit/hash/size, pinned dependency revision, Linux kernel/WireGuard tool
versions, AP model/settings, test duration and timestamps. Attach public logs,
route snapshots and capture hashes; never attach keys or private JSON.

1. **Handshake/readiness:** Linux `latest-handshakes` advances from zero and outer
   capture shows an IPv4 UDP exchange. Record `ready=0` before authenticated
   activity and `ready=1` after actual received AEAD traffic. Wi-Fi availability
   or interface initialization alone must never pass. Repeat with a mismatched
   peer public key (a separate private JSON file): no authenticated readiness or
   successful echoes. Restore the correct test config by reset, not live probing.
2. **Both directions:** device `device_echo_ok` and Linux `linux_echo_ok` increase;
   ESP `linux_requests` and Linux `device_queries` independently increase. Retain
   multiple patterned exchanges, not only ICMP or a handshake timestamp.
3. **AllowedIPs:** ordinary Linux probes succeed while the forbidden source probes
   are sent over the same authenticated peer. Confirm Linux transfer/capture
   evidence during injection and ESP `unexpected_rx=0`, `bad_rx=0`; positive
   controls succeed immediately before/after. No echo by itself is insufficient:
   an egress filter could hide an ingress filtering bug. The app records forbidden
   sources reaching its socket even if the return packet would be blocked.
4. **MTU:** UDP payloads 64, 1232 and **1372** succeed in both directions; the latter
   plus IPv6/UDP headers is exactly 1420. Both tools also attempt 1373. With inner
   fragmentation disabled and Linux `IPV6_DONTFRAG`, record synchronous rejection
   (`oversized_rejected` increasing, `oversized_accepted=0`). Review each ESP
   `mtu_oversize_rejected` errno against the successful boundary-size probes;
   unrelated socket/network errors do not establish MTU rejection. ESP `boundary_echo` must increase. This does not establish
   arbitrary underlay path-MTU discovery; test a lower-MTU WAN separately if needed.
5. **Route isolation:** while up, ESP `probe_ifindex` identifies the live WG
   interface, `unbound_peer_wg=0` and `default_wg=0`. `source_route_peer_wg` is a
   diagnostic hint and need not be one under a competing RA; actual probes/replies
   must still traverse WG. Confirm with WG/underlay captures and successful traffic,
   including a lab underlay RA that would win source-only route selection.
   Before/after route diffs on Linux contain only the dedicated lab
   addresses/routes; no default route changes. Verify an unrelated IPv4/IPv6
   destination still selects the original host interface with `ip route get` and
   `ip -6 route get`. Do not change host routes to force a passing result.
6. **Recovery/stop:** intentional stop remains stopped until start; no packets or
   WG netif persist during the gap. AP loss drops readiness, then recovery restores
   real traffic. Station recreation/repeated starts bind a fresh underlay and do
   not resurrect the old tunnel. Compare counts/logs/captures through each cycle.
7. **Rekey:** at least two distinct Linux handshake timestamps during one stable
   360-second window, increasing transfer counts, continued bidirectional echoes
   and no invalid authenticated readiness. Restart-generated handshakes do not
   prove rekeying. Repeat longer to detect timestamp/counter problems.
8. **Resources:** capture free/minimum/largest heap, owner stack low-water, netif
   and UDP PCB counts after startup, authenticated operation and each stop. After
   `tunnel_stop`, `wg_netifs=0` and closed-probe/UDP counts return to an equivalent
   warmed Wi-Fi-only baseline. After shutdown verify stopped backend and no WG
   resources. Run 50+ cycles: investigate any monotonic free-heap/largest-block loss
   or falling stack margin. Minimum-ever heap naturally cannot recover. Record
   TCP/IP/event-task stack/heap tracing separately on hardware if required; the
   built-in stack metric covers only the owner task. Counters are evidence, not a
   proof of zero remaining secret bytes or absence of every allocation leak.

Remove only the dedicated interface after stopping both tools/capture:

```sh
sudo bash integration/wireguard_interop/linux_peer.sh down
ip -j route show table all > "$LAB/routes4.after.json"
ip -j -6 route show table all > "$LAB/routes6.after.json"
```

## Executed evidence

- Hardware discovery included a host check outside the filesystem sandbox:
  no `/dev/ttyUSB*`, `/dev/ttyACM*` or serial-by-id node was present. The host USB
  inventory contained no identified ESP32 or serial/debug adapter; no remote board
  was supplied. Physical flash/run: **not run**.
- Real Linux/ESP handshake, encrypted IPv6 exchange, RF recovery, rekey and device
  heap/stack/resource measurements: **not run**, all pending the checklist above.
- All eleven host CTest suites passed: the existing 159 C++ tests plus nine
  Python tests (168 total), and the real socket-egress regression above.
  ASan/UBSan with leak detection passed the suites (Python itself is not
  sanitizer-instrumented).
- The opt-in classic ESP32 application built and linked using ESP-IDF 5.2.1;
  image size 931,344 bytes, with 11% of the default app partition free.
- Shell syntax and Python compilation checks passed. CI additionally builds the
  default/Wi-Fi/production-backend/probe configurations and QEMU identity regression.
  None of these execute physical interoperability tests.
