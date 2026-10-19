#include "esp_err.h"
typedef struct esp_netif { int unused; } esp_netif_t;
esp_netif_t *esp_netif_get_handle_from_ifkey(const char *key);
esp_err_t esp_netif_get_netif_impl_name(esp_netif_t *netif, char *name);
