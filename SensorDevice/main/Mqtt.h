#include <stdint.h>
#include "mqtt_client.h"

typedef struct MqttCtx MqttCtx;

struct MqttCtx{
    bool initialised;
    esp_mqtt_client_handle_t mqttHandle;
};

uint8_t mqttInit( MqttCtx *_ctx, const char * _uri );

int mqttPublish( MqttCtx *_ctx, const char * _topic, const char * data );

int mqttSubscribe( MqttCtx *_ctx, const char * _topic );