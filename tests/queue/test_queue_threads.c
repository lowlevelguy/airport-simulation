#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>
#include <stdbool.h>

#include "unity.h"
#include "queue.h"


#define ITEM_SIZE		((uint8_t)sizeof(test_item_t))
#define CAPACITY		((uint8_t)7)

#define PRODUCER_COUNT		2u
#define CONSUMER_COUNT		2u
#define ITEMS_PER_PRODUCER	2000u
#define TOTAL_ITEMS		(PRODUCER_COUNT * ITEMS_PER_PRODUCER)
#define ROUNDS			2u

#define BURST_PERIOD		8u
#define MARKER_SEED		0x9E3779B9u


_Static_assert(ITEMS_PER_PRODUCER <= 65535u, "sequence number is a uint16_t");
_Static_assert(ROUNDS > 0u, "at least one round is required");


/* Types ---------------------------------------------------------------------*/
typedef struct {
	uint8_t producer;
	uint16_t seq;
	uint32_t marker;
} test_item_t;

typedef struct {
	uint8_t index;
	uint32_t seed;
} thread_arg_t;


/* Private variables ---------------------------------------------------------*/
static uint8_t buffer[CAPACITY * ITEM_SIZE];
static queue_t queue;

// Written by both consumers, hence atomics: a duplicate shows up as a second
// store to the same slot.
static _Atomic uint8_t seen[PRODUCER_COUNT][ITEMS_PER_PRODUCER];

// Per-consumer bookkeeping, never touched by another thread.
static uint16_t last_seq[CONSUMER_COUNT][PRODUCER_COUNT];
static bool seq_recorded[CONSUMER_COUNT][PRODUCER_COUNT];

static _Atomic uint32_t failures;
static _Atomic uint32_t consumed;


/* Helper functions ----------------------------------------------------------*/
/**
 * @brief Advances a per-thread pseudo-random sequence.
 *
 * @param state generator state, updated in place
 *
 * @return the next pseudo-random value
 *
 * @note Deterministic per seed, so a failing round can be reproduced.
 */
static uint32_t xorshift(uint32_t* state) {
	uint32_t x = *state;

	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;
	*state = x;

	return x;
}

/**
 * @brief Derives the marker a well-formed item must carry.
 *
 * @param producer producing thread index
 * @param seq sequence number within that producer
 *
 * @return the marker value
 */
static uint32_t marker_for(uint8_t producer, uint16_t seq) {
	return (uint32_t)producer * MARKER_SEED + (uint32_t)seq;
}

/**
 * @brief Records a defect without touching Unity, which is not thread safe.
 */
static void record_failure(void) {
	(void)atomic_fetch_add_explicit(&failures, 1u, memory_order_relaxed);
}

/**
 * @brief Checks one popped item and records it as seen.
 *
 * @param consumer consuming thread index
 * @param item     item returned by queue_pop
 *
 * @note Detects duplicates, torn copies and items that reach one consumer out
 * of FIFO order.
 */
static void record_item(uint8_t consumer, const test_item_t* item) {
	if (item->producer >= PRODUCER_COUNT) {
		record_failure();
		return;
	}

	if (item->seq >= ITEMS_PER_PRODUCER) {
		record_failure();
		return;
	}

	if (item->marker != marker_for(item->producer, item->seq)) {
		record_failure();
		return;
	}

	if (atomic_load_explicit(&seen[item->producer][item->seq], memory_order_relaxed) != 0u) {
		record_failure();
	}
	atomic_store_explicit(&seen[item->producer][item->seq], 1u, memory_order_relaxed);

	if (seq_recorded[consumer][item->producer]
		&& (item->seq < last_seq[consumer][item->producer])) {
		record_failure();
	}

	last_seq[consumer][item->producer] = item->seq;
	seq_recorded[consumer][item->producer] = true;
}

/**
 * @brief Pushes its share of the items, pacing them pseudo-randomly.
 *
 * @param arg thread_arg_t for this producer
 *
 * @return NULL
 */
static void* producer_thread(void* arg) {
	const thread_arg_t* thread = arg;
	uint8_t producer = thread->index;
	uint32_t rng = thread->seed;

	for (uint16_t seq = 0; seq < ITEMS_PER_PRODUCER; seq++) {
		test_item_t item = {
			.producer = producer,
			.seq = seq,
			.marker = marker_for(producer, seq),
		};

		// Randomised pacing: most items are pushed back to back, some are
		// preceded by a yield, so the interleaving differs on every run.
		if ((xorshift(&rng) % BURST_PERIOD) == 0u) {
			sched_yield();
		}

		while (queue_push(&queue, &item, ITEM_SIZE) != QUEUE_STATUS_OK) {
			sched_yield();
		}
	}

	return NULL;
}

/**
 * @brief Pops until every item has been consumed, checking each one.
 *
 * @param arg thread_arg_t for this consumer
 *
 * @return NULL
 */
