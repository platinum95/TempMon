#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE

#include <stdio.h>
#include "esp_system.h"
#include "driver/gpio.h"
#include "PlatDht.h"
#include "esp32/rom/ets_sys.h"
#include "esp_system.h"
#include "esp_sleep.h"
#include "esp_log.h"
#include "esp_timer.h"

int getSignalLevel( const DhtCtx *_ctx, int usTimeOut, bool state );
// == global defines =============================================

const char *logTag = "DHT";

#define assertCall( func, failureMessage ) if( func != ESP_OK ){ ESP_LOGE( logTag, failureMessage ); return 1; }

void IRAM_ATTR dhtGpioIsr( void *_args ){
	volatile DhtTransitionCtx *transitions = (DhtTransitionCtx*) _args;

	if( transitions->nextTransitionIdx == ARR_SIZE ){
		transitions->overflow = true;
		return;
	}
	volatile DhtTransitionTimestamp *timestamp = &transitions->timestampArray[ transitions->nextTransitionIdx ];
	transitions->nextTransitionIdx++;
	int64_t timeDiff = esp_timer_get_time() - transitions->startTime;
	if( timeDiff < 0 ) timeDiff = 0;
	if( timeDiff > UINT32_MAX ) timeDiff = UINT32_MAX;
	timestamp->timeStamp = timeDiff;
	timestamp->level = gpio_get_level( transitions->pin );
	return;
}



uint32_t initialiseDht( DhtCtx *_ctx, uint8_t _pin ){
    if( _ctx->initialised ){
        ESP_LOGE( logTag, "Attempting to reinitialise DHT device\n" );
        return 1;
    }
    // TODO - check pin range here

    _ctx->pin = _pin;

	gpio_config_t dhtGpioConf = { 0 };
	dhtGpioConf.intr_type = GPIO_PIN_INTR_ANYEDGE;
	dhtGpioConf.mode = GPIO_MODE_OUTPUT;
	dhtGpioConf.pin_bit_mask = 1 << _pin;
	dhtGpioConf.pull_down_en = 0;
	dhtGpioConf.pull_up_en = 1;

	assertCall( gpio_config( &dhtGpioConf ), "Failed to configure DHT gpio pin" );

	assertCall( gpio_set_level( _pin, 1 ), "Failed to set DHT pin level" );
	assertCall( gpio_install_isr_service( ESP_INTR_FLAG_IRAM ), "Failed to setup DHT gpio ISR" );
	_ctx->transitionCtx.nextTransitionIdx = 0;
	_ctx->transitionCtx.pin = _pin;
	assertCall( gpio_isr_handler_add( _pin, dhtGpioIsr, (void*) &_ctx->transitionCtx ), "Failed to add DHT gpio ISR" );

	ESP_LOGI( logTag, "Setup DHT context\n" );
	_ctx->initialised = true;

	return 0;
}


// == error handler ===============================================

// Wait for `level` to occur on pin, or exit after `timout` uS. 
inline uint8_t waitForSignalChange( DhtCtx *_ctx, uint8_t level, uint16_t timeout, uint16_t *timeWaited ){
	level = level ? 1 : 0;
	if( gpio_get_level( _ctx->pin ) == level ){
		*timeWaited = 0;
		return 0;
	}
	
	int64_t startTime = esp_timer_get_time();

	int64_t timeDiff = 0;
	while( timeDiff < timeout ){
		int pLev = gpio_get_level( _ctx->pin );
		if( pLev == level ){
			*timeWaited = ( timeDiff >= UINT16_MAX ) ? UINT16_MAX : (uint16_t)timeDiff;
			return 0;
		}
		ets_delay_us( 1 );
		timeDiff = esp_timer_get_time() - startTime;
	}

	// Timeout
	*timeWaited = ( timeDiff >= UINT16_MAX ) ? UINT16_MAX : (uint16_t)timeDiff;
	return 1;
}

// Return the pin level and time spent at that level for a given index
uint8_t decodeTransition( const volatile DhtTransitionCtx *_state, uint8_t _tsIdx, uint8_t *_level, uint16_t *_tDiff ){
	const int64_t lastTime = _tsIdx == 0 ? _state->startTime : _state->timestampArray[ _tsIdx - 1 ].timeStamp;

	const volatile DhtTransitionTimestamp *ts = &_state->timestampArray[ _tsIdx ];
	*_tDiff = ts->timeStamp - lastTime;
	*_level = ts->level ? 0 : 1;
	return 0;
}

uint8_t decodeBit( const volatile DhtTransitionCtx *_state, uint8_t *_currIdx, uint8_t *_bitVal ){
	uint8_t level;
	uint16_t time;

	decodeTransition( _state, *_currIdx, &level, &time );
	if( level != 0 ){
		// Error
		printf( "fail 1 \n" );
		return 1;
	}
	if( time > 80 || time < 20 ){
		// Error
		printf( "fail 2\n" );
		return 1;
	}
	// Got successful bit init transition
	*_currIdx = (*_currIdx) + 1;
	decodeTransition( _state, *_currIdx, &level, &time );
	if( level != 1 ){
		printf( "fail 2\n" );
		return 1;
	}
	*_currIdx = (*_currIdx) + 1;
	*_bitVal = time > 50 ? 1 : 0;
	return 0;
}

