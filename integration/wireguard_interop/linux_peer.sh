#!/usr/bin/env bash
# Dedicated lab interface only; no wg-quick, firewall, sysctl or default routes.
set -euo pipefail
iface=ndwgtest
if [[ $EUID != 0 ]]; then echo 'Run with sudo on the designated Linux lab peer.' >&2; exit 2; fi
case ${1:-} in
  up)
    [[ $# == 3 ]] || { echo 'Usage: linux_peer.sh up LINUX_PRIVATE_KEY_FILE ESP_PUBLIC_KEY_FILE' >&2; exit 2; }
    [[ -f $2 && -f $3 ]] || exit 2
    permissions=$(stat -c '%a' -- "$2")
    [[ $permissions == 600 || $permissions == 400 ]] || { echo 'Private key file must be mode 0600 or 0400.' >&2; exit 2; }
    if ip link show dev "$iface" >/dev/null 2>&1; then echo 'ndwgtest already exists; inspect it before proceeding.' >&2; exit 2; fi
    peer_public=$(cat -- "$3")
    [[ $peer_public =~ ^[A-Za-z0-9+/]{43}=$ ]] || { echo 'Invalid public key file.' >&2; exit 2; }
    ip link add dev "$iface" type wireguard
    trap 'ip link delete dev "$iface"' ERR
    wg set "$iface" listen-port 51820 private-key "$2" peer "$peer_public" \
      allowed-ips fd42:4e44:2::2/128 persistent-keepalive 15
    ip link set dev "$iface" mtu 1420 up
    ip -6 address add fd42:4e44:1::1/128 dev "$iface" nodad
    ip -6 address add fd42:4e44:1:0:8000::1/128 dev "$iface" nodad
    ip -6 route add fd42:4e44:2::2/128 dev "$iface"
    trap - ERR
    echo 'ndwgtest ready: UDP/IPv4 port 51820, IPv6 tunnel MTU 1420. No default routes changed.'
    ;;
  down)
    [[ $# == 1 ]] || exit 2
    ip link delete dev "$iface"
    ;;
  *) echo 'Usage: linux_peer.sh up LINUX_PRIVATE_KEY_FILE ESP_PUBLIC_KEY_FILE | down' >&2; exit 2 ;;
esac
