#!/usr/bin/env python3
"""Reproduce source-level blockers in the pinned ESPHome candidate; no traffic test."""
import argparse
import hashlib
from pathlib import Path
import re

REVISION = "9b7660fed27151abdcbf32b43861044cf4a32497"  # v0.4.8
FILES = {
    "src/esp_wireguard.c": "412bda8be1d8cf6d7142e9842beacca4e7e69cbd09dd8a2184aaedb32e107812",
    "src/wireguardif.c": "86054e15c8ce0d26b0160d2f8553b8527c7db889f1a82d824cbb192a4f6a9506",
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path, help="extracted upstream v0.4.8 source")
    parser.add_argument("--expect-blocked", action="store_true",
                        help="return success only when the pinned blockers are reproduced")
    args = parser.parse_args()
    sources = {}
    for name, digest in FILES.items():
        path = args.source / name
        if not path.is_file() or hashlib.sha256(path.read_bytes()).hexdigest() != digest:
            parser.exit(2, f"Source mismatch: {name}; use unmodified revision {REVISION}\n")
        sources[name] = path.read_text()
    interface = sources["src/wireguardif.c"]
    api = sources["src/esp_wireguard.c"]
    blockers = {
        "No IPv6 output callback is registered":
            "netif->output = wireguardif_output;" in interface
            and not re.search(r"(?:->|\.)\s*output_ip6\s*=", interface),
        "Tunnel creation coerces configured addresses to IPv4":
            re.search(r"netif_add\(\s*&wg_netif_struct,\s*ip_2_ip4\(&ip_addr\),", api) is not None
            and "netif_add_ip6_address" not in api,
        "IPv6 receive path bypasses allowed-prefix filtering and reads an IPv4 length":
            re.search(r"#if LWIP_IPV6\s*if \(IPH_V\(iphdr\) == 6\)\s*\{\s*"
                      r"// TODO: IPV6 support for route filtering\s*"
                      r"header_len = PP_NTOHS\(IPH_LEN\(iphdr\)\);\s*dest_ok = true;", interface) is not None,
        "Device teardown frees memory without wiping the device":
            re.search(r"void wireguardif_fini\([^}]+free\(device\);", interface) is not None
            and not re.search(r"void wireguardif_fini\([^}]+crypto_zero", interface),
    }
    print(f"ESPHome WireGuard v0.4.8 ({REVISION})")
    for description, reproduced in blockers.items():
        print(f"{'BLOCKED' if reproduced else 'NOT REPRODUCED'}: {description}")
    print("Source audit only. No handshake, encrypted packets or backend lifecycle proof.")
    if not all(blockers.values()):
        parser.exit(2, "Findings changed; review manually instead of accepting this backend.\n")
    return 0 if args.expect_blocked else 1


if __name__ == "__main__":
    raise SystemExit(main())
