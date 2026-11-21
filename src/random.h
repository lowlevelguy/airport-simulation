#pragma once

#include <stdint.h>

/**
 * @brief Returns a positive real sampled from an exponential distribution of mean @p mean, or
 * @p cap when the sampled value exceeds it.
 *
 * @param mean: mean of exponential distribution to sample from; 1/lambda
 * @param cap: the desired ceiling of values
 *
 * @warning Aborts on @c cap<=0.
 */
float exponential_rand(float mean, float cap);
