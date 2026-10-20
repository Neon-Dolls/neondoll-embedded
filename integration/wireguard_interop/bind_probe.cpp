#include "bind_probe.hpp"
#include <lwip/netif.h>
#include <lwip/tcpip.h>
#include <cerrno>
#include <cstring>
namespace interop {
int bind_probe_interface(int socket, const in6_addr& source, unsigned& index) {
    ifreq request{};
    unsigned selected = 0;
    bool ambiguous = false;
    LOCK_TCPIP_CORE();
    for (auto* n = netif_list; n; n = n->next) {
        if (n->name[0] != 'w' || n->name[1] != 'g' || !netif_is_up(n) || !netif_is_link_up(n)) continue;
        for (int i = 0; i < LWIP_IPV6_NUM_ADDRESSES; ++i) {
            if (ip6_addr_isvalid(netif_ip6_addr_state(n, i)) &&
                !std::memcmp(netif_ip6_addr(n, i)->addr, &source, 16)) {
                if (selected) ambiguous = true;
                selected = netif_get_index(n);
            }
        }
    }
    if (selected && !ambiguous) netif_index_to_name(selected, request.ifr_name);
    UNLOCK_TCPIP_CORE();
    index = 0;
    if (!selected || ambiguous || !request.ifr_name[0]) { errno = ENODEV; return -1; }
    // ESP-IDF/lwIP expects struct ifreq, not Linux's bare interface-name buffer.
    if (lwip_setsockopt(socket, SOL_SOCKET, SO_BINDTODEVICE, &request, sizeof(request))) return -1;
    index = selected; // Requested index, not getsockopt readback (unsupported here).
    return 0;
}
}
