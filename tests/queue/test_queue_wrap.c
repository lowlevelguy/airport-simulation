#include <unity.h>

#include "queue.h"


#ifndef QUEUE_TEST_CAP
#error "QUEUE_TEST_CAP must be defined by the build system"
#endif

#define ITEM_SIZE	((uint8_t)sizeof(uint32_t))
#define CAPACITY	((uint8_t)QUEUE_TEST_CAP)

// Only ring wrapping is under test: a cursor leaves the ring and returns to slot
// zero. Two fills wrap every cursor twice, the smallest count that does so. The
// defect this guards against is a cursor advanced modulo 256 instead of modulo
// the capacity, which push_checked() catches on its first step past the ring, so
// the number of fills below does not need to reach 256.
#define WRAP_FILLS	2u
#define SLIDE_LENGTH	(WRAP_FILLS * CAPACITY)


_Static_assert(CAPACITY >= 1, "capacity must hold at least one item");
_Static_assert(CAPACITY < 256, "capacity is stored in a uint8_t");


/* Private variables ---------------------------------------------------------*/
static uint8_t buffer[CAPACITY * ITEM_SIZE];
static queue_t queue;


/* Helper functions ----------------------------------------------------------*/
/**
 * @brief Pushes one item and fails the test unless the queue accepted it.
 *
 * @param value value to push
 *
 * @note Also asserts the cursor invariant: a cursor is a slot index inside the
 * ring. A cursor advanced modulo 256 leaves that range before it corrupts any
 * stored value, which at a capacity of 255 is otherwise invisible.
 */
static void push_checked(uint32_t value) {
	TEST_ASSERT_EQUAL_INT(QUEUE_STATUS_OK, queue_push(&queue, &value, ITEM_SIZE));
	TEST_ASSERT_LESS_THAN_UINT8(CAPACITY, queue.push_idx);
}

/**
 * @brief Pops one item, fails the test unless it is @p expected.
 *
 * @param expected value the queue must return
 */
static void pop_checked(uint32_t expected) {
	uint32_t popped = 0;

	TEST_ASSERT_EQUAL_INT(QUEUE_STATUS_OK, queue_pop(&queue, &popped, ITEM_SIZE));
	TEST_ASSERT_EQUAL_UINT32(expected, popped);
	TEST_ASSERT_LESS_THAN_UINT8(CAPACITY, queue.pop_idx);
}


/* Unit tests ----------------------------------------------------------------*/
/**
 * @brief Tests that repeated fill and drain cycles keep FIFO order across the
 * uint8_t index wrap.
 *
 * @note A cursor advanced without the modulo reaches the end of the buffer, so
 * the symptom is either a wrong value or a write past the ring buffer.
 */
static void test_fill_drain_cycles_preserve_fifo_order(void) {
	for (uint32_t fill = 0; fill < WRAP_FILLS; fill++) {
		for (uint8_t i = 0; i < CAPACITY; i++) {
			push_checked((uint32_t)fill * (uint32_t)CAPACITY + (uint32_t)i);
		}

		for (uint8_t i = 0; i < CAPACITY; i++) {
			pop_checked((uint32_t)fill * (uint32_t)CAPACITY + (uint32_t)i);
		}
	}

	uint32_t popped = 0;

	TEST_ASSERT_EQUAL_INT(QUEUE_STATUS_EMPTY, queue_pop(&queue, &popped, ITEM_SIZE));
}

/**
 * @brief Tests that a sliding window, never full and never empty, keeps FIFO
 * order across the uint8_t index wrap.
 *
 * @note With a capacity of one this degenerates into strict alternation, so the
 * push cursor must return to slot zero on every single push.
 */
static void test_sliding_window_preserves_fifo_order(void) {
	uint32_t produced = 0;
	uint32_t consumed = 0;
	uint32_t prefill = (uint32_t)(CAPACITY - 1u);

	// Leave exactly one free slot, so the window can always advance. With a
	// capacity of one this prefill is empty.
	for (uint32_t i = 0; i < prefill; i++) {
		push_checked(produced);
		produced++;
	}

	for (uint32_t n = 0; n < SLIDE_LENGTH; n++) {
		push_checked(produced);
		produced++;

		pop_checked(consumed);
		consumed++;
	}

	TEST_ASSERT_EQUAL_UINT8((uint8_t)(CAPACITY - 1u), queue.size);
}


/* Unity harness -------------------------------------------------------------*/
void setUp(void) {
	queue_init(&queue, buffer, ITEM_SIZE, CAPACITY);
}

void tearDown(void) {
	queue_deinit(&queue);
}

int main(void) {
	UNITY_BEGIN();

	RUN_TEST(test_fill_drain_cycles_preserve_fifo_order);
	RUN_TEST(test_sliding_window_preserves_fifo_order);

	return UNITY_END();
}