uint32_t readDht( DhtCtx *_ctx, float * _temp, float *_hum ){
	// Initiate data communication between ESP and DHT. Ensure pin is set HIGH before starting,
	// then pull pin LOW for 1-10ms (say 5), then pull HIGH and wait for response (20-40us).
	if( !_ctx->initialised ){
		ESP_LOGE( logTag, "Attempting to use uninitialised DHT context" );
		return 1;
	}

	int pin = _ctx->pin;
	assertCall( gpio_set_direction( pin, GPIO_MODE_INPUT ), "Failed to set DHT pin to input mode" );

	if( gpio_get_level( pin ) != 1 ){
		ESP_LOGE( logTag, "Invalid start condition for DHT value read" );
		return 1;
	}
		
	assertCall( gpio_set_direction( pin, GPIO_MODE_OUTPUT ), "Failed to set DHT pin to output mode" );

	volatile DhtTransitionCtx *transitionCtx = (DhtTransitionCtx*) &_ctx->transitionCtx;
	/*
	* Pull pin low
	***************************************************/
	assertCall( gpio_set_level( pin, 0 ), "Failed to set DHT pin level" );
	
	 // Configure sleep for 5000uS
	assertCall( esp_sleep_enable_timer_wakeup( 5e3 ), "Failed to set wakup timer in DHT read" );
	assertCall( esp_light_sleep_start(), "Failed to sleep in DHT read" );

	/*
	* Pull pin high
	******************************************************/
	assertCall( gpio_set_level( pin, 1 ), "Failed to set DHT pin level" );
		
	transitionCtx->nextTransitionIdx = 0;
	transitionCtx->pin = _ctx->pin;
	transitionCtx->overflow = false;

	ets_delay_us( 25 );
	transitionCtx->startTime = esp_timer_get_time();
	assertCall( gpio_set_direction( pin, GPIO_MODE_INPUT ), "Failed to set DHT pin to input mode" );

	/*
	* Wait for DHT pulldown response
	*********************************************************/
	// Now we wait for the pin to be pulled down, should be no more than 40us
	uint16_t timeWaited = 0;
	uint8_t tWaitRet;
	if( gpio_get_level( pin ) != 0 ){
		tWaitRet = waitForSignalChange( _ctx, 0, 50, &timeWaited );
		if( tWaitRet ){
			ESP_LOGE( logTag, "Timeout when waiting for initial DHT response" );
			return 1;
		}
	}

	// Wait for data to come in
	// Should be around 4500 uS
	// TODO - rather than sleep here, we should loop checking for new ISR data, and consume it.
	// 		  This way, we can reduce the size of the DHT context by having a much smaller data array
	ESP_LOGD( logTag, "Sleeping for %i uS", DHT_DATA_TIME_MAX );
	ets_delay_us( DHT_DATA_TIME_MAX );
	
	ESP_LOGD( logTag, "Recorded %i transitions, expected %i", transitionCtx->nextTransitionIdx, TRANSITION_COUNT );
	if( transitionCtx->nextTransitionIdx != TRANSITION_COUNT ){
		ESP_LOGE( logTag, "Did not record expected number of transitions" );
		return 1;
	}
	if( transitionCtx->overflow ){
		ESP_LOGE( logTag, "Recorded too many transitions" );
		return 1;
	}

#ifdef PRINT_DHT_TRANSITIONS
	// Print out transition history	
	for( int i = 0; i < transitionCtx->nextTransitionIdx; i++ ){
		uint8_t level;
		uint16_t tDiff;
		if( decodeTransition( transitionCtx, i, &level, &tDiff ) ){
			ESP_LOGE( logTag, "Failed to decode transition %i", i );
			return 1;
		}
		ESP_LOGI( logTag, "State %i for %i uS", level, tDiff );
	}
#endif
	int64_t maxTime = (int64_t)transitionCtx->timestampArray[ 83 ].timeStamp;
	ESP_LOGI( logTag, "Max time: %i", (int) maxTime );
	// Skip first 4 transitions (sensor init)
	uint8_t currIdx = 3;
	// Start loading in the bits
	uint16_t humidityValRead = 0;
	uint16_t tempValRead = 0;
	uint8_t checksum = 0;

	for( int8_t i = 15; i >= 0; i-- ){
		uint8_t bit;
		if( decodeBit( transitionCtx, &currIdx, &bit ) ){
			printf( "Fail 4\n" );
			return 1;
		}
		humidityValRead |= ((uint16_t)bit) << i;
	}

	for( int8_t i = 15; i >= 0; i-- ){
		uint8_t bit;
		if( decodeBit( transitionCtx, &currIdx, &bit ) ){
			printf( "Fail 4\n" );
			return 1;
		}
		tempValRead |= ((uint16_t)bit) << i;
	}
	for( int8_t i = 7; i >= 0; i-- ){
		uint8_t bit;
		if( decodeBit( transitionCtx, &currIdx, &bit ) ){
			printf( "Fail 4\n" );
			return 1;
		}
		checksum |= ((uint8_t)bit) << i;
	}

		// Verify checksum
	uint64_t sum = ( ( humidityValRead & 0xFF00 ) >> 8 ) + ( humidityValRead & 0x00FF ) +
				   ( ( tempValRead & 0xFF00 ) >> 8 ) + ( tempValRead & 0x00FF );
	if( ((uint8_t) (sum & 0xFF)) != checksum ){
		ESP_LOGE( logTag, "DHT checksum failed: %i %i %i", humidityValRead, tempValRead, checksum );
		return 1;
	} 

	// Parse the data
	*_hum = ((float)humidityValRead) / 10.0f;
	int16_t tempSign = ( tempValRead & 0x8000 ) ? -1 : 1;
	int16_t tempVal = ( tempValRead & 0x7FFF ) * tempSign;
	*_temp = ((float)tempVal) / 10.0f;

	return 0;

}