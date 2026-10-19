// Test boundary: real patched upstream C, real lwIP pbufs, real libsodium/AEAD.
// OS/netif/UDP/time/RNG delivery is simulated; this is not a peer handshake.
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include "harness.h"
#include "wireguard.h"
#include "crypto.h"
#include "lwip/mem.h"
#include "lwip/timeouts.h"
static const char *fault = "";
static int core_depth, require_core, devices, udps, interfaces, timers, zeroizations, decoded_wipes;
static int present = 1, platform_live;
static uint32_t fake_now = 1000;
static struct netif *latest;
static sys_timeout_handler saved_timer;
static void *saved_timer_arg;
static void require_lock(void) { assert(!require_core || core_depth == 1); }
void *nd_device_calloc(size_t n, size_t size);
void nd_device_free(void *p);
bool nd_device_init(struct wireguard_device *d, const uint8_t *key);
void nd_crypto_zero(void *data, size_t size);
#undef mem_calloc
#define mem_calloc nd_device_calloc
#undef mem_free
#define mem_free nd_device_free
#define free nd_device_free
#define wireguard_device_init nd_device_init
#define crypto_zero nd_crypto_zero
#define wireguardif_add_peer nd_real_add_peer
#define wireguardif_add_allowed_ip nd_real_add_allowed
#define wireguardif_connect nd_real_connect
err_t nd_capture_input(struct pbuf *q,struct netif *n);
#define ip_input nd_capture_input
#include "wireguardif.c"
#undef free
#undef mem_free
#undef mem_calloc
#define mem_calloc calloc
#undef wireguard_device_init
#undef crypto_zero
#undef wireguardif_add_peer
#undef wireguardif_add_allowed_ip
#undef wireguardif_connect
err_t wireguardif_add_peer(struct netif *n, struct wireguardif_peer *p, u8_t *i) {
    require_lock();return !strcmp(fault,"peer")?ERR_MEM:nd_real_add_peer(n,p,i);
}
err_t wireguardif_add_allowed_ip(struct netif *n,u8_t i,ip_addr_t p,ip_addr_t m) {
    require_lock();return !strcmp(fault,"allowed")?ERR_MEM:nd_real_add_allowed(n,i,p,m);
}
err_t wireguardif_connect(struct netif *n,u8_t i) {
    require_lock();return !strcmp(fault,"connect")?ERR_IF:nd_real_connect(n,i);
}
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

