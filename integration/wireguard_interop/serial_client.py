#!/usr/bin/env python3
"""Load RAM-only test config without echo, then capture device evidence to stdout."""
import argparse
import json
import sys
import time
from protocol import COMMANDS, config_line


def schedule(value):
    try:
        when, command = value.split(":", 1)
        when = float(when)
        if not 0 <= when <= 86400 or command not in COMMANDS:
            raise ValueError()
        return when, command
    except ValueError:
        raise argparse.ArgumentTypeError("use seconds:command from the documented command list") from None


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True)
    parser.add_argument("--config", required=True, help="mode 0600 private JSON outside the checkout")
    parser.add_argument("--duration", type=float, default=600)
    parser.add_argument("--at", action="append", type=schedule, default=[], help="seconds after config:command")
    args = parser.parse_args()
    if not 1 <= args.duration <= 86400:
        parser.error("duration must be 1..86400 seconds")
    # Validate without emitting credentials. Python/OS buffers cannot promise zeroization.
    config_line(args.config, time.time())
    import serial  # pyserial, intentionally not required by host unit tests
    with serial.Serial(args.port, 115200, timeout=0.2, write_timeout=2, exclusive=True) as port:
        port.dtr = False
        port.rts = False
        configured_at = None
        deadline = time.monotonic() + 60
        commands = sorted(args.at)
        buffer = bytearray()
        while True:
            now = time.monotonic()
            if configured_at is None and now >= deadline:
                raise SystemExit("No await_config event; press EN/reset with this client connected")
            if configured_at is not None:
                if now - configured_at >= args.duration:
                    port.write(b'{"command":"metrics"}\n')
                    port.write(b'{"command":"shutdown"}\n')
                    # Drain the cleanup metrics; no fabricated pass/fail verdict.
                    end = time.monotonic() + 2
                    while time.monotonic() < end:
                        data = port.readline(2048)
                        if data.startswith(b"NDWG "):
                            print(data.decode(errors="replace").rstrip(), flush=True)
                    break
                while commands and commands[0][0] <= now - configured_at:
                    _, cmd = commands.pop(0)
                    port.write(json.dumps({"command": cmd}).encode() + b"\n")
            data = port.read(256)
            buffer.extend(data)
            if len(buffer) > 4096:
                buffer.clear()  # bounded log framing, ignore SDK/boot chatter
            while b"\n" in buffer:
                raw, _, rest = buffer.partition(b"\n")
                buffer[:] = rest
                if not raw.startswith(b"NDWG "):
                    continue
                try:
                    item = json.loads(raw[5:])
                except (ValueError, UnicodeError):
                    continue
                print(raw.decode(errors="replace"), flush=True)
                if item.get("event") in {"configuration_rejected", "configure_failed", "cleanup_failed", "identity_failed"}:
                    raise SystemExit("Device setup failed; inspect numeric events above (configuration values suppressed)")
                if item.get("event") == "await_config" and configured_at is None:
                    port.write(config_line(args.config, time.time()))
                    configured_at = time.monotonic()
        print(json.dumps({"event": "capture_finished", "hardware_acceptance": "manual evidence review required"}))


if __name__ == "__main__":
    try:
        main()
    except ValueError as error:
        print(str(error), file=sys.stderr)
        raise SystemExit(2)
