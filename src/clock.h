#pragma once

#include <stdint.h>


/* Macros ----------------------------------------------------------------------------------------*/
#define SIMULATION_MS_PER_TICK				(500)	// resolution of the clock
#define REALTIME_TO_SIMULATION_MULTIPLIER	(80)	// simulation seconds per realtime second
#define SIMULATION_MS_TO_TICKS(x)			((x)/SIMULATION_MS_PER_TICK)


/* Types -----------------------------------------------------------------------------------------*/
typedef uint16_t tick_t;

typedef enum {
	CLOCK_STATUS_OK,
	CLOCK_STATUS_ALARMS_FULL,
} clock_status_e ;


/* Functions -------------------------------------------------------------------------------------*/
/**
 * @brief Initialises the clock: mutexes, conditions, and starts the clock thread.
 */
void clock_init(void);

/**
 * @brief Sets an alarm for the specified duration in ticks, and blocks the calling thread until
 * the alarm fires.
 *
 * @param duration: number of ticks to block for
 */
clock_status_e clock_set_alarm(tick_t duration);

/**
 * @brief Returns the current tick count, asynchronously.
 */
tick_t clock_get_tick_count(void);
