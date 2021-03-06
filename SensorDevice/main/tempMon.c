#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp32/rom/ets_sys.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include <stdatomic.h>
#include <stdint.h>

#include "esp_sleep.h"
#define LOG_LOCAL_LEVEL ESP_LOG_WARN
#include "esp_log.h"

#include "PlatDht.h"
#include "networkConnect.h"

#include "Mqtt.h"

#define SSID			"<ssid here>"
#define PSK				"<pw here>"
#define MQTT_HOST_URL 	"SensorHub.local"
#define MQTT_HOST_PORT 	1883
#define MQTT_TOPIC "test/message"
const char jsonFormat[] = "{ \"temp\":\"%2.1f\", \"hum\":\"%2.1f\"}";
const size_t maxMessageLen = sizeof( jsonFormat ) - 2 + 1;

DhtCtx dhtCtx = { 0 };
NetworkCtx netCtx = { 0 };
MqttCtx mqttCtx = { 0 };


#define SETUP_RETRY_INTERVAL_MS 2e3
#define SENSOR_READ_INTERVAL_MS 30e3
#define SENSOR_SEND_INTERVAL_MS SENSOR_READ_INTERVAL_MS / 3

#define SENSOR_READ_RETRY_COUNT 5

_Atomic bool dataReady = false;
_Atomic bool wifiConnected = false;
_Atomic float g_temperature = -1.0f;
_Atomic float g_humidity = -1.0f;

void initialiseNetwork( void ){
	const char * logTag = "Network";
	bool initialised = false;
	while( !initialised ){
		if( connectToWifi( &netCtx, SSID, PSK, true, 10e3 ) ){
			ESP_LOGE( logTag, "Failed to connect to wifi" );
			vTaskDelay( SETUP_RETRY_INTERVAL_MS / portTICK_RATE_MS );
			continue;
		}

		if( mqttInit( &mqttCtx, MQTT_HOST_URL ) ){
			ESP_LOGE( logTag, "Failed to start MQTT" );
			vTaskDelay( SETUP_RETRY_INTERVAL_MS / portTICK_RATE_MS );
			continue;
		}
		initialised = true;
	}
	ESP_LOGI( logTag, "Network/MQTT initialised successfully" );
}

void networkTask( void *unused ){
	const char * logTag = "Network";
	ESP_LOGI( logTag, "Task started" );
	
	initialiseNetwork();

	char messageBuffer[ maxMessageLen ];
	while( 1 ){
		if( !netCtx.connected ){
			initialiseNetwork();
		}
		else if( atomic_load( &dataReady ) ){
			float temp = atomic_load( &g_temperature );
			float humidity = atomic_load( &g_humidity );
			atomic_store( &dataReady, false );
			int len = snprintf( messageBuffer, maxMessageLen, jsonFormat, temp, humidity );
			if( len >= maxMessageLen || len <= 0 ){
				ESP_LOGE( logTag, "Failed to generate message string: %i", len );
			}
			else{
				ESP_LOGD( logTag, "Sending MQTT message: %s", messageBuffer );
				if( mqttPublish( &mqttCtx, MQTT_TOPIC, messageBuffer ) ){
					ESP_LOGE( logTag, "Failed to publish sensor data" );
				}
			}
		}
		else{
			ESP_LOGI( logTag, "No data to send" );
		}
		vTaskDelay( SENSOR_SEND_INTERVAL_MS / portTICK_RATE_MS );
	}
}



void DHT_task( void *pvParameter ){
	const char * logTag = "DHT";
    ESP_LOGI( logTag, "Task started" );

	bool initialised = false;
	while( !initialised ){
    	if( initialiseDht( &dhtCtx, 4 ) ){
        	printf( "Failed to initialise DHT\n" );
			vTaskDelay( SETUP_RETRY_INTERVAL_MS / portTICK_RATE_MS );
			continue;
    	}
		initialised = true;
	}
	volatile bool *wifiConnected = &netCtx.connected;
	while( dhtCtx.initialised ) {
		if( *wifiConnected ){
			float _temp=0.0f, _humidity=0.0f;

			int ret;
			int retryIteration = 0;
			do{
				ret = readDht( &dhtCtx, &_temp, &_humidity ) ;
			}while( ret && retryIteration++ < SENSOR_READ_RETRY_COUNT );

			if( ret ){
				ESP_LOGE( logTag, "Failed to get DHT value, tried %i times", SENSOR_READ_RETRY_COUNT );
			}
			else{
				ESP_LOGD( logTag, "Temp %2.1f, Humidity %2.1f", _temp, _humidity );
				atomic_store( &g_temperature, _temp );
				atomic_store( &g_humidity, _humidity );
				atomic_store( &dataReady, true );
			}
		}
		else{
			ESP_LOGI( logTag, "No connection, not reading DHT" );
		}
		vTaskDelay( SENSOR_READ_INTERVAL_MS / portTICK_RATE_MS );
	}
}

void app_main(){
	ESP_LOGI( "Main", "Free memory: %d bytes", esp_get_free_heap_size());
	nvs_flash_init();
	ESP_ERROR_CHECK(esp_event_loop_create_default());
	vTaskDelay( 1000 / portTICK_RATE_MS );

	xTaskCreatePinnedToCore( &DHT_task, "DHT_task", 2048 * 2^4, NULL, 5, NULL, 1 );
	xTaskCreatePinnedToCore( &networkTask, "NetworkTask", 2048 * 2*3, NULL, 5, NULL, 1 );
}