uint32_t wireguard_sys_now(void) { return fake_now; }
void wireguard_random_bytes(void *bytes, size_t size) { memset(bytes, 0x19, size); }
void wireguard_tai64n_now(uint8_t *out) { memset(out, 0, 12); }
bool wireguard_is_under_load(void) { return false; }
esp_err_t wireguard_platform_init(void) { require_lock();if(!strcmp(fault,"platform"))return ESP_FAIL;platform_live=1;return ESP_OK; }
void wireguard_platform_deinit(void) { require_lock();platform_live=0; }
const char *esp_err_to_name(int e) { (void)e; return "test"; }
esp_netif_t *esp_netif_get_handle_from_ifkey(const char *key) { (void)key; static esp_netif_t n; return present?&n:NULL; }
esp_err_t esp_netif_get_netif_impl_name(esp_netif_t *n, char *name) { (void)n; assert(core_depth == 0); strcpy(name, "st0"); return ESP_OK; }
struct netif *netif_find(const char *name) { (void)name; require_lock();return present?&underlay:NULL; }
void netif_set_up(struct netif *n) { require_lock(); n->flags |= NETIF_FLAG_UP; }
void netif_set_link_up(struct netif *n) { n->flags |= NETIF_FLAG_LINK_UP; }
void netif_set_link_down(struct netif *n) { n->flags &= ~NETIF_FLAG_LINK_UP; }
void netif_set_default(struct netif *n) { netif_default = n; }
void netif_remove(struct netif *n) { require_lock();assert(interfaces>0);--interfaces;if(latest==n)latest=NULL; }
void netif_ip6_addr_set(struct netif *n, s8_t i, const ip6_addr_t *a) { ip_addr_copy_from_ip6(n->ip6_addr[i], *a); }
void netif_ip6_addr_set_state(struct netif *n, s8_t i, u8_t state) { n->ip6_addr_state[i] = state; }
struct netif *netif_add(struct netif *n, const ip4_addr_t *a, const ip4_addr_t *m,
    const ip4_addr_t *g, void *state, netif_init_fn init, netif_input_fn input) {
    require_lock();if(!strcmp(fault,"netif"))return NULL;
    memset(n, 0, sizeof(*n)); n->num=1;n->state = state; n->input = input;
    ip4_addr_copy(*ip_2_ip4(&n->ip_addr), *a);
    ip4_addr_copy(*ip_2_ip4(&n->netmask), *m);
    ip4_addr_copy(*ip_2_ip4(&n->gw), *g);
    if(init(n)!=ERR_OK)return NULL;latest=n;++interfaces;return n;
}
struct udp_pcb *udp_new_ip_type(u8_t type) { (void)type;require_lock();if(!strcmp(fault,"udp"))return NULL;++udps;return calloc(1, sizeof(struct udp_pcb)); }
err_t udp_bind(struct udp_pcb *p, const ip_addr_t *a, u16_t port) { (void)p;(void)a;(void)port;require_lock();return !strcmp(fault,"bind")?ERR_IF:ERR_OK; }
void udp_bind_netif(struct udp_pcb *p, const struct netif *n) { require_lock();p->netif_idx=netif_get_index(n); }
void udp_disconnect(struct udp_pcb *p) { (void)p; }
void udp_remove(struct udp_pcb *p) { require_lock();assert(udps>0);--udps;free(p); }
void udp_recv(struct udp_pcb *p, udp_recv_fn recv, void *arg) { p->recv=recv;p->recv_arg=arg; }
err_t udp_sendto_if(struct udp_pcb *p, struct pbuf *q, const ip_addr_t *ip, u16_t port, struct netif *n) {
    (void)p;(void)port;assert(n == &underlay);
    outer4=IP_IS_V4(ip);wire_length=q->tot_len;assert(wire_length<=sizeof(wire));
    pbuf_copy_partial(q,wire,wire_length,0);return ERR_OK;
}
void sys_timeout(u32_t ms, sys_timeout_handler h, void *arg) { (void)ms;require_lock();assert(timers==0);++timers;saved_timer=h;saved_timer_arg=arg; }
void sys_untimeout(sys_timeout_handler h, void *arg) { require_lock();assert(saved_timer==h && saved_timer_arg==arg);timers=0;saved_timer=NULL;saved_timer_arg=NULL; }
err_t ip_input(struct pbuf *q, struct netif *n) {
    assert(n==latest);delivered_length=q->tot_len;assert(delivered_length<=sizeof(delivered));
    pbuf_copy_partial(q,delivered,delivered_length,0);pbuf_free(q);return ERR_OK;
}
err_t dns_gethostbyname(const char *name, ip_addr_t *a, dns_found_callback cb, void *arg) {
    (void)cb;(void)arg;return ipaddr_aton(name,a)?ERR_OK:ERR_ARG;
}
int mbedtls_base64_decode(unsigned char *out,size_t max,size_t *len,const unsigned char *in,size_t size) {
    (void)size;*len=max;return wireguard_base64_decode((const char *)in,out,len)?0:-1;
}
int nd_setup(const char *address,const char *prefix,const char *mask) {
    underlay.flags=NETIF_FLAG_UP|NETIF_FLAG_LINK_UP;
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
    if(wg_netif){wireguardif_shutdown(wg_netif);netif_remove(wg_netif);wireguardif_fini(wg_netif);wg_netif=NULL;}
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

// Test the protocol-length helper independently of lwIP's 16-bit pbuf/MTU limits.
size_t nd_validate_ip6_length(const uint8_t *header,size_t bytes,int padded) {
    return nd_ip6_length(header,bytes,padded!=0);
}

struct netif *netif_get_by_index(u8_t index) { require_lock();return present && index==netif_get_index(&underlay)?&underlay:NULL; }
void nd_core_lock(void) { assert(core_depth==0);++core_depth; }
void nd_core_unlock(void) { assert(core_depth==1);--core_depth; }
void *nd_device_calloc(size_t n,size_t size) {
    require_lock();if(!strcmp(fault,"device_alloc"))return NULL;void *p=calloc(n,size);if(p)++devices;return p;
}
void nd_device_free(void *p) {
    require_lock();const uint8_t *bytes=p;for(size_t i=0;i<sizeof(struct wireguard_device);++i)assert(bytes[i]==0);
    assert(devices>0);--devices;free(p);
}
bool nd_device_init(struct wireguard_device *d,const uint8_t *key) {
    require_lock();if(!strcmp(fault,"device_init")){memset(d->private_key,0x44,32);return false;}
    return wireguard_device_init(d,key);
}
void nd_crypto_zero(void *data,size_t size) {
    crypto_zero(data,size);if(size==32)++decoded_wipes;
}
void mbedtls_platform_zeroize(void *data,size_t size) {
    volatile uint8_t *out=data;for(size_t i=0;i<size;++i)out[i]=0;++zeroizations;
}
int mbedtls_base64_encode(unsigned char *out,size_t max,size_t *len,const unsigned char *in,size_t size) {
    require_lock();if(!strcmp(fault,"base64")){memset(out,0x55,max);return -1;}
    *len=max;return wireguard_base64_encode(in,size,(char *)out,len)?0:-1;
}
void nd_backend_reset(void) {
    assert(!devices&&!udps&&!interfaces&&!timers&&!platform_live);require_core=1;fault="";present=1;
    zeroizations=decoded_wipes=0;fake_now=1000;underlay.flags=NETIF_FLAG_UP|NETIF_FLAG_LINK_UP;
    ip4addr_aton("192.0.2.2",ip_2_ip4(&underlay.ip_addr));
}
void nd_backend_fault(const char *name) { fault=name; }
void nd_backend_clock(uint32_t now) { fake_now=now; }
int nd_backend_resources(void) { return devices+udps+interfaces+timers+platform_live; }
int nd_backend_zeroizations(void) { return zeroizations; }
int nd_backend_decoded_wipes(void) { return decoded_wipes; }
void nd_backend_underlay(int enabled) { present=enabled; }
void nd_backend_peer_keys(void) {
    nd_core_lock();struct wireguard_device *device=latest->state;
    struct wireguard_keypair *k=&device->peers[0].curr_keypair;memset(k,0,sizeof(*k));
    k->valid=k->receiving_valid=k->initiator=true;k->local_index=k->remote_index=7;k->keypair_millis=fake_now;
    memset(session_key,0x45,32);memcpy(k->sending_key,session_key,32);memcpy(k->receiving_key,session_key,32);
    nonce_counter=0;nd_core_unlock();
}
int nd_backend_authenticate(int tamper) {
    uint8_t data[48]={0x60};data[5]=8;data[6]=59;data[7]=64;data[8]=data[24]=0xfd;data[23]=1;data[39]=2;
    nd_core_lock();wg_netif=latest;int result=nd_receive(data,sizeof(data),tamper,0);wg_netif=NULL;nd_core_unlock();return result;
}
int nd_backend_tick(void) {
    if(!saved_timer)return 0;nd_core_lock();sys_timeout_handler h=saved_timer;void *arg=saved_timer_arg;
    timers=0;saved_timer=NULL;saved_timer_arg=NULL;h(arg);nd_core_unlock();return 1;
}

err_t ip4_input(struct pbuf *p,struct netif *n) { return nd_capture_input(p,n); }
err_t ip6_input(struct pbuf *p,struct netif *n) { return nd_capture_input(p,n); }
int nd_backend_configured(void) {
    nd_core_lock();struct wireguard_device *d=latest->state;
    const uint8_t *a=(void *)netif_ip6_addr(latest,0)->addr,*b=(void *)netif_ip6_addr(latest,1)->addr;
    const uint8_t *m=(void *)ip_2_ip6(&d->peers[0].allowed_source_ips[1].mask)->addr;
    const uint8_t *p=(void *)ip_2_ip6(&d->peers[0].allowed_source_ips[1].ip)->addr;
    int result=a[0]==0xfd && a[15]==2 && b[15]==3 && !netif_ip6_addr_isstatic(latest,0) &&
        m[7]==0xff && m[8]==0x80 && m[9]==0 && p[7]==1 && IP_IS_V4(&d->peers[0].ip) && d->peers[0].port==51820;
    nd_core_unlock();return result;
}
