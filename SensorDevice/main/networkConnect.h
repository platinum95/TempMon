
#include <stdint.h>
#include <stdbool.h>

#include "esp_wifi.h"
#include "esp_netif.h"

typedef struct NetworkCtx NetworkCtx;

struct NetworkCtx{
    bool driverInitialised;
    bool wifiConfigured;
    bool wifiStarted;
    bool initialised;
    bool connected;
    bool ipObtained;

    esp_netif_t *netif;
    wifi_config_t wifiConfig;

    esp_ip4_addr_t deviceAddress;

    uint8_t reconnectAttempt;

};


int connectToWifi( NetworkCtx *_ctx, const char *_ssid, const char * _psk,
                   bool _block, uint32_t _timeoutMs );