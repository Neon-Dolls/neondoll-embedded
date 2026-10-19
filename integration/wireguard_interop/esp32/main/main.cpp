// Opt-in lab app. No embedded credentials, key persistence or production protocol.
#include <neondoll/esp32_platform.hpp>
#include <neondoll/esp32_wireguard.hpp>
#include "../../packet.hpp"
#include <cJSON.h>
#include <driver/uart.h>
#include <esp_vfs_dev.h>
#include <esp_heap_caps.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <lwip/sockets.h>
#include <lwip/tcpip.h>
#include <lwip/ip6.h>
#include <lwip/udp.h>
#include <mbedtls/base64.h>
#include <mbedtls/platform_util.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <cmath>

#if LWIP_IPV6_FRAG || LWIP_IPV6_REASS
#error "Interop MTU tests require inner IPv6 fragmentation/reassembly disabled"
#endif
namespace {
neondoll::Esp32Storage storage;
neondoll::Esp32EntropySource entropy;
neondoll::Esp32Clock clock_source;
neondoll::Esp32Logger logger;
neondoll::Esp32NetworkAvailability network;
neondoll::NetworkSupervisor recovery{network,clock_source};
neondoll::Esp32WireGuardBackend backend;
neondoll::WireGuardTransport tunnel{network,backend};
neondoll::PlatformContext context;
struct Config {
    char ssid[33]{}, password[65]{};
    neondoll::WireGuardConfig wg;
    sockaddr_in6 local{}, peer{};
    void clear() {
        mbedtls_platform_zeroize(ssid,sizeof(ssid));
        mbedtls_platform_zeroize(password,sizeof(password)); wg.clear();
    }
} config;
bool configured=false, finished=false, pending=false, tcpip_ready=false;
int probe=-1;
uint32_t sequence=0, pending_sequence=0;
size_t pending_size=0;
uint64_t sent=0, echo_ok=0, linux_requests=0, unexpected_rx=0, bad_rx=0, send_errors=0;
uint64_t boundary_sent=0, boundary_echo=0, oversized_rejected=0, oversized_accepted=0;
uint8_t tx[interop::oversize_payload]{}, rx[interop::oversize_payload+1]{};
int64_t next_probe=0, next_metrics=0;
char line[1025]{};size_t line_size=0;bool overflow=false;

void event(const char* name,int result=0) {
    std::printf("NDWG {\"event\":\"%s\",\"result\":%d,\"ms\":%lld}\n",name,result,
        static_cast<long long>(esp_timer_get_time()/1000));
}
void close_probe() { if(probe>=0){close(probe);probe=-1;}pending=false; }
void metrics() {
    unsigned count=0, wg=0, udp=0;bool default_wg=false, bound=false, unbound=false;
    // Read-only inspection: no routes or netif state are changed here.
    if(tcpip_ready) {
        LOCK_TCPIP_CORE();
        for(auto* n=netif_list;n;n=n->next){++count;if(n->name[0]=='w'&&n->name[1]=='g')++wg;}
        for(auto* p=udp_pcbs;p;p=p->next)++udp;
        auto is_wg=[](netif* n){return n&&n->name[0]=='w'&&n->name[1]=='g';};
        default_wg=is_wg(netif_default);
        ip6_addr_t src{},dest{};
        std::memcpy(src.addr,config.wg.addresses[0].address.data(),16);
        std::memcpy(dest.addr,&config.peer.sin6_addr,16);
        bound=is_wg(ip6_route(&src,&dest));unbound=is_wg(ip6_route(IP6_ADDR_ANY6,&dest));
        UNLOCK_TCPIP_CORE();
    }
    std::printf("NDWG {\"event\":\"metrics\",\"ms\":%lld,\"wifi\":%d,\"epoch\":%llu,"
        "\"tunnel\":%d,\"ready\":%d,\"heap\":%u,\"heap_min\":%u,\"largest\":%u,\"owner_stack_min_bytes\":%u,"
        "\"resource_sample_valid\":%d,\"netifs\":%u,\"wg_netifs\":%u,\"udp_pcbs\":%u,\"default_wg\":%d,\"bound_peer_wg\":%d,\"unbound_peer_wg\":%d,"
        "\"device_sent\":%llu,\"device_echo_ok\":%llu,\"linux_requests\":%llu,\"unexpected_rx\":%llu,\"bad_rx\":%llu,"
        "\"send_errors\":%llu,\"boundary_sent\":%llu,\"boundary_echo\":%llu,\"oversized_rejected\":%llu,\"oversized_accepted\":%llu}\n",
        static_cast<long long>(esp_timer_get_time()/1000),network.available(),
        static_cast<unsigned long long>(network.availability_epoch()),int(tunnel.status()),
        backend.status()==neondoll::WireGuardBackend::State::up,
        unsigned(heap_caps_get_free_size(MALLOC_CAP_8BIT)),unsigned(heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT)),
        unsigned(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)),unsigned(uxTaskGetStackHighWaterMark(nullptr)),
        tcpip_ready,count,wg,udp,default_wg,bound,unbound,
        sent,echo_ok,linux_requests,unexpected_rx,bad_rx,send_errors,boundary_sent,boundary_echo,
        oversized_rejected,oversized_accepted);
}
// Reject nested JSON before invoking the recursive parser. Input is at most 1 KiB.
bool flat(const char* input) {
    unsigned depth=0;bool quoted=false,escape=false;
    for(const char* p=input;*p;++p) {
        if(quoted){if(escape)escape=false;else if(*p=='\\')escape=true;else if(*p=='"')quoted=false;continue;}
        if(*p=='"')quoted=true;
        else if(*p=='['||*p==']')return false;
        else if(*p=='{' && ++depth>1)return false;
        else if(*p=='}'){if(!depth)return false;--depth;}
    }
    return !depth&&!quoted;
}
struct Json {
    cJSON* value;
    ~Json() {
        if(!value)return;
        for(auto* p=value->child;p;p=p->next) {
            if(p->valuestring)mbedtls_platform_zeroize(p->valuestring,std::strlen(p->valuestring));
        }
        cJSON_Delete(value);
    }
};
const char* text(cJSON* obj,const char* key,size_t maximum) {
    auto* p=cJSON_GetObjectItemCaseSensitive(obj,key);
    return cJSON_IsString(p)&&std::strlen(p->valuestring)<=maximum?p->valuestring:nullptr;
}
bool number(cJSON* obj,const char* key,double low,double high,double& out) {
    auto* p=cJSON_GetObjectItemCaseSensitive(obj,key);
    if(!cJSON_IsNumber(p)||!std::isfinite(p->valuedouble)||p->valuedouble<low||p->valuedouble>high||
        std::floor(p->valuedouble)!=p->valuedouble)return false;
    out=p->valuedouble;return true;
}
bool configure(cJSON* root) {
    Config candidate;
    struct Wipe {Config& c;~Wipe(){c.clear();}} wipe{candidate};
    const char* ssid=text(root,"wifi_ssid",32),*password=text(root,"wifi_password",64);
    const char* private_key=text(root,"private_key",44),*public_key=text(root,"peer_public_key",44);
    const char* endpoint=text(root,"endpoint_ipv4",15),*local=text(root,"local_ipv6",45);
    const char* peer=text(root,"peer_ipv6",45),*prefix=text(root,"allowed_prefix",45);
    double port,length,unix_time;
    if(!ssid||!*ssid||!password||!private_key||!public_key||!endpoint||!local||!peer||!prefix||
        !number(root,"endpoint_port",1,65535,port)||!number(root,"allowed_length",1,128,length)||
        !number(root,"unix_time",1577836800.,4102444800.,unix_time))return false;
    size_t decoded=0;
    auto decode=[&decoded](const char* source,std::array<uint8_t,32>& key){
        return std::strlen(source)==44&&mbedtls_base64_decode(key.data(),key.size(),&decoded,
            reinterpret_cast<const unsigned char*>(source),44)==0&&decoded==32;};
    if(!decode(private_key,candidate.wg.private_key)||!decode(public_key,candidate.wg.peer_public_key))return false;
    if(inet_pton(AF_INET,endpoint,candidate.wg.endpoint.address.data())!=1||
        inet_pton(AF_INET6,local,candidate.wg.addresses[0].address.data())!=1||
        inet_pton(AF_INET6,prefix,candidate.wg.allowed_ips[0].address.data())!=1||
        inet_pton(AF_INET6,peer,&candidate.peer.sin6_addr)!=1)return false;
    candidate.wg.endpoint.port=uint16_t(port);candidate.wg.address_count=candidate.wg.allowed_ip_count=1;
    candidate.wg.addresses[0].length=128;candidate.wg.allowed_ips[0].length=uint8_t(length);
    if(!candidate.wg.valid()||!std::memcmp(&candidate.peer.sin6_addr,candidate.wg.addresses[0].address.data(),16))return false;
    const auto* peer_bytes=reinterpret_cast<const uint8_t*>(&candidate.peer.sin6_addr);
    for(unsigned bit=0;bit<unsigned(length);++bit)
        if((peer_bytes[bit/8]^(candidate.wg.allowed_ips[0].address[bit/8]))&(0x80u>>(bit%8)))return false;
    std::strcpy(candidate.ssid,ssid);std::strcpy(candidate.password,password);
    candidate.local.sin6_family=candidate.peer.sin6_family=AF_INET6;
    candidate.local.sin6_port=candidate.peer.sin6_port=htons(4242);
    std::memcpy(&candidate.local.sin6_addr,candidate.wg.addresses[0].address.data(),16);
    timeval now{time_t(unix_time),0};if(settimeofday(&now,nullptr))return false;
    config=candidate;
    int result=tunnel.init(config.wg);
    // Only the transport retains the private key needed for future starts.
    mbedtls_platform_zeroize(config.wg.private_key.data(),config.wg.private_key.size());
    if(!result)result=network.start(config.ssid,config.password,0);
    if(!result){tcpip_ready=true;result=recovery.start();}
    if(!result)result=tunnel.start();
    if(result) {
        tunnel.deinit();recovery.stop();const int cleanup=network.stop();
        config.clear();finished=true;event("configure_failed",result);
        if(cleanup)event("cleanup_failed",cleanup);
        return false;
    }
    configured=true;event("configured");return true;
}
void command() {
    if(std::memchr(line,0,line_size)||!flat(line)){event("invalid_input");return;}
    Json json{cJSON_ParseWithLengthOpts(line,line_size+1,nullptr,true)};
    if(!cJSON_IsObject(json.value)){event("invalid_input");return;}
    for(auto* a=json.value->child;a;a=a->next)for(auto* b=a->next;b;b=b->next)
        if(a->string&&b->string&&!std::strcmp(a->string,b->string)){event("invalid_input");return;}
    const char* cmd=text(json.value,"command",24);
    if(!cmd){event("invalid_input");return;}
    if(!std::strcmp(cmd,"configure")) {
        if(configured||finished||!configure(json.value))event("configuration_rejected");
        return;
    }
    if(!configured||finished){event("not_running");return;}
    int result=0;
    if(!std::strcmp(cmd,"metrics")){}
    else if(!std::strcmp(cmd,"tunnel_stop")){close_probe();result=tunnel.stop();}
    else if(!std::strcmp(cmd,"tunnel_start"))result=tunnel.start();
    else if(!std::strcmp(cmd,"wifi_pause")){close_probe();result=recovery.disconnect();}
    else if(!std::strcmp(cmd,"wifi_resume"))result=recovery.request_recovery();
    else if(!std::strcmp(cmd,"station_restart")) {
        close_probe();result=tunnel.stop();if(!result)result=recovery.stop();
        if(!result)result=network.start(config.ssid,config.password,0);
        if(!result)result=recovery.start();
        if(!result)result=tunnel.start();
    } else if(!std::strcmp(cmd,"shutdown")) {
        close_probe();result=tunnel.deinit();const int stop=recovery.stop();if(!result)result=stop;
        config.clear();finished=true;
    } else {event("invalid_command");return;}
    event(cmd,result);metrics();
}
void input() {
    char bytes[128];const auto n=read(STDIN_FILENO,bytes,sizeof(bytes));
    for(int i=0;i<n;++i){const char c=bytes[i];if(c=='\r')continue;
        if(c=='\n'){
            if(overflow)event("input_too_long");else{line[line_size]=0;command();}
            mbedtls_platform_zeroize(line,sizeof(line));line_size=0;overflow=false;
        }else if(line_size<sizeof(line)-1&&!overflow)line[line_size++]=c;else overflow=true;
    }
    mbedtls_platform_zeroize(bytes,sizeof(bytes));
}
void packets(int64_t now) {
    if(tunnel.status()!=neondoll::TunnelState::up){close_probe();return;}
    if(probe<0) {
        probe=socket(AF_INET6,SOCK_DGRAM,IPPROTO_UDP);
        if(probe<0){event("socket_failed",errno);return;}
        if(bind(probe,reinterpret_cast<sockaddr*>(&config.local),sizeof(config.local))||
            fcntl(probe,F_SETFL,O_NONBLOCK)){event("bind_failed",errno);close_probe();return;}
        event("socket_bound");
    }
    for(unsigned budget=0;budget<8;++budget) {
        sockaddr_in6 from{};socklen_t length=sizeof(from);
        const int n=recvfrom(probe,rx,sizeof(rx),0,reinterpret_cast<sockaddr*>(&from),&length);
        if(n<0)break;
        if(std::memcmp(&from.sin6_addr,&config.peer.sin6_addr,16)||from.sin6_port!=htons(4242)){
            ++unexpected_rx;event("unexpected_source");continue;}
        uint32_t seq;
        if(!interop::parse(rx,n,seq)){++bad_rx;continue;}
        if(rx[8]=='Q'&&rx[9]=='L') {
            ++linux_requests;rx[8]='E';
            if(sendto(probe,rx,n,0,reinterpret_cast<sockaddr*>(&config.peer),sizeof(config.peer))!=n)++send_errors;
        } else if(rx[8]=='E'&&rx[9]=='D'&&pending&&seq==pending_sequence&&size_t(n)==pending_size) {
            ++echo_ok;if(size_t(n)==interop::max_payload)++boundary_echo;pending=false;
        } else ++bad_rx;
    }
    if(now<next_probe)return;
    next_probe=now+1000000;
    const size_t sizes[]={64,1232,interop::max_payload,interop::oversize_payload};
    const size_t size=sizes[sequence%4];interop::make(tx,size,'Q','D',++sequence);
    const int result=sendto(probe,tx,size,0,reinterpret_cast<sockaddr*>(&config.peer),sizeof(config.peer));
    if(size==interop::oversize_payload) {
        if(result<0) {
            ++oversized_rejected;event("mtu_oversize_rejected",errno);
        } else {++oversized_accepted;event("mtu_unexpected_accept");}
        pending=false;
    } else if(result==int(size)) {
        ++sent;if(size==interop::max_payload)++boundary_sent;
        pending=true;pending_sequence=sequence;pending_size=size;
    } else {++send_errors;event("send_failed",errno);pending=false;}
}
}
extern "C" void app_main() {
    context.storage=&storage;context.entropy=&entropy;context.clock=&clock_source;
    context.network=&network;context.logger=&logger;
    const int init=neondoll_init(&context);if(init){event("identity_failed",init);return;}
    // M1 entropy/bootstrap has completed before Wi-Fi or WireGuard initialization.
    if(uart_driver_install(UART_NUM_0,2048,0,0,nullptr,0)!=ESP_OK){event("uart_failed");return;}
    esp_vfs_dev_uart_use_driver(UART_NUM_0);
    if(fcntl(STDIN_FILENO,F_SETFL,O_NONBLOCK)){event("uart_nonblock_failed");return;}
    metrics();event("await_config");
    auto previous=neondoll::TunnelState::uninitialized;
    for(;;) {
        input();
        if(configured&&!finished) {
            recovery.poll();const int status=tunnel.poll();if(status)event("tunnel_error",status);
            const auto current=tunnel.status();if(current!=previous){event("tunnel_state",int(current));previous=current;}
            packets(esp_timer_get_time());
        }
        const int64_t now=esp_timer_get_time();
        if(configured&&now>=next_metrics){metrics();next_metrics=now+5000000;}
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
