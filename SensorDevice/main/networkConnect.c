#include "networkConnect.h"
#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"
#include "esp32/rom/ets_sys.h"
#include "esp_system.h"
#include "esp_sleep.h"
#include "esp_timer.h"
#include "string.h"
#include "esp_task_wdt.h"

static const char * logTag = "NetCon";
#define assertEspCall( call ) if( call != ESP_OK ) return 1;
#define assertEspCallMessage( call, msg ) if( call != ESP_OK ){ ESP_LOGE( logTag, msg ); return 1; }

/***************************
 * Network event callbacks
 ***************************/

static void upObtainedCallback( void *_arg, esp_event_base_t _eventBase, 
                           int32_t _eventId, void *_eventData ){
    volatile NetworkCtx *_ctx = (NetworkCtx*) _arg;
    ip_event_got_ip_t *eventData = (ip_event_got_ip_t*) _eventData;
    _ctx->deviceAddress = eventData->ip_info.ip;
    _ctx->ipObtained = true;
    ESP_LOGI( logTag, "Optained IP address: " IPSTR, IP2STR( &eventData->ip_info.ip ) );
}

static void wifiDisconnectedCallback( void *_arg, esp_event_base_t _eventBase, 
                              int32_t _eventId, void *_eventData ){
    volatile NetworkCtx *_ctx = (NetworkCtx*) _arg;
    _ctx->connected = false;
    ESP_LOGI( logTag, "Wifi disconnected" );
}

static void wifiConnectedCallback( void *_arg, esp_event_base_t _eventBase, 
                           int32_t _eventId, void *_eventData ){
    volatile NetworkCtx *_ctx = (NetworkCtx*) _arg;
    _ctx->connected = true;
    ESP_LOGI( logTag, "Wifi connected" );
}


/*****************************
 * Network setup functions
 *****************************/

int wifiInit( NetworkCtx *_ctx ){
    assertEspCallMessage( esp_netif_init(), "Failed to initialise network interface" );

    wifi_init_config_t wifiInitCfg = WIFI_INIT_CONFIG_DEFAULT();
    assertEspCallMessage( esp_wifi_init( &wifiInitCfg ), "Failed to initialise wifi" );
    
    esp_netif_config_t netifCfg = ESP_NETIF_DEFAULT_WIFI_STA();
    esp_netif_t *netif = esp_netif_new( &netifCfg );

    if( !netif ){
        ESP_LOGE( logTag, "Failed to create network interface" );
        return 1;
    }
    _ctx->netif = netif;
    
    assertEspCallMessage( esp_netif_attach_wifi_station( _ctx->netif ),
                          "Failed to attach interface to station" );
    //assertEspCallMessage( esp_wifi_set_default_wifi_sta_handlers(),
    //                      "Failed to set default callbacks" );
    int ret = esp_wifi_set_default_wifi_sta_handlers();
    if( ret != ESP_OK ){
        ESP_LOGE( logTag, "Failed to set default callbacks, %s", esp_err_to_name( ret ) );
    }
    
    assertEspCallMessage( esp_event_handler_register( WIFI_EVENT, WIFI_EVENT_STA_CONNECTED,
                                &wifiConnectedCallback, (void*) _ctx ),
                          "Failed to add 'wifi connected' callback" );
    assertEspCallMessage( esp_event_handler_register( WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED,
                                &wifiDisconnectedCallback, (void*) _ctx ),
                          "Failed to add 'wifi disconnected' callback" );
    assertEspCallMessage( esp_event_handler_register( IP_EVENT, IP_EVENT_STA_GOT_IP,
                                &upObtainedCallback, (void*) _ctx ),
                          "Failed to add 'ip obtained' callback" );

    return 0;
}

int wifiConfig( NetworkCtx *_ctx, const char *_ssid, const char *_psk ){

    // TODO - consider setting to flash storage
    esp_wifi_set_storage( WIFI_STORAGE_RAM );
    _ctx->wifiConfig = (wifi_config_t){ 0 };

    const size_t maxSsidLen = sizeof( _ctx->wifiConfig.sta.ssid );
    const size_t maxPwLen = sizeof( _ctx->wifiConfig.sta.password );

    const size_t ssidLen = strlen( _ssid ) + 1;
    const size_t pwLen = strlen( _psk ) + 1;

    if( ssidLen > maxSsidLen ){
        ESP_LOGE( logTag, "SSID must not be more than %i characters", maxSsidLen - 1 );
        return 1;
    }
    if( pwLen > maxPwLen ){
        ESP_LOGE( logTag, "Wifi password must not be more than %i characters", maxPwLen - 1 );
        return 1;
    }

    memcpy( _ctx->wifiConfig.sta.ssid, _ssid, ssidLen );
    memcpy( _ctx->wifiConfig.sta.password, _psk, pwLen );


    assertEspCallMessage( esp_wifi_set_mode( WIFI_MODE_STA ), "Failed to set Wifi mode" );
    assertEspCallMessage( esp_wifi_set_config( ESP_IF_WIFI_STA, &_ctx->wifiConfig ),
                          "Failed to set wifi config" );

    return 0;
}

int wifiStart( NetworkCtx *_ctx ){
    assertEspCallMessage( esp_wifi_start(), "Failed to start wifi" );

    assertEspCallMessage( esp_wifi_connect(), "Failed to connect wifi" );

    return 0;
}

int connectToWifi( NetworkCtx *_ctx, const char *_ssid, const char * _psk,
                   bool _block, uint32_t _timeoutMs ){
    if( _ctx->initialised ){
        return 1;
    }
    
    if( wifiInit( _ctx ) || wifiConfig( _ctx, _ssid, _psk ) || wifiStart( _ctx ) ){
        return 1;
    }

    _ctx->initialised = true;
    if( _block ){
        // Wait for IP address
        int64_t startTime = esp_timer_get_time();
        while( !_ctx->ipObtained ){
            vTaskDelay( 1e3 / portTICK_RATE_MS );
            int64_t timeWaited = esp_timer_get_time() - startTime;
            if( timeWaited >= (int64_t) _timeoutMs * 1e3 ){
                ESP_LOGE( logTag, "Timeout waiting for IP" );
                return 1;
            }
        }
    }

    _ctx->initialised = true;

    return 0;

}

int disconnectWifi( NetworkCtx *_ctx ){

    return 0;
}