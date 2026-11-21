#pragma once

#include <stdint.h>
#include <pthread.h>


/* Types -----------------------------------------------------------------------------------------*/
typedef struct {
	void* ring_buffer;
	uint8_t ring_alignment;		// size in bytes of a single element in the queue
	uint8_t ring_capacity;		// maximum number of elements that can fit in the queue

	// Declaring as volatile is unneeded, since mutex locking acts as a memory guard
	uint8_t push_idx;
	uint8_t pop_idx;
	uint8_t size;

	pthread_mutex_t mutex;
} queue_t;

typedef enum {
	QUEUE_STATUS_OK,
	QUEUE_STATUS_EMPTY,
	QUEUE_STATUS_FULL,
} queue_status_e;


/* Function prototypes----------------------------------------------------------------------------*/
/**
 * @brief Initialises the queue pointed to by @p q, setting @p buffer as its underlying buffer with
 * an alignment of @p buffer_alignment in bytes and capacity of @p buffer_capacity aligned items,
 * and marks the buffer empty.
 *
 * @param q: pointer to queue to be initiliased
 * @param buffer: the buffer to be used to hold the queue
 * @param buffer_alignment: size in bytes of a single element in the buffer
 * @param buffer_capacity: the maximum number of elements that can fit in the buffer
 *
 * @note Aborts on @c q==NULL, @c buffer==NULL, @c buffer_alignment==0 and @c buffer_capacity==0.
 * @warning The queue is assumed non-initialised, and in particular the internal mutex is assumed
 * unlocked.
 */
void queue_init(queue_t* q, void* buffer, uint8_t buffer_alignment, uint8_t buffer_capacity);

/**
 * @brief Pushes @p item, of size @p item_size, into the queue pointed to by @p q of alignment equal
 * to @p item_size.
 *
 * @param q: pointer to queue to be push into
 * @param item: item to be pushed
 * @param item_size: size in bytes of the item to be pushed; must coincide with the queue alignment
 *
 * @return @c QUEUE_STATUS_OK on success, @c QUEUE_STATUS_FULL on full queue
 *
 * @warning Aborts on @c q==NULL, @c item==NULL and @c item_size!=q->alignment.
 */
queue_status_e queue_push(queue_t* q, const void* item, uint8_t item_size);

/**
 * @brief Pops the oldest item in the queue pointed to by @p q, and copies its content into the
 * memory area pointed to by @p item, of size @p item_size equal to queue alignment.
 *
 * @param q: pointer to queue to be pop from
 * @param item: pointer to memory area the popped item will be written to
 * @param item_size: size in bytes of the memory area; must coincide with the queue alignment
 *
 * @return @c QUEUE_STATUS_OK on success, @c QUEUE_STATUS_EMPTY on empty queue
 *
 * @warning Aborts on @c q==NULL, @c item==NULL and @c item_size!=q->alignment.
 */
queue_status_e queue_pop(queue_t* q, void* item, uint8_t item_size);

/**
 * @brief Deinitialises the queue pointed to by @p q.
 *
 * @param q: pointer to the queue to deinitialise
 *
 * @warning The queue is assumed to not be in current use; more precisely, the internal mutex is
 * assumed unlocked.
 */
void queue_deinit(queue_t* q);
