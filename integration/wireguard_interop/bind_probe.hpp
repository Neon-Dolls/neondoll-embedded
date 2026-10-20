#ifndef ND_INTEROP_BIND_PROBE_HPP
#define ND_INTEROP_BIND_PROBE_HPP
#include <lwip/sockets.h>
namespace interop {
// Owner task only, while this tunnel epoch is alive. Never hold the core lock
// across this call: lwip_setsockopt acquires it internally. No routing changes.
int bind_probe_interface(int socket, const in6_addr& source, unsigned& index);
}
#endif
