#include <pthread.h>
#include <string.h>
#include <stdint.h>

#include "unity.h"
#include "queue.h"


#define ITEM_SIZE		((uint8_t)sizeof(uint32_t))
#define SMALL_CAPACITY	((uint8_t)4)
#define MAX_CAPACITY	((uint8_t)255)

#define BYTE_ITEM_SIZE	((uint8_t)1)
#define NINE_ITEM_SIZE	((uint8_t)9)


/* Types ---------------------------------------------------------------------*/
typedef struct {
	uint8_t bytes[9];
} nine_byte_item_t;


/* Private variables ---------------------------------------------------------*/
static uint8_t buffer[MAX_CAPACITY * ITEM_SIZE];

// queue_deinit is verified by interposing on pthread_mutex_destroy for this test
// binary (-Wl,--wrap, see tests/CMakeLists.txt). Probing a destroyed mutex with
// pthread_mutex_trylock instead would be undefined behaviour.
static int mutex_destroy_calls;
static pthread_mutex_t* mutex_destroy_arg;


/* External functions --------------------------------------------------------*/
extern int __real_pthread_mutex_destroy(pthread_mutex_t* mutex);

// Linker-mandated entry point for -Wl,--wrap=pthread_mutex_destroy; defined below.
int __wrap_pthread_mutex_destroy(pthread_mutex_t* mutex);


/* Helper functions ----------------------------------------------------------*/
/**
 * @brief Interposes on pthread_mutex_destroy so that queue_deinit can be
 * observed.
 *
 * @param mutex mutex handed to the intercepted call
 *
 * @return whatever the real pthread_mutex_destroy returns
 *
 * @note The symbol names are dictated by the linker's --wrap option, so they
 * keep reserved double-underscore prefixes; logged as a MISRA C:2012 rule 21.1
 * deviation in .agents/STYLES.md.
 */
int __wrap_pthread_mutex_destroy(pthread_mutex_t* mutex) {
	mutex_destroy_calls++;
	mutex_destroy_arg = mutex;

	return __real_pthread_mutex_destroy(mutex);
}

/**
 * @brief Pushes a run of items carrying consecutive values.
 *
 * @param queue queue to push into
 * @param count number of items to push
 * @param first value of the first item, every following item is one greater
 */
static void push_items(queue_t* queue, uint8_t count, uint32_t first) {
	for (uint8_t i = 0; i < count; i++) {
		uint32_t item = first + (uint32_t)i;

		TEST_ASSERT_EQUAL_INT(QUEUE_STATUS_OK, queue_push(queue, &item, ITEM_SIZE));
	}
}


/* Unit tests ----------------------------------------------------------------*/
/**
 * @brief Tests that a freshly initialised queue exposes the caller's buffer and
 * reports itself empty.
 */
static void test_init_sets_fields_and_marks_queue_empty(void) {
	queue_t queue;

	queue_init(&queue, buffer, ITEM_SIZE, SMALL_CAPACITY);

	TEST_ASSERT_EQUAL_PTR(buffer, queue.ring_buffer);
	TEST_ASSERT_EQUAL_UINT8(ITEM_SIZE, queue.ring_alignment);
	TEST_ASSERT_EQUAL_UINT8(SMALL_CAPACITY, queue.ring_capacity);
	TEST_ASSERT_EQUAL_UINT8(0, queue.push_idx);
	TEST_ASSERT_EQUAL_UINT8(0, queue.pop_idx);
	TEST_ASSERT_EQUAL_UINT8(0, queue.size);

	queue_deinit(&queue);
}

/**
 * @brief Tests that pushing into an empty queue succeeds and raises the
 * occupancy to one.
 */
static void test_push_into_empty_queue_succeeds(void) {
	queue_t queue;
	uint32_t item = 0x12345678u;

	queue_init(&queue, buffer, ITEM_SIZE, SMALL_CAPACITY);
	TEST_ASSERT_EQUAL_INT(QUEUE_STATUS_OK, queue_push(&queue, &item, ITEM_SIZE));
	TEST_ASSERT_EQUAL_UINT8(1, queue.size);
	queue_deinit(&queue);
}

