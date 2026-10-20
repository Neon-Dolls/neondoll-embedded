#!/usr/bin/env python3
"""Real Linux IPv6 UDP peer; echoes ESP queries and sends independent probes."""
import argparse
import errno
import ipaddress
import json
import selectors
import socket
import subprocess
import time
from protocol import MAX_PAYLOAD, OVERSIZE_PAYLOAD, packet, parse


def emit(**fields):
    print(json.dumps({"monotonic": round(time.monotonic(), 3), **fields}), flush=True)


def wg_metrics(interface):
    result = {}
    for field in ("latest-handshakes", "transfer"):
        run = subprocess.run(["wg", "show", interface, field], capture_output=True, text=True, check=True)
        rows = [line.split()[1:] for line in run.stdout.splitlines()]
        result[field] = [[int(v) for v in row] for row in rows]
    return result  # These explicit wg fields never contain private keys.


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--local", default="fd42:4e44:1::1")
    parser.add_argument("--device", default="fd42:4e44:2::2")
    parser.add_argument("--forbidden", default="fd42:4e44:1:0:8000::1")
    parser.add_argument("--interface", default="ndwgtest")
    parser.add_argument("--duration", type=float, default=600)
    args = parser.parse_args()
    for value in (args.local, args.device, args.forbidden):
        ipaddress.IPv6Address(value)
    if not 1 <= args.duration <= 86400:
        parser.error("duration must be 1..86400 seconds")
    target = (args.device, 4242)
    selector = selectors.DefaultSelector()
    counters = {"device_queries": 0, "linux_echo_ok": 0, "invalid": 0, "timeouts": 0,
                "forbidden_sent": 0, "forbidden_echo": 0, "oversize_rejected": 0,
                "oversize_accepted": 0, "send_errors": 0, "receive_errors": 0}
    pending = {}
    with socket.socket(socket.AF_INET6, socket.SOCK_DGRAM) as good, socket.socket(socket.AF_INET6, socket.SOCK_DGRAM) as bad:
        for sock, address in ((good, args.local), (bad, args.forbidden)):
            sock.setsockopt(socket.IPPROTO_IPV6, socket.IPV6_V6ONLY, 1)
            sock.setsockopt(socket.IPPROTO_IPV6, socket.IPV6_DONTFRAG, 1)
            sock.bind((address, 4242))
            sock.setblocking(False)
            selector.register(sock, selectors.EVENT_READ)
        start = time.monotonic()
        next_send = next_metrics = start
        sequence = 0
        while time.monotonic() - start < args.duration:
            now = time.monotonic()
            if now >= next_send:
                next_send = now + 1
                size = (64, 1232, MAX_PAYLOAD, OVERSIZE_PAYLOAD)[sequence % 4]
                sequence = (sequence + 1) & 0xFFFFFFFF
                data = packet("Q", "L", sequence, size)
                try:
                    good.sendto(data, target)
                    if size > MAX_PAYLOAD:
                        counters["oversize_accepted"] += 1
                    else:
                        pending[sequence] = (now, size)
                except OSError as error:
                    if size > MAX_PAYLOAD and error.errno == errno.EMSGSIZE:
                        counters["oversize_rejected"] += 1
                    else:
                        counters["send_errors"] += 1
                        emit(event="send_error", size=size, errno=error.errno)
                # Negative source probe: success means OS accepted it, NOT that filtering passed.
                try:
                    bad.sendto(packet("Q", "L", sequence, 64), target)
                    counters["forbidden_sent"] += 1
                except OSError:
                    counters["send_errors"] += 1
            for seq, (sent_at, _) in list(pending.items()):
                if now - sent_at > 3:
                    del pending[seq]
                    counters["timeouts"] += 1
            for key, _ in selector.select(0.1):
                try:
                    data, source = key.fileobj.recvfrom(2048)
                except BlockingIOError:
                    continue
                except OSError as error:
                    counters["receive_errors"] += 1
                    emit(event="receive_error", errno=error.errno)
                    continue
                try:
                    kind, origin, seq = parse(data)
                except ValueError:
                    counters["invalid"] += 1
                    continue
                if source[0] != str(ipaddress.IPv6Address(args.device)) or source[1] != 4242:
                    counters["invalid"] += 1
                elif key.fileobj is bad:
                    counters["forbidden_echo"] += 1
                elif kind == "Q" and origin == "D":
                    counters["device_queries"] += 1
                    try:
                        good.sendto(packet("E", "D", seq, len(data)), source)
                    except OSError:
                        counters["send_errors"] += 1
                elif kind == "E" and origin == "L" and seq in pending and pending[seq][1] == len(data):
                    counters["linux_echo_ok"] += 1
                    emit(event="linux_echo", size=len(data), rtt_ms=round((time.monotonic()-pending.pop(seq)[0])*1000, 3))
                else:
                    counters["invalid"] += 1
            if now >= next_metrics:
                next_metrics = now + 5
                emit(event="peer_metrics", **counters, **wg_metrics(args.interface))
        emit(event="peer_summary", **counters, **wg_metrics(args.interface), hardware_acceptance="review device log and captures")
    selector.close()


if __name__ == "__main__":
    main()
