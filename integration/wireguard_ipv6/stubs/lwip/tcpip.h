#ifndef ND_TCPIP_H
#define ND_TCPIP_H
#ifdef __cplusplus
extern "C" {
#endif
void nd_core_lock(void);
void nd_core_unlock(void);
#ifdef __cplusplus
}
#endif
#define LOCK_TCPIP_CORE() nd_core_lock()
#define UNLOCK_TCPIP_CORE() nd_core_unlock()
#endif