/**
 * @brief Tests that popping from a full queue yields the oldest item and lowers
 * the occupancy.
 */
static void test_pop_from_full_queue_returns_oldest_item(void) {
	queue_t queue;
	uint32_t popped = 0;

	queue_init(&queue, buffer, ITEM_SIZE, SMALL_CAPACITY);
	push_items(&queue, SMALL_CAPACITY, 10u);
	TEST_ASSERT_EQUAL_UINT8(SMALL_CAPACITY, queue.size);

	TEST_ASSERT_EQUAL_INT(QUEUE_STATUS_OK, queue_pop(&queue, &popped, ITEM_SIZE));
	TEST_ASSERT_EQUAL_UINT32(10u, popped);
	TEST_ASSERT_EQUAL_UINT8((uint8_t)(SMALL_CAPACITY - 1u), queue.size);

	queue_deinit(&queue);
}

/**
 * @brief Tests that pushing into a full queue is rejected with FULL and that
 * nothing is stored.
 */
static void test_push_into_full_queue_returns_full(void) {
	queue_t queue;
	uint32_t item = 99u;

	queue_init(&queue, buffer, ITEM_SIZE, SMALL_CAPACITY);
	push_items(&queue, SMALL_CAPACITY, 10u);

	TEST_ASSERT_EQUAL_INT(QUEUE_STATUS_FULL, queue_push(&queue, &item, ITEM_SIZE));
	TEST_ASSERT_EQUAL_UINT8(SMALL_CAPACITY, queue.size);

	queue_deinit(&queue);
}

/**
 * @brief Tests that popping from an empty queue is rejected with EMPTY and that
 * nothing is stored.
 */
static void test_pop_from_empty_queue_returns_empty(void) {
	queue_t queue;
	uint32_t popped = 0;

	queue_init(&queue, buffer, ITEM_SIZE, SMALL_CAPACITY);
	TEST_ASSERT_EQUAL_INT(QUEUE_STATUS_EMPTY, queue_pop(&queue, &popped, ITEM_SIZE));
	TEST_ASSERT_EQUAL_UINT8(0, queue.size);
	queue_deinit(&queue);
}

/**
 * @brief Tests that the item popped is the item pushed.
 */
static void test_push_then_pop_returns_the_pushed_item(void) {
	queue_t queue;
	uint32_t pushed = 0xDEADBEEFu;
	uint32_t popped = 0;

	queue_init(&queue, buffer, ITEM_SIZE, SMALL_CAPACITY);
	TEST_ASSERT_EQUAL_INT(QUEUE_STATUS_OK, queue_push(&queue, &pushed, ITEM_SIZE));
	TEST_ASSERT_EQUAL_INT(QUEUE_STATUS_OK, queue_pop(&queue, &popped, ITEM_SIZE));
	TEST_ASSERT_EQUAL_UINT32(pushed, popped);
	queue_deinit(&queue);
}

/**
 * @brief Tests that a rejected push leaves the stored items intact and still in
 * order.
 */
static void test_rejected_push_leaves_queue_unchanged(void) {
	queue_t queue;
	uint32_t popped = 0;
	uint32_t rejected = 555u;

	queue_init(&queue, buffer, ITEM_SIZE, SMALL_CAPACITY);
	push_items(&queue, SMALL_CAPACITY, 10u);
	TEST_ASSERT_EQUAL_INT(QUEUE_STATUS_FULL, queue_push(&queue, &rejected, ITEM_SIZE));

	for (uint8_t i = 0; i < SMALL_CAPACITY; i++) {
		TEST_ASSERT_EQUAL_INT(QUEUE_STATUS_OK, queue_pop(&queue, &popped, ITEM_SIZE));
		TEST_ASSERT_EQUAL_UINT32(10u + (uint32_t)i, popped);
	}

	queue_deinit(&queue);
}

/**
 * @brief Tests that a rejected pop leaves the queue usable.
 */
