// Actual lwIP socket -> netconn -> UDP -> IPv6 -> netif output path.
// Only OS waiting/thread creation and final NIC output are simulated.
#include "../bind_probe.hpp"
#include <lwip/init.h>
#include <lwip/tcpip.h>
#include <lwip/ip6.h>
#include <lwip/priv/nd6_priv.h>
#include <lwip/udp.h>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <cerrno>
static netif underlay{}, wg{};
static unsigned underlay_packets, wg_packets, ipv4_packets;
static err_t capture(netif* n,pbuf* p,const ip6_addr_t*) {
    assert(p->tot_len>=48);
    if(n==&wg)++wg_packets;else {assert(n==&underlay);++underlay_packets;}
    return ERR_OK;
}
static err_t capture4(netif* n,pbuf* p,const ip4_addr_t*) {
    assert(n==&underlay&&p->tot_len>=28);++ipv4_packets;return ERR_OK;
}
static err_t initialize(netif* n) { n->output_ip6=capture;n->output=capture4;n->mtu=1420;return ERR_OK; }
static void add(netif& n,const char* name,const char* address) {
    assert(netif_add_noaddr(&n,nullptr,initialize,ip_input));n.name[0]=name[0];n.name[1]=name[1];
    ip6_addr_t a{};assert(ip6addr_aton(address,&a));netif_ip6_addr_set(&n,0,&a);
    netif_ip6_addr_set_valid_life(&n,0,IP6_ADDR_LIFE_INFINITE);
    netif_ip6_addr_set_pref_life(&n,0,IP6_ADDR_LIFE_INFINITE);
    netif_ip6_addr_set_state(&n,0,IP6_ADDR_PREFERRED);netif_set_up(&n);netif_set_link_up(&n);
}
static sockaddr_in6 addr(const char* value) {
    sockaddr_in6 a{};a.sin6_len=sizeof(a);a.sin6_family=AF_INET6;a.sin6_port=lwip_htons(4242);
    assert(lwip_inet_pton(AF_INET6,value,&a.sin6_addr)==1);return a;
}
static int open_source(const sockaddr_in6& source) {
    int s=lwip_socket(AF_INET6,SOCK_DGRAM,IPPROTO_UDP);assert(s>=0);
    assert(lwip_bind(s,reinterpret_cast<const sockaddr*>(&source),sizeof(source))==0);return s;
}
static int send(int s,const sockaddr_in6& destination,const char* packet) {
    return lwip_sendto(s,packet,std::strlen(packet),0,reinterpret_cast<const sockaddr*>(&destination),sizeof(destination));
}
int main() {
    // Core-locking APIs execute synchronously; lwIP's test sys_arch creates no OS thread.
    lwip_init();assert(sys_mutex_new(&lock_tcpip_core)==ERR_OK);
    LOCK_TCPIP_CORE();
    add(underlay,"st","fd42:4e44:9::9");add(wg,"wg","fd42:4e44:2::2");netif_set_default(&underlay);
    ip4_addr_t ip{},mask{},gateway{};
    assert(ip4addr_aton("192.0.2.2",&ip)&&ip4addr_aton("255.255.255.0",&mask)&&ip4addr_aton("192.0.2.1",&gateway));
    netif_set_addr(&underlay,&ip,&mask,&gateway);
    // Simulate a competing RA prefix, without modifying either interface's IPv6 addresses.
    assert(ip6addr_aton("fd42:4e44:1::",&prefix_list[0].prefix));
    prefix_list[0].netif=&underlay;prefix_list[0].invalidation_timer=600;
    UNLOCK_TCPIP_CORE();
    auto source=addr("fd42:4e44:2::2"),peer=addr("fd42:4e44:1::1");
    auto ordinary=addr("fd42:4e44:9::9"),unrelated=addr("fd42:4e44:8::8");
    int s=open_source(source);
    ip6_addr_t source_ip{}, peer_ip{};
    std::memcpy(source_ip.addr,&source.sin6_addr,16);std::memcpy(peer_ip.addr,&peer.sin6_addr,16);
    LOCK_TCPIP_CORE();assert(ip6_route(&source_ip,&peer_ip)==&underlay);UNLOCK_TCPIP_CORE();
    // Wrong route fails local-source validation; source bind alone cannot send.
    assert(send(s,peer,"source-only")<0);assert(underlay_packets==0&&wg_packets==0);
    assert(lwip_close(s)==0);
    s=lwip_socket(AF_INET6,SOCK_DGRAM,IPPROTO_UDP);assert(s>=0);
    unsigned index=0;assert(interop::bind_probe_interface(s,source.sin6_addr,index)==0);
    // Match production order: device binding first, then source-address bind.
    assert(lwip_bind(s,reinterpret_cast<const sockaddr*>(&source),sizeof(source))==0);
    assert(index==netif_get_index(&wg));
    assert(send(s,peer,"probe")>0);assert(send(s,peer,"reply")>0);
    assert(wg_packets==2&&underlay_packets==0); // Removing the real socket option fails here.
    unsigned invalid_index=99;
    assert(interop::bind_probe_interface(-1,source.sin6_addr,invalid_index)<0&&invalid_index==0&&errno==EBADF);
    int other=open_source(ordinary);
    assert(interop::bind_probe_interface(other,ordinary.sin6_addr,invalid_index)<0&&errno==ENODEV);
    assert(send(other,unrelated,"ordinary")>0);assert(underlay_packets==1&&wg_packets==2);
    int v4=lwip_socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);assert(v4>=0);
    sockaddr_in destination4{};destination4.sin_len=sizeof(destination4);
    destination4.sin_family=AF_INET;destination4.sin_port=lwip_htons(4242);
    assert(lwip_inet_pton(AF_INET,"192.0.2.10",&destination4.sin_addr)==1);
    assert(lwip_sendto(v4,"IPv4",4,0,reinterpret_cast<sockaddr*>(&destination4),sizeof(destination4))==4);
    assert(ipv4_packets==1&&lwip_close(v4)==0);
    assert(netif_default==&underlay&&!netif_ip6_addr_isstatic(&wg,0));
    // No implicit WG /64: an unbound socket targeting a neighbor of its own address uses underlay.
    auto neighbor=addr("fd42:4e44:2::7");assert(send(other,neighbor,"no implicit prefix")>0);
    assert(underlay_packets==2&&wg_packets==2);
    // Missing bound netif must fail, never fall through to the available default route.
    LOCK_TCPIP_CORE();netif_remove(&wg);UNLOCK_TCPIP_CORE();
    assert(send(s,peer,"stale")<0);assert(underlay_packets==2&&wg_packets==2);
    assert(lwip_close(s)==0);
    assert(interop::bind_probe_interface(other,source.sin6_addr,index)<0&&index==0);
    LOCK_TCPIP_CORE();add(wg,"wg","fd42:4e44:2::2");UNLOCK_TCPIP_CORE();
    s=lwip_socket(AF_INET6,SOCK_DGRAM,IPPROTO_UDP);assert(s>=0);
    assert(interop::bind_probe_interface(s,source.sin6_addr,index)==0);
    assert(lwip_bind(s,reinterpret_cast<const sockaddr*>(&source),sizeof(source))==0);
    assert(send(s,peer,"rebound")>0);assert(wg_packets==3&&underlay_packets==2);
    assert(lwip_close(s)==0&&lwip_close(other)==0);
    LOCK_TCPIP_CORE();netif_remove(&wg);netif_remove(&underlay);UNLOCK_TCPIP_CORE();
    sys_mutex_free(&lock_tcpip_core);assert(udp_pcbs==nullptr);
    std::puts("PASS: real source-route shadow/rejection, interface-bound probes/replies, unrelated traffic, no /64/default changes, stale binding and restart");
}
