#include <math.h>
#include <assert.h>

#include "random.h"


/* Private variables -----------------------------------------------------------------------------*/
static uint32_t seed = 0xDEADBEEF;


/* Private functions -----------------------------------------------------------------------------*/
/**
 * @brief Generates a pseudo-random number, emulating sampling from a uniform distribution on the
 * set of integers {1, 2, ..., @c UINT32_MAX}.
 */
uint32_t xorshift32() {
	uint32_t new = seed;

	new ^= new << 13;
	new ^= new >> 17;
	new ^= new << 5;

	seed = new;
	return seed;
}


/* Public functions ------------------------------------------------------------------------------*/
float exponential_rand(float mean, float cap) {
	assert(cap > 0);

	double uniform = (float)(xorshift32() - 1) / (float)UINT32_MAX;
	float exp = mean * log(1.0 - uniform);

	if (exp > cap) {
		return cap;
	} else {
		return exp;
	}
}
