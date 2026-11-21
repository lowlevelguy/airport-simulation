#include <assert.h>
#include <string.h>
#include <pthread.h>

#include "queue.h"

/* Macros ----------------------------------------------------------------------------------------*/
#define U8(x)		((uint8_t)(x))
#define U8_PTR(x)	((uint8_t*)(x))


/* Public functions ------------------------------------------------------------------------------*/
void queue_init(queue_t* q, void* buffer, uint8_t buffer_alignment, uint8_t buffer_capacity) {
	assert(q != NULL);
	assert(buffer != NULL);
	assert(buffer_alignment > 0);
	assert(buffer_capacity > 0);

	q->ring_buffer = buffer;
	q->ring_alignment = buffer_alignment;
	q->ring_capacity = buffer_capacity;

	pthread_mutex_init(&q->mutex, NULL);
	q->push_idx = 0;
	q->pop_idx = 0;
	q->size = 0;
}

queue_status_e queue_push(queue_t* q, const void* item, uint8_t item_size) {
	assert(q != NULL);
	assert(item != NULL);
	assert(item_size == q->ring_alignment);

	pthread_mutex_lock(&q->mutex);
	
	// Return error if queue is full
	if (q->size >= q->ring_capacity) {
		pthread_mutex_unlock(&q->mutex);
		return QUEUE_STATUS_FULL;
	}

	memcpy(U8_PTR(q->ring_buffer) + q->push_idx * q->ring_alignment, item, q->ring_alignment);
	q->push_idx = U8((q->push_idx + 1) % q->ring_capacity);
	q->size++;

	pthread_mutex_unlock(&q->mutex);

	return QUEUE_STATUS_OK;
}

queue_status_e queue_pop(queue_t* q, void* item, uint8_t item_size) {
	assert(q != NULL);
	assert(item != NULL);
	assert(item_size == q->ring_alignment);

	pthread_mutex_lock(&q->mutex);

	// Return error if queue is empty
	if (q->size == 0) {
		pthread_mutex_unlock(&q->mutex);
		return QUEUE_STATUS_EMPTY;
	}
	
	memcpy(item, U8_PTR(q->ring_buffer) + q->pop_idx * q->ring_alignment, q->ring_alignment);
	q->pop_idx = U8((q->pop_idx + 1) % q->ring_capacity);
	q->size--;

	pthread_mutex_unlock(&q->mutex);

	return QUEUE_STATUS_OK;
}

void queue_deinit(queue_t* q) {
	assert(q != NULL);

	q->ring_buffer = NULL;
	q->ring_alignment = 0;
	q->ring_capacity = 0;

	pthread_mutex_destroy(&q->mutex);
	
	q->size = 0;
	q->push_idx = 0;
	q->pop_idx = 0;
}
