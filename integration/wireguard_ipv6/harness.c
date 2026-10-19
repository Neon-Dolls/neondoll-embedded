// Test boundary: real patched upstream C, real lwIP pbufs, real libsodium/AEAD.
// OS/netif/UDP/time/RNG delivery is simulated; this is not a peer handshake.
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include "harness.h"
#define ip_input nd_capture_input
#include "wireguardif.c"
#undef TAG
#define wireguard_peer_index nd_api_peer_index
#include "esp_wireguard.c"
#undef wireguard_peer_index

static struct netif underlay;
struct netif *netif_default = &underlay;
static uint8_t wire[2048], delivered[2048];
static size_t wire_length, delivered_length;
static int outer4;
static uint64_t nonce_counter;
static uint8_t session_key[32];
static char address_text[64], prefix_text[64], mask_text[64];
static wireguard_config_t config;

uint32_t wireguard_sys_now(void) { return 1000; }
void wireguard_random_bytes(void *bytes, size_t size) { memset(bytes, 0x19, size); }
void wireguard_tai64n_now(uint8_t *out) { memset(out, 0, 12); }
bool wireguard_is_under_load(void) { return false; }
esp_err_t wireguard_platform_init(void) { return ESP_OK; }
const char *esp_err_to_name(int e) { (void)e; return "test"; }
esp_netif_t *esp_netif_get_handle_from_ifkey(const char *key) { (void)key; static esp_netif_t n; return &n; }
esp_err_t esp_netif_get_netif_impl_name(esp_netif_t *n, char *name) { (void)n; strcpy(name, "st0"); return ESP_OK; }
struct netif *netif_find(const char *name) { (void)name; return &underlay; }
void netif_set_up(struct netif *n) { n->flags |= NETIF_FLAG_UP; }
void netif_set_link_up(struct netif *n) { n->flags |= NETIF_FLAG_LINK_UP; }
void netif_set_link_down(struct netif *n) { n->flags &= ~NETIF_FLAG_LINK_UP; }
void netif_set_default(struct netif *n) { netif_default = n; }
void netif_remove(struct netif *n) { (void)n; }
void netif_ip6_addr_set(struct netif *n, s8_t i, const ip6_addr_t *a) { ip_addr_copy_from_ip6(n->ip6_addr[i], *a); }
void netif_ip6_addr_set_state(struct netif *n, s8_t i, u8_t state) { n->ip6_addr_state[i] = state; }
struct netif *netif_add(struct netif *n, const ip4_addr_t *a, const ip4_addr_t *m,
    const ip4_addr_t *g, void *state, netif_init_fn init, netif_input_fn input) {
    memset(n, 0, sizeof(*n)); n->state = state; n->input = input;
    ip4_addr_copy(*ip_2_ip4(&n->ip_addr), *a);
    ip4_addr_copy(*ip_2_ip4(&n->netmask), *m);
    ip4_addr_copy(*ip_2_ip4(&n->gw), *g);
    return init(n) == ERR_OK ? n : NULL;
}
struct udp_pcb *udp_new_ip_type(u8_t type) { (void)type; return calloc(1, sizeof(struct udp_pcb)); }
err_t udp_bind(struct udp_pcb *p, const ip_addr_t *a, u16_t port) { (void)p;(void)a;(void)port;return ERR_OK; }
void udp_bind_netif(struct udp_pcb *p, const struct netif *n) { (void)p;(void)n; }
void udp_disconnect(struct udp_pcb *p) { (void)p; }
void udp_remove(struct udp_pcb *p) { free(p); }
void udp_recv(struct udp_pcb *p, udp_recv_fn recv, void *arg) { p->recv=recv;p->recv_arg=arg; }
err_t udp_sendto_if(struct udp_pcb *p, struct pbuf *q, const ip_addr_t *ip, u16_t port, struct netif *n) {
    (void)p;(void)port;assert(n == &underlay);
    outer4=IP_IS_V4(ip);wire_length=q->tot_len;assert(wire_length<=sizeof(wire));
    pbuf_copy_partial(q,wire,wire_length,0);return ERR_OK;
}
void sys_timeout(u32_t ms, sys_timeout_handler h, void *arg) { (void)ms;(void)h;(void)arg; }
void sys_untimeout(sys_timeout_handler h, void *arg) { (void)h;(void)arg; }
err_t ip_input(struct pbuf *q, struct netif *n) {
    assert(n==wg_netif);delivered_length=q->tot_len;assert(delivered_length<=sizeof(delivered));
    pbuf_copy_partial(q,delivered,delivered_length,0);pbuf_free(q);return ERR_OK;
}
err_t dns_gethostbyname(const char *name, ip_addr_t *a, dns_found_callback cb, void *arg) {
    (void)cb;(void)arg;return ipaddr_aton(name,a)?ERR_OK:ERR_ARG;
}
int mbedtls_base64_decode(unsigned char *out,size_t max,size_t *len,const unsigned char *in,size_t size) {
    (void)size;*len=max;return wireguard_base64_decode((const char *)in,out,len)?0:-1;
}
int nd_setup(const char *address,const char *prefix,const char *mask) {
    strcpy(address_text,address);strcpy(prefix_text,prefix);strcpy(mask_text,mask);
    memset(&config,0,sizeof(config));config.private_key="ERERERERERERERERERERERERERERERERERERERERERE=";
    config.public_key="IiIiIiIiIiIiIiIiIiIiIiIiIiIiIiIiIiIiIiIiIiI=";
    config.address=address_text;config.netmask="ffff:ffff:ffff:ffff::";
    config.allowed_ip=prefix_text;config.allowed_ip_mask=mask_text;
    config.endpoint="192.0.2.1";config.port=51820;ipaddr_aton(config.endpoint,&config.endpoint_ip);
    delivered_length=wire_length=0;outer4=0;
    int result=esp_wireguard_netif_create(&config);if(result!=ESP_OK)return result;
    struct wireguard_device *device=wg_netif->state;
    result=esp_wireguard_peer_init(&config,&peer);if(result!=ESP_OK)return result;
    u8_t index=0;result=wireguardif_add_peer(wg_netif,&peer,&index);if(result!=ERR_OK)return result;
    struct wireguard_peer *remote=&device->peers[index];
    struct wireguard_keypair *key=&remote->curr_keypair;key->valid=key->initiator=key->receiving_valid=true;
    key->local_index=7;key->remote_index=7;key->keypair_millis=1000;
    memset(session_key,0x45,sizeof(session_key));memcpy(key->sending_key,session_key,32);memcpy(key->receiving_key,session_key,32);
    key->sending_counter=1;nonce_counter=0;
    return 0;
}
void nd_cleanup(void) {
    if(wg_netif){wireguardif_shutdown(wg_netif);wireguardif_fini(wg_netif);wg_netif=NULL;}
}
int nd_transmit(const uint8_t *data,size_t length,int chained) {
    struct pbuf *q=pbuf_alloc(PBUF_RAW,chained?17:length,PBUF_RAM);assert(q);pbuf_take(q,data,q->tot_len);
    if(chained){struct pbuf *tail=pbuf_alloc(PBUF_RAW,length-17,PBUF_RAM);assert(tail);pbuf_take(tail,data+17,length-17);pbuf_cat(q,tail);}
    ip6_addr_t dest;memset(&dest,0,sizeof(dest));if(length>=40)memcpy(dest.addr,data+24,16);
    int result=wg_netif->output_ip6(wg_netif,q,&dest);pbuf_free(q);return result;
}
int nd_receive(const uint8_t *data,size_t length,int tamper,size_t padding) {
    uint8_t encrypted[2048];assert(length+padding+32<=sizeof(encrypted));memset(encrypted,0,sizeof(encrypted));
    struct message_transport_data *message=(void *)encrypted;message->receiver=7;
    ++nonce_counter;U64TO8_LITTLE(message->counter,nonce_counter);
    uint8_t plain[2048]={0};memcpy(plain,data,length);
    struct wireguard_keypair sending={0};memcpy(sending.sending_key,session_key,32);sending.sending_counter=nonce_counter;
    wireguard_encrypt_packet(message->enc_packet,plain,length+padding,&sending);
    if(tamper)message->enc_packet[0]^=1;
    delivered_length=0;struct wireguard_device *device=wg_netif->state;
    wireguardif_process_data_message(device,&device->peers[0],message,length+padding+16,&device->peers[0].ip,51820);
    return delivered_length!=0;
}
size_t nd_received(uint8_t *out) { memcpy(out,delivered,delivered_length);return delivered_length; }
int nd_outer_ipv4(void) { return outer4; }
int nd_default_unchanged(void) { return netif_default==&underlay && esp_wireguard_set_default(NULL)==ESP_ERR_INVALID_STATE && esp_wireguard_restore_default(NULL)==ESP_ERR_INVALID_STATE; }
int nd_address_correct(const char *text) {
    ip_addr_t ip;ipaddr_aton(text,&ip);return ip6_addr_cmp(netif_ip6_addr(wg_netif,0),ip_2_ip6(&ip)) &&
        netif_ip6_addr_state(wg_netif,0)==IP6_ADDR_PREFERRED && !netif_ip6_addr_isstatic(wg_netif,0) && ip4_addr_isany(ip_2_ip4(&wg_netif->ip_addr));
}
int nd_output_registered(void) { return wg_netif && wg_netif->output_ip6!=NULL; }
int nd_add_allowed(const char *prefix,const char *mask) {
    ip_addr_t p,m;ipaddr_aton(prefix,&p);ipaddr_aton(mask,&m);
    struct wireguard_device *device=wg_netif->state;return peer_add_ip(&device->peers[0],p,m);
}

int nd_receive_wire(void) {
    struct pbuf *p=pbuf_alloc(PBUF_RAW,wire_length,PBUF_RAM);assert(p);
    pbuf_take(p,wire,wire_length);delivered_length=0;
    struct wireguard_device *device=wg_netif->state;
    wireguardif_network_rx(device,device->udp_pcb,p,&device->peers[0].ip,51820);
    return delivered_length!=0;
}
