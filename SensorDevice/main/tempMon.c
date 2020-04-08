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
#include "esp_log.h"

#include "PlatDht.h"

DhtCtx dhtCtx={0};

// 10 second interval
#define SENSOR_READ_INTERVAL_MS 10e3

void DHT_task( void *pvParameter ){
    
	//DhtCtx dhtCtx = { 0 };
    if( initialiseDht( &dhtCtx, 4 ) ){
        printf( "Failed to initialise DHT\n" );
    }
	
	while( dhtCtx.initialised ) {
		printf("=== Reading DHT ===\n" );
		float _temp=0.0f, _humidity=0.0f;
		int ret = readDht( &dhtCtx, &_temp, &_humidity ) ;
		if( ret ){
			ESP_LOGE( "Main", "Failed to get DHT value" );
            continue;
		}
        
		// -- wait at least 2 sec before reading again ------------
		// The interval of whole process must be beyond 2 seconds !! 
		vTaskDelay( SENSOR_READ_INTERVAL_MS / portTICK_RATE_MS );
	}
}

void publishDataTask( void *taskParam ){

}

void app_main(){
	nvs_flash_init();
	vTaskDelay( 1000 / portTICK_RATE_MS );
	xTaskCreate( &DHT_task, "DHT_task", 2048, NULL, 5, NULL );
}