static void test_rejected_pop_leaves_queue_usable(void) {
	queue_t queue;
	uint32_t popped = 0;
	uint32_t item = 42u;

	queue_init(&queue, buffer, ITEM_SIZE, SMALL_CAPACITY);
	TEST_ASSERT_EQUAL_INT(QUEUE_STATUS_EMPTY, queue_pop(&queue, &popped, ITEM_SIZE));

	TEST_ASSERT_EQUAL_INT(QUEUE_STATUS_OK, queue_push(&queue, &item, ITEM_SIZE));
	TEST_ASSERT_EQUAL_INT(QUEUE_STATUS_OK, queue_pop(&queue, &popped, ITEM_SIZE));
	TEST_ASSERT_EQUAL_UINT32(item, popped);

	queue_deinit(&queue);
}

/**
 * @brief Tests that a capacity of one holds a single item and rejects the
 * second.
 */
static void test_capacity_of_one_holds_a_single_item(void) {
	queue_t queue;
	uint32_t item = 7u;
	uint32_t popped = 0;

	queue_init(&queue, buffer, ITEM_SIZE, (uint8_t)1);
	TEST_ASSERT_EQUAL_INT(QUEUE_STATUS_OK, queue_push(&queue, &item, ITEM_SIZE));
	TEST_ASSERT_EQUAL_INT(QUEUE_STATUS_FULL, queue_push(&queue, &item, ITEM_SIZE));
	TEST_ASSERT_EQUAL_INT(QUEUE_STATUS_OK, queue_pop(&queue, &popped, ITEM_SIZE));
	TEST_ASSERT_EQUAL_UINT32(item, popped);
	TEST_ASSERT_EQUAL_INT(QUEUE_STATUS_EMPTY, queue_pop(&queue, &popped, ITEM_SIZE));
	queue_deinit(&queue);
}

/**
 * @brief Tests that all 255 slots are reachable and drain in FIFO order.
 */
static void test_maximum_capacity_is_fully_usable(void) {
	queue_t queue;
	uint32_t popped = 0;
	uint32_t item = 0u;

	queue_init(&queue, buffer, ITEM_SIZE, MAX_CAPACITY);
	push_items(&queue, MAX_CAPACITY, 0u);
	TEST_ASSERT_EQUAL_INT(QUEUE_STATUS_FULL, queue_push(&queue, &item, ITEM_SIZE));

	for (uint8_t i = 0; i < MAX_CAPACITY; i++) {
		TEST_ASSERT_EQUAL_INT(QUEUE_STATUS_OK, queue_pop(&queue, &popped, ITEM_SIZE));
		TEST_ASSERT_EQUAL_UINT32((uint32_t)i, popped);
	}
	TEST_ASSERT_EQUAL_INT(QUEUE_STATUS_EMPTY, queue_pop(&queue, &popped, ITEM_SIZE));

	queue_deinit(&queue);
}

/**
 * @brief Tests that an item size of one byte is stored and returned in order, so
 * that no size is hard-coded to four.
 */
static void test_single_byte_items_are_stored_and_returned_in_order(void) {
	queue_t queue;

	queue_init(&queue, buffer, BYTE_ITEM_SIZE, SMALL_CAPACITY);

	for (uint8_t i = 0; i < SMALL_CAPACITY; i++) {
		uint8_t item = (uint8_t)(0xA0u + i);

		TEST_ASSERT_EQUAL_INT(QUEUE_STATUS_OK, queue_push(&queue, &item, BYTE_ITEM_SIZE));
	}

	for (uint8_t i = 0; i < SMALL_CAPACITY; i++) {
		uint8_t popped = 0;

		TEST_ASSERT_EQUAL_INT(QUEUE_STATUS_OK, queue_pop(&queue, &popped, BYTE_ITEM_SIZE));
		TEST_ASSERT_EQUAL_UINT8((uint8_t)(0xA0u + i), popped);
	}

	queue_deinit(&queue);
}

/**
 * @brief Tests that a nine-byte item is copied whole, without truncation.
 */
