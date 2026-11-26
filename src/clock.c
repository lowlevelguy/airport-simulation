#include <stdbool.h>
#include <time.h>
#include <pthread.h>

#include "clock.h"


/* Macros ----------------------------------------------------------------------------------------*/
#define REALTIME_NS_PER_TICK	(1e6 * SIMULATION_MS_PER_TICK \
								/ REALTIME_TO_SIMULATION_MULTIPLIER)
#define ALARM_COUNT				(64)

#define UNUSED(x)				((void)(x))


/* Private types ---------------------------------------------------------------------------------*/
typedef struct {
	uint16_t remaining_ticks;
	pthread_mutex_t mutex;
	pthread_cond_t elapsed_condition;
} alarm_t;


/* Private variables -----------------------------------------------------------------------------*/
static pthread_t clk_thread;

static uint16_t tick_count = 0;
static alarm_t alarms[ALARM_COUNT] = {0};


/* Private functions -----------------------------------------------------------------------------*/
/**
 * @brief Systick ISR-like thread. Increments clock tick, decrements remaining alarm ticks, and
 * broadcasts expired alarms.
 *
 * @param params: unused
 * @return unused; NULL
 */
static void* clock_thread(void* params) {
	UNUSED(params);
	
	struct timespec timer = {
		.tv_sec = 0,
		.tv_nsec = REALTIME_NS_PER_TICK,
	};


	while (true) {
		tick_count++;
	
		for (int i = 0; i < ALARM_COUNT; i++) {
			pthread_mutex_lock(&alarms[i].mutex);

			if (alarms[i].remaining_ticks > 0) {
				alarms[i].remaining_ticks--;

				if (alarms[i].remaining_ticks == 0) {
					pthread_cond_broadcast(&alarms[i].elapsed_condition);
				}
			}
			
			pthread_mutex_unlock(&alarms[i].mutex);
		}

		nanosleep(&timer, NULL);
	}

	return NULL;
}


/* Public functions ------------------------------------------------------------------------------*/
void clock_init(void) {
	pthread_attr_t thread_attr;
	pthread_attr_init(&thread_attr);
	pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_DETACHED);

	for (int i = 0; i < ALARM_COUNT; i++) {
		pthread_mutex_init(&alarms[i].mutex, NULL);
		pthread_cond_init(&alarms[i].elapsed_condition, NULL);
	}

	pthread_create(&clk_thread, &thread_attr, clock_thread, NULL);
}

clock_status_e clock_set_alarm(tick_t duration) {
	if (duration == 0) {
		return CLOCK_STATUS_OK;
	}

	for (int i = 0; i < ALARM_COUNT; i++) {
		pthread_mutex_lock(&alarms[i].mutex);

		// Alarm slot is free iff remainig_ticks == 0
		if (alarms[i].remaining_ticks == 0) {
			alarms[i].remaining_ticks = duration;
			
			pthread_cond_wait(&alarms[i].elapsed_condition, &alarms[i].mutex);
			pthread_mutex_unlock(&alarms[i].mutex);

			return CLOCK_STATUS_OK;
		}

		pthread_mutex_unlock(&alarms[i].mutex);
	}

	// No free alarm slots
	return CLOCK_STATUS_ALARMS_FULL;
}

tick_t clock_get_tick_count(void) {
	return tick_count;
}
