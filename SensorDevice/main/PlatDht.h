#ifndef PLAT_DHT_H
#define PLAT_DHT_H

#include <stdint.h>


// == function prototypes =======================================
typedef struct DhtCtx DhtCtx;



// Ignore initialisation transitions
#define SENSOR_START_TRANSITIONS 3 // hi->low->wait 80us->high->wait 80us->low
#define TRANSITIONS_PER_BIT      2 // wait 50us->high->wait 25-70uS->low
#define TRANSITION_FINAL		 1 // Final transition from lo-hi at end
#define NUM_BITS				 40
#define TRANSITIONS_FOR_DATA     TRANSITIONS_PER_BIT * NUM_BITS
#define TRANSITION_COUNT         TRANSITIONS_FOR_DATA + SENSOR_START_TRANSITIONS + TRANSITION_FINAL - 1

#define ARR_SIZE TRANSITION_COUNT // Just throw a couple extra transition spots in there

#define DHT_INIT_TRAN_TIME		( 80 + 80 )
#define MAX_TIME_PER_BIT		( 50 + 70 )
#define DHT_DATA_TIME_MAX       MAX_TIME_PER_BIT * NUM_BITS
#define DHT_COLLECTION_TIME     DHT_INIT_TRAN_TIME + DHT_DATA_TIME_MAX
typedef struct DhtTransitionCtx DhtTransitionCtx;
typedef struct DhtTransitionTimestamp DhtTransitionTimestamp;
struct DhtTransitionTimestamp{
	uint32_t timeStamp;
	uint8_t level;
};

struct DhtTransitionCtx{
	DhtTransitionTimestamp timestampArray[ ARR_SIZE ];
	int64_t startTime;
	uint16_t nextTransitionIdx;
	uint8_t pin;
	bool overflow;
};

struct DhtCtx{
    bool initialised;
    uint8_t pin;
	DhtTransitionCtx transitionCtx;
};

uint32_t initialiseDht( DhtCtx *_ctx, uint8_t _pin );
uint32_t readDht( DhtCtx *, float*, float* );

#endif // PLAT_DHT_H