static void test_nine_byte_items_are_copied_without_truncation(void) {
	queue_t queue;
	nine_byte_item_t pushed;
	nine_byte_item_t popped;

	TEST_ASSERT_EQUAL_size_t(sizeof(nine_byte_item_t), NINE_ITEM_SIZE);

	for (uint8_t i = 0; i < NINE_ITEM_SIZE; i++) {
		pushed.bytes[i] = (uint8_t)(0x10u + i);
	}
	memset(&popped, 0, sizeof(popped));

	queue_init(&queue, buffer, NINE_ITEM_SIZE, SMALL_CAPACITY);
	TEST_ASSERT_EQUAL_INT(QUEUE_STATUS_OK, queue_push(&queue, &pushed, NINE_ITEM_SIZE));
	TEST_ASSERT_EQUAL_INT(QUEUE_STATUS_OK, queue_pop(&queue, &popped, NINE_ITEM_SIZE));
	TEST_ASSERT_EQUAL_MEMORY(pushed.bytes, popped.bytes, sizeof(pushed.bytes));

	queue_deinit(&queue);
}

/**
 * @brief Tests that queue_deinit destroys the queue's own mutex, exactly once.
 *
 * @note The remaining fields are deliberately not asserted: queue_deinit is not
 * required to reset them.
 */
static void test_deinit_destroys_the_mutex(void) {
	queue_t queue;

	queue_init(&queue, buffer, ITEM_SIZE, SMALL_CAPACITY);

	mutex_destroy_calls = 0;
	mutex_destroy_arg = NULL;
	queue_deinit(&queue);

	TEST_ASSERT_EQUAL_INT(1, mutex_destroy_calls);
	TEST_ASSERT_EQUAL_PTR(&queue.mutex, mutex_destroy_arg);
}

/**
 * @brief Tests that init, use and deinit may be repeated on the same object.
 */
static void test_deinit_then_reinit_is_safe(void) {
	queue_t queue;
	uint32_t item = 0;
	uint32_t popped = 0;

	for (uint32_t round = 0; round < 3; round++) {
		queue_init(&queue, buffer, ITEM_SIZE, SMALL_CAPACITY);

		item = 100u + round;
		TEST_ASSERT_EQUAL_INT(QUEUE_STATUS_OK, queue_push(&queue, &item, ITEM_SIZE));
		TEST_ASSERT_EQUAL_INT(QUEUE_STATUS_OK, queue_pop(&queue, &popped, ITEM_SIZE));
		TEST_ASSERT_EQUAL_UINT32(item, popped);
		TEST_ASSERT_EQUAL_INT(QUEUE_STATUS_EMPTY, queue_pop(&queue, &popped, ITEM_SIZE));

		queue_deinit(&queue);
	}
}


/* Unity harness -------------------------------------------------------------*/
void setUp(void) {}

void tearDown(void) {
	mutex_destroy_calls = 0;
	mutex_destroy_arg = NULL;
}

int main(void) {
	UNITY_BEGIN();

	RUN_TEST(test_init_sets_fields_and_marks_queue_empty);
	RUN_TEST(test_push_into_empty_queue_succeeds);
	RUN_TEST(test_pop_from_full_queue_returns_oldest_item);
	RUN_TEST(test_push_into_full_queue_returns_full);
	RUN_TEST(test_pop_from_empty_queue_returns_empty);
	RUN_TEST(test_push_then_pop_returns_the_pushed_item);
	RUN_TEST(test_rejected_push_leaves_queue_unchanged);
	RUN_TEST(test_rejected_pop_leaves_queue_usable);
	RUN_TEST(test_capacity_of_one_holds_a_single_item);
	RUN_TEST(test_maximum_capacity_is_fully_usable);
	RUN_TEST(test_single_byte_items_are_stored_and_returned_in_order);
	RUN_TEST(test_nine_byte_items_are_copied_without_truncation);
	RUN_TEST(test_deinit_destroys_the_mutex);
	RUN_TEST(test_deinit_then_reinit_is_safe);

	return UNITY_END();
}
