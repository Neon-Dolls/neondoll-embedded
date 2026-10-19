"""Bounded lab framing only; neither a production protocol nor crypto."""
import base64
import ipaddress
import json
import os
import stat
import struct

MAGIC = b"NDWG32C!"
MAX_PAYLOAD = 1372
OVERSIZE_PAYLOAD = 1373
COMMANDS = {"metrics", "tunnel_stop", "tunnel_start", "wifi_pause", "wifi_resume", "station_restart", "shutdown"}


def packet(kind, origin, sequence, size):
    if kind not in "QE" or origin not in "DL" or len(kind) != 1 or len(origin) != 1:
        raise ValueError("invalid header")
    if not 14 <= size <= OVERSIZE_PAYLOAD or not 0 <= sequence <= 0xFFFFFFFF:
        raise ValueError("invalid size/sequence")
    return MAGIC + kind.encode() + origin.encode() + struct.pack("!I", sequence) + bytes(
        (sequence + i) & 255 for i in range(14, size))


def parse(data):
    if not 14 <= len(data) <= OVERSIZE_PAYLOAD or data[:8] != MAGIC:
        raise ValueError("invalid packet")
    kind, origin, sequence = chr(data[8]), chr(data[9]), struct.unpack("!I", data[10:14])[0]
    if packet(kind, origin, sequence, len(data)) != data:
        raise ValueError("invalid packet pattern")
    return kind, origin, sequence


def config_line(path, unix_time):
    """Read a private regular file; errors deliberately contain no config values."""
    try:
        fd = os.open(path, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK)
        with os.fdopen(fd) as source:
            info = os.fstat(source.fileno())
            if not stat.S_ISREG(info.st_mode) or info.st_mode & 0o077 or info.st_size > 1024:
                raise ValueError()
            obj = json.load(source)
        fields = {"wifi_ssid", "wifi_password", "private_key", "peer_public_key", "endpoint_ipv4",
                  "endpoint_port", "local_ipv6", "peer_ipv6", "allowed_prefix", "allowed_length"}
        if set(obj) != fields or not all(isinstance(obj[k], str) for k in fields - {"endpoint_port", "allowed_length"}):
            raise ValueError()
        if not 1 <= len(obj["wifi_ssid"].encode()) <= 32 or len(obj["wifi_password"].encode()) > 64:
            raise ValueError()
        if any("\x00" in v for v in obj.values() if isinstance(v, str)):
            raise ValueError()
        keys = [base64.b64decode(obj[k], validate=True) for k in ("private_key", "peer_public_key")]
        if any(len(k) != 32 or not any(k) for k in keys) or keys[0] == keys[1]:
            raise ValueError()
        endpoint = ipaddress.IPv4Address(obj["endpoint_ipv4"])
        if int(endpoint) >> 24 == 0 or int(endpoint) >> 24 >= 224 or endpoint.is_loopback or endpoint.is_link_local:
            raise ValueError()
        for k in ("endpoint_port", "allowed_length"):
            if type(obj[k]) is not int:
                raise ValueError()
        if not 1 <= obj["endpoint_port"] <= 65535 or not 1 <= obj["allowed_length"] <= 128:
            raise ValueError()
        local, peer = (ipaddress.IPv6Address(obj[k]) for k in ("local_ipv6", "peer_ipv6"))
        allowed = ipaddress.IPv6Network(f'{obj["allowed_prefix"]}/{obj["allowed_length"]}', strict=True)
        if any(a.is_unspecified or a.is_multicast or a.is_link_local or a.is_loopback or a.ipv4_mapped or int(a) >> 32 == 0
               for a in (local, peer, allowed.network_address)) or local == peer or peer not in allowed:
            raise ValueError()
        if not 1577836800 <= int(unix_time) <= 4102444800:
            raise ValueError()
        obj.update(command="configure", unix_time=int(unix_time))
        data = json.dumps(obj, separators=(",", ":"), ensure_ascii=True).encode()
        if len(data) > 1024:
            raise ValueError()
        return data + b"\n"
    except (OSError, ValueError, TypeError, KeyError, UnicodeError):
        raise ValueError("invalid private configuration file (values suppressed)") from None
