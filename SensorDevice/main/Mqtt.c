#include "Mqtt.h"
#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"


static const char * logTag = "MQTT";

#define assertEspCall( call ) if( call != ESP_OK ) return 1;
#define assertEspCallMessage( call, msg ) if( call != ESP_OK ){ ESP_LOGE( logTag, msg ); return 1; }

static void mqttHandler(){

}

uint8_t mqttInit( MqttCtx *_ctx, const char * _uri ){
    if( _ctx->initialised ){
        ESP_LOGE( logTag, "MQTT already initialised" );
        return 1;
    }
    esp_mqtt_client_config_t mqttConfig = {
        .host = _uri
    };

    _ctx->mqttHandle = esp_mqtt_client_init( &mqttConfig );
    if( !_ctx->mqttHandle ){
        ESP_LOGE( logTag, "Failed to create mqtt client" );
        return 1;
    }
    assertEspCallMessage( esp_mqtt_client_register_event( _ctx->mqttHandle, ESP_EVENT_ANY_ID,
                                                          mqttHandler, _ctx->mqttHandle ),
                          "Failed to register MQTT event handler" );
    assertEspCallMessage( esp_mqtt_client_start( _ctx->mqttHandle ),
                          "Failed to start MQTT client" );
    return 0;
}

int mqttPublish( MqttCtx *_ctx, const char * _topic, const char * _data ){
    int ret = esp_mqtt_client_publish( _ctx->mqttHandle, _topic, _data, 0, 5, 0 );
    return ret >= 0 ? 0 : 1;
}

int mqttSubscribe( MqttCtx *_ctx, const char * _topic ){
    return 1;
}