static void* consumer_thread(void* arg) {
	const thread_arg_t* thread = arg;
	uint8_t consumer = thread->index;
	uint32_t rng = thread->seed;

	while (1) {
		if (atomic_load_explicit(&consumed, memory_order_relaxed) >= TOTAL_ITEMS) {
			break;
		}

		test_item_t item;

		if (queue_pop(&queue, &item, ITEM_SIZE) == QUEUE_STATUS_OK) {
			record_item(consumer, &item);
			(void)atomic_fetch_add_explicit(&consumed, 1u, memory_order_relaxed);
		} else if ((xorshift(&rng) % 2u) == 0u) {
			sched_yield();
		}
	}

	return NULL;
}

/**
 * @brief Clears the bookkeeping and starts a fresh queue.
 *
 * @param round round index, mixed into the thread seeds
 */
static void reset_state(uint32_t round) {
	for (uint8_t producer = 0; producer < PRODUCER_COUNT; producer++) {
		for (uint16_t seq = 0; seq < ITEMS_PER_PRODUCER; seq++) {
			atomic_store_explicit(&seen[producer][seq], 0u, memory_order_relaxed);
		}
	}

	for (uint8_t consumer = 0; consumer < CONSUMER_COUNT; consumer++) {
		for (uint8_t producer = 0; producer < PRODUCER_COUNT; producer++) {
			last_seq[consumer][producer] = 0;
			seq_recorded[consumer][producer] = false;
		}
	}

	atomic_store_explicit(&failures, 0u, memory_order_relaxed);
	atomic_store_explicit(&consumed, 0u, memory_order_relaxed);

	queue_init(&queue, buffer, ITEM_SIZE, CAPACITY);

	(void)round;
}

/**
 * @brief Runs one producer/consumer round and returns once every thread has
 * finished.
 *
 * @param round round index, mixed into the thread seeds
 */
static void run_round(uint32_t round) {
	pthread_t producers[PRODUCER_COUNT];
	pthread_t consumers[CONSUMER_COUNT];
	thread_arg_t args[PRODUCER_COUNT + CONSUMER_COUNT];

	reset_state(round);

	for (uint8_t i = 0; i < PRODUCER_COUNT; i++) {
		args[i].index = i;
		args[i].seed = MARKER_SEED + round * 7919u + (uint32_t)i + 1u;

		TEST_ASSERT_EQUAL_INT(0,
			pthread_create(&producers[i], NULL, producer_thread, &args[i]));
	}

	for (uint8_t i = 0; i < CONSUMER_COUNT; i++) {
		uint8_t slot = (uint8_t)(PRODUCER_COUNT + i);

		args[slot].index = i;
		args[slot].seed = MARKER_SEED + round * 7919u + (uint32_t)slot + 1u;

		TEST_ASSERT_EQUAL_INT(0,
			pthread_create(&consumers[i], NULL, consumer_thread, &args[slot]));
	}

	for (uint8_t i = 0; i < PRODUCER_COUNT; i++) {
		TEST_ASSERT_EQUAL_INT(0, pthread_join(producers[i], NULL));
	}

	for (uint8_t i = 0; i < CONSUMER_COUNT; i++) {
		TEST_ASSERT_EQUAL_INT(0, pthread_join(consumers[i], NULL));
	}

	test_item_t leftover;

	TEST_ASSERT_EQUAL_INT(QUEUE_STATUS_EMPTY, queue_pop(&queue, &leftover, ITEM_SIZE));

	queue_deinit(&queue);
}


/* Unit tests ----------------------------------------------------------------*/
/**
 * @brief Tests two producers and two consumers under randomised access, twice.
 *
 * @note Fails on a lost item, a duplicated item, a torn copy, an item reaching
 * one consumer out of FIFO order, or an item left behind in the queue.
 */
static void test_two_producers_two_consumers_under_randomised_access(void) {
	for (uint32_t round = 0; round < ROUNDS; round++) {
		run_round(round);

		TEST_ASSERT_EQUAL_UINT32(0u, atomic_load_explicit(&failures, memory_order_relaxed));
		TEST_ASSERT_EQUAL_UINT32(TOTAL_ITEMS,
			atomic_load_explicit(&consumed, memory_order_relaxed));

		uint32_t counted = 0;

		for (uint8_t producer = 0; producer < PRODUCER_COUNT; producer++) {
			for (uint16_t seq = 0; seq < ITEMS_PER_PRODUCER; seq++) {
				counted += atomic_load_explicit(&seen[producer][seq], memory_order_relaxed);
			}
		}

		TEST_ASSERT_EQUAL_UINT32(TOTAL_ITEMS, counted);
	}
}


/* Unity harness -------------------------------------------------------------*/
void setUp(void) {}

void tearDown(void) {}

int main(void) {
	UNITY_BEGIN();

	RUN_TEST(test_two_producers_two_consumers_under_randomised_access);

	return UNITY_END();
}
