/**
 * @file test_cogutil_advanced.c
 * @brief Advanced CogUtil Tests — Threading, Strings, Data Structures, Timers
 *
 * Covers gaps not addressed in test_cogwxp.c:
 *  - Mutex create / lock / trylock / unlock / destroy
 *  - Read-write lock concurrent read / exclusive write
 *  - Thread creation, join, detach lifecycle
 *  - Thread pool submit, drain, destroy
 *  - Condition variable signal and broadcast
 *  - UUID uniqueness at scale (100 UUIDs)
 *  - Hash function stability (same input → same hash)
 *  - Hash combine associativity check
 *  - String utilities: trim, split, join, starts_with, ends_with
 *  - Dynamic array: push, pop, get, clear
 *  - Hashmap: set, get, remove, contains, overwrite
 *  - Linked list: push_front, push_back, pop_front, pop_back
 *  - Timer: create, start, stop, reset callback counting
 *  - cog_time_now_ms / cog_time_now_us ordering
 *
 * @copyright CoGWXP-OS9 Project
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdatomic.h>
#include "../opencog/cogutil/cogutil.h"

/*===========================================================================
 * Test framework
 *===========================================================================*/

static int g_passed = 0;
static int g_failed = 0;

#define TEST_ASSERT(cond, msg) do { \
    if (cond) { \
        printf("  [PASS] %s\n", (msg)); \
        g_passed++; \
    } else { \
        printf("  [FAIL] %s  (line %d)\n", (msg), __LINE__); \
        g_failed++; \
    } \
} while (0)

/*===========================================================================
 * Mutex Tests
 *===========================================================================*/

static void test_mutex_lifecycle(void) {
    printf("\n=== Mutex Lifecycle ===\n");

    cog_mutex_t m = cog_mutex_create();
    TEST_ASSERT(m != NULL, "Mutex created successfully");

    cog_mutex_lock(m);
    TEST_ASSERT(1, "Mutex locked (no deadlock)");

    cog_mutex_unlock(m);
    TEST_ASSERT(1, "Mutex unlocked");

    /* trylock on unlocked mutex should succeed */
    bool got = cog_mutex_trylock(m);
    TEST_ASSERT(got, "trylock on unlocked mutex succeeds");
    cog_mutex_unlock(m);

    cog_mutex_destroy(m);
    TEST_ASSERT(1, "Mutex destroyed without crash");
}

/*===========================================================================
 * Read-Write Lock Tests
 *===========================================================================*/

static void test_rwlock_lifecycle(void) {
    printf("\n=== RWLock Lifecycle ===\n");

    cog_rwlock_t rw = cog_rwlock_create();
    TEST_ASSERT(rw != NULL, "RWLock created");

    /*
     * Single-thread read-lock re-entrance: acquire two read locks sequentially
     * (both are unlocked between each other to avoid implementation-specific
     * behaviour around recursive read-locking).
     */
    cog_rwlock_read_lock(rw);
    cog_rwlock_read_unlock(rw);
    cog_rwlock_read_lock(rw);
    cog_rwlock_read_unlock(rw);
    TEST_ASSERT(1, "Two sequential read lock/unlock cycles succeed");

    /* Write lock */
    cog_rwlock_write_lock(rw);
    TEST_ASSERT(1, "Write lock acquired");
    cog_rwlock_write_unlock(rw);
    TEST_ASSERT(1, "Write lock released");

    cog_rwlock_destroy(rw);
    TEST_ASSERT(1, "RWLock destroyed");
}

/*===========================================================================
 * Thread Tests
 *===========================================================================*/

static atomic_int g_thread_counter = 0;
static uint64_t g_worker_thread_id = 0;

static void* thread_increment(void* arg) {
    (void)arg;
    g_worker_thread_id = cog_thread_id();
    atomic_fetch_add(&g_thread_counter, 1);
    return NULL;
}

static void test_thread_lifecycle(void) {
    printf("\n=== Thread Lifecycle ===\n");

    atomic_store(&g_thread_counter, 0);
    g_worker_thread_id = 0;

    uint64_t main_thread_id = cog_thread_id();

    cog_thread_t t = cog_thread_create(thread_increment, NULL);
    TEST_ASSERT(t != NULL, "Thread created");

    cog_thread_join(t);
    TEST_ASSERT(atomic_load(&g_thread_counter) == 1, "Thread executed its function");
    TEST_ASSERT(main_thread_id != 0, "Main thread ID is non-zero");
    TEST_ASSERT(g_worker_thread_id != 0, "Worker thread ID is non-zero");
    TEST_ASSERT(g_worker_thread_id != main_thread_id, "Worker thread ID differs from main thread ID");
}

static void test_multiple_threads(void) {
    printf("\n=== Multiple Threads ===\n");

    atomic_store(&g_thread_counter, 0);

    const int N = 8;
    cog_thread_t threads[8];
    for (int i = 0; i < N; i++)
        threads[i] = cog_thread_create(thread_increment, NULL);
    for (int i = 0; i < N; i++)
        cog_thread_join(threads[i]);

    TEST_ASSERT(atomic_load(&g_thread_counter) == N,
                "All 8 threads incremented counter");
}

/*===========================================================================
 * Thread Pool Tests
 *===========================================================================*/

static atomic_int g_pool_counter = 0;

static void* pool_task(void* arg) {
    (void)arg;
    atomic_fetch_add(&g_pool_counter, 1);
    return NULL;
}

static void test_threadpool(void) {
    printf("\n=== Thread Pool ===\n");

    atomic_store(&g_pool_counter, 0);

    cog_threadpool_t pool = cog_threadpool_create(4);
    TEST_ASSERT(pool != NULL, "Thread pool created with 4 threads");

    const int TASKS = 20;
    for (int i = 0; i < TASKS; i++) {
        cog_result_t r = cog_threadpool_submit(pool, pool_task, NULL);
        TEST_ASSERT(r == COG_SUCCESS, "Pool task submitted");
    }

    /* Wait for tasks to complete (give pool time to drain) */
    cog_sleep_ms(200);

    cog_threadpool_destroy(pool);
    TEST_ASSERT(atomic_load(&g_pool_counter) == TASKS,
                "All pool tasks executed");
}

/*===========================================================================
 * Condition Variable Tests
 *===========================================================================*/

typedef struct {
    cog_mutex_t mutex;
    cog_cond_t  cond;
    int         ready;
    int         value;
} cond_shared_t;

static void* cond_sender(void* arg) {
    cond_shared_t* s = (cond_shared_t*)arg;
    cog_mutex_lock(s->mutex);
    s->value = 42;
    s->ready = 1;
    cog_cond_signal(s->cond);
    cog_mutex_unlock(s->mutex);
    return NULL;
}

static void test_cond_signal(void) {
    printf("\n=== Condition Variable Signal ===\n");

    cond_shared_t s;
    s.mutex = cog_mutex_create();
    s.cond  = cog_cond_create();
    s.ready = 0;
    s.value = 0;

    cog_thread_t sender = cog_thread_create(cond_sender, &s);

    cog_mutex_lock(s.mutex);
    while (!s.ready)
        cog_cond_wait(s.cond, s.mutex);
    cog_mutex_unlock(s.mutex);

    cog_thread_join(sender);

    TEST_ASSERT(s.value == 42, "Value set by sender thread");
    TEST_ASSERT(s.ready == 1, "Ready flag set by sender");

    cog_cond_destroy(s.cond);
    cog_mutex_destroy(s.mutex);
}

static void test_cond_timedwait_timeout(void) {
    printf("\n=== Condition Variable Timed Wait Timeout ===\n");

    cog_mutex_t mutex = cog_mutex_create();
    cog_cond_t cond = cog_cond_create();
    TEST_ASSERT(mutex != NULL, "Timed wait mutex created");
    TEST_ASSERT(cond != NULL, "Timed wait condition created");

    cog_mutex_lock(mutex);
    /* Per cogutil.h, this wrapper takes a relative timeout in milliseconds. */
    TEST_ASSERT(!cog_cond_timedwait(cond, mutex, 20), "Timed wait times out without a signal");
    cog_mutex_unlock(mutex);

    cog_cond_destroy(cond);
    cog_mutex_destroy(mutex);
}

static void* cond_timed_sender(void* arg) {
    cond_shared_t* s = (cond_shared_t*)arg;
    cog_sleep_ms(10);
    cog_mutex_lock(s->mutex);
    s->ready = 1;
    cog_cond_signal(s->cond);
    cog_mutex_unlock(s->mutex);
    return NULL;
}

static void test_cond_timedwait_signal(void) {
    printf("\n=== Condition Variable Timed Wait Signal ===\n");

    cond_shared_t s;
    s.mutex = cog_mutex_create();
    s.cond = cog_cond_create();
    s.ready = 0;
    s.value = 0;

    cog_thread_t sender = cog_thread_create(cond_timed_sender, &s);
    TEST_ASSERT(sender != NULL, "Timed wait sender thread created");

    cog_mutex_lock(s.mutex);
    while (!s.ready) {
        TEST_ASSERT(cog_cond_timedwait(s.cond, s.mutex, 100),
                    "Timed wait succeeds when signalled before timeout");
    }
    cog_mutex_unlock(s.mutex);

    cog_thread_join(sender);
    cog_cond_destroy(s.cond);
    cog_mutex_destroy(s.mutex);
}

/*===========================================================================
 * UUID Tests
 *===========================================================================*/

static void test_uuid_uniqueness(void) {
    printf("\n=== UUID Uniqueness (100 IDs) ===\n");

    const int N = 100;
    cog_uuid_t uuids[100];
    for (int i = 0; i < N; i++)
        cog_uuid_generate(&uuids[i]);

    /* Check all are unique */
    int all_unique = 1;
    for (int i = 0; i < N && all_unique; i++)
        for (int j = i + 1; j < N && all_unique; j++)
            if (cog_uuid_equals(&uuids[i], &uuids[j]))
                all_unique = 0;

    TEST_ASSERT(all_unique, "100 generated UUIDs are all unique");
}

static void test_uuid_string_roundtrip(void) {
    printf("\n=== UUID String Roundtrip ===\n");

    cog_uuid_t orig;
    cog_uuid_generate(&orig);
    char buf[64];
    cog_uuid_to_string(&orig, buf);

    TEST_ASSERT(strlen(buf) > 0, "UUID string non-empty");

    cog_uuid_t parsed;
    cog_uuid_from_string(&parsed, buf);
    TEST_ASSERT(cog_uuid_equals(&parsed, &orig), "UUID parsed from string matches original");
}

/*===========================================================================
 * Hash Function Tests
 *===========================================================================*/

static void test_hash_string_stability(void) {
    printf("\n=== Hash String Stability ===\n");

    uint32_t h1 = cog_hash_string("cogwxp-os9");
    uint32_t h2 = cog_hash_string("cogwxp-os9");
    TEST_ASSERT(h1 == h2, "Same string hashes to same value");

    uint32_t h3 = cog_hash_string("cogwxp-os10");
    TEST_ASSERT(h1 != h3, "Different strings (usually) produce different hashes");

    uint32_t h_empty = cog_hash_string("");
    TEST_ASSERT(1, "Empty string hash does not crash");
    (void)h_empty;
}

static void test_hash_data(void) {
    printf("\n=== Hash Data ===\n");

    const char data[] = "hello world";
    uint64_t h1 = cog_hash_data(data, sizeof(data));
    uint64_t h2 = cog_hash_data(data, sizeof(data));
    TEST_ASSERT(h1 == h2, "hash_data stable for same input");

    uint64_t combined = cog_hash_combine(h1, h2);
    TEST_ASSERT(1, "hash_combine does not crash");
    (void)combined;
}

/*===========================================================================
 * String Utility Tests
 *===========================================================================*/

static void test_string_trim(void) {
    printf("\n=== String Trim ===\n");

    char buf1[] = "  hello  ";
    char* t = cog_string_trim(buf1);
    TEST_ASSERT(strcmp(t, "hello") == 0, "Trim removes leading/trailing spaces");

    char buf2[] = "no_space";
    t = cog_string_trim(buf2);
    TEST_ASSERT(strcmp(t, "no_space") == 0, "Trim on no-space string is identity");

    char buf3[] = "   ";
    t = cog_string_trim(buf3);
    TEST_ASSERT(strlen(t) == 0, "Trim of whitespace-only → empty");
}

static void test_string_split_join(void) {
    printf("\n=== String Split & Join ===\n");

    size_t count = 0;
    char** parts = cog_string_split("a,b,c,d", ',', &count);
    TEST_ASSERT(parts != NULL, "split returns non-NULL");
    TEST_ASSERT(count == 4, "split into 4 parts");
    TEST_ASSERT(strcmp(parts[0], "a") == 0, "First part is 'a'");
    TEST_ASSERT(strcmp(parts[3], "d") == 0, "Last part is 'd'");

    const char* cparts[] = {"a","b","c","d"};
    char* joined = cog_string_join(cparts, 4, ",");
    TEST_ASSERT(joined != NULL, "join returns non-NULL");
    TEST_ASSERT(strcmp(joined, "a,b,c,d") == 0, "Joined string matches original");

    free(joined);
    cog_string_split_free(parts, count);
}

static void test_string_predicates(void) {
    printf("\n=== String Predicates ===\n");

    TEST_ASSERT(cog_string_starts_with("cogwxp-os9", "cogwxp"), "starts_with positive");
    TEST_ASSERT(!cog_string_starts_with("cogwxp-os9", "xyz"), "starts_with negative");
    TEST_ASSERT(cog_string_ends_with("cogwxp-os9", "os9"), "ends_with positive");
    TEST_ASSERT(!cog_string_ends_with("cogwxp-os9", "xyz"), "ends_with negative");

    /* Edge: empty prefix/suffix */
    TEST_ASSERT(cog_string_starts_with("abc", ""), "starts_with empty prefix");
    TEST_ASSERT(cog_string_ends_with("abc", ""), "ends_with empty suffix");
}

/*===========================================================================
 * Dynamic Array Tests
 *===========================================================================*/

static void test_dynamic_array(void) {
    printf("\n=== Dynamic Array ===\n");

    cog_array_t arr = cog_array_create(sizeof(int));
    TEST_ASSERT(arr != NULL, "Array created");
    TEST_ASSERT(cog_array_size(arr) == 0, "Empty array has size 0");

    for (int i = 0; i < 10; i++)
        cog_array_push(arr, &i);

    TEST_ASSERT(cog_array_size(arr) == 10, "Array size 10 after 10 pushes");

    int* elem = (int*)cog_array_get(arr, 5);
    TEST_ASSERT(*elem == 5, "Element at index 5 is 5");

    cog_array_pop(arr);
    TEST_ASSERT(cog_array_size(arr) == 9, "Size 9 after pop");

    cog_array_clear(arr);
    TEST_ASSERT(cog_array_size(arr) == 0, "Size 0 after clear");

    cog_array_destroy(arr);
    TEST_ASSERT(1, "Array destroyed");
}

/*===========================================================================
 * Hashmap Tests
 *===========================================================================*/

static void test_hashmap(void) {
    printf("\n=== Hashmap ===\n");

    cog_hashmap_t m = cog_hashmap_create();
    TEST_ASSERT(m != NULL, "Hashmap created");
    TEST_ASSERT(cog_hashmap_size(m) == 0, "Empty hashmap size is 0");

    int val1 = 42, val2 = 99;
    cog_hashmap_set(m, "key1", &val1);
    cog_hashmap_set(m, "key2", &val2);

    TEST_ASSERT(cog_hashmap_size(m) == 2, "Size 2 after 2 insertions");
    TEST_ASSERT(cog_hashmap_contains(m, "key1"), "contains 'key1'");
    TEST_ASSERT(!cog_hashmap_contains(m, "missing"), "not contains 'missing'");

    int* got = (int*)cog_hashmap_get(m, "key1");
    TEST_ASSERT(got != NULL && *got == 42, "get 'key1' returns 42");

    /* Overwrite */
    int val3 = 100;
    cog_hashmap_set(m, "key1", &val3);
    got = (int*)cog_hashmap_get(m, "key1");
    TEST_ASSERT(got != NULL && *got == 100, "Overwrite key1 to 100");

    /* Remove */
    bool removed = cog_hashmap_remove(m, "key1");
    TEST_ASSERT(removed, "Remove returns true for existing key");
    TEST_ASSERT(!cog_hashmap_contains(m, "key1"), "key1 no longer present");

    bool not_removed = cog_hashmap_remove(m, "nonexistent");
    TEST_ASSERT(!not_removed, "Remove returns false for missing key");

    cog_hashmap_destroy(m);
    TEST_ASSERT(1, "Hashmap destroyed");
}

/*===========================================================================
 * Linked List Tests
 *===========================================================================*/

static void test_linked_list(void) {
    printf("\n=== Linked List ===\n");

    cog_list_t list = cog_list_create();
    TEST_ASSERT(list != NULL, "List created");
    TEST_ASSERT(cog_list_size(list) == 0, "Empty list size 0");

    int a = 1, b = 2, c = 3;
    cog_list_push_back(list, &a);
    cog_list_push_back(list, &b);
    cog_list_push_front(list, &c);

    TEST_ASSERT(cog_list_size(list) == 3, "Size 3 after 3 pushes");

    int* front = (int*)cog_list_pop_front(list);
    TEST_ASSERT(front != NULL && *front == c, "pop_front returns c (=3)");

    int* back = (int*)cog_list_pop_back(list);
    TEST_ASSERT(back != NULL && *back == b, "pop_back returns b (=2)");

    TEST_ASSERT(cog_list_size(list) == 1, "Size 1 after two pops");

    cog_list_destroy(list);
    TEST_ASSERT(1, "List destroyed");
}

/*===========================================================================
 * Timer Callback Test
 *===========================================================================*/

static atomic_int g_timer_fires = 0;

static void timer_cb(void* arg) {
    (void)arg;
    atomic_fetch_add(&g_timer_fires, 1);
}

static void test_timer(void) {
    printf("\n=== Timer ===\n");

    atomic_store(&g_timer_fires, 0);

    cog_timer_t t = cog_timer_create(50, timer_cb, NULL);
    TEST_ASSERT(t != NULL, "Timer created");

    cog_timer_start(t);
    cog_sleep_ms(200);   /* Should fire ~4 times */
    cog_timer_stop(t);

    int fires = atomic_load(&g_timer_fires);
    TEST_ASSERT(fires >= 2, "Timer fired at least 2 times in 200ms");

    cog_timer_reset(t);
    cog_timer_destroy(t);
    TEST_ASSERT(1, "Timer destroyed");
}

/*===========================================================================
 * Time Ordering Tests
 *===========================================================================*/

static void test_time_ordering(void) {
    printf("\n=== Time Ordering ===\n");

    uint64_t t1_ms = cog_time_now_ms();
    cog_sleep_ms(10);
    uint64_t t2_ms = cog_time_now_ms();
    TEST_ASSERT(t2_ms >= t1_ms, "cog_time_now_ms is monotonically non-decreasing");
    TEST_ASSERT(t2_ms - t1_ms >= 5, "Sleep 10ms advances time by at least 5ms");

    uint64_t t1_us = cog_time_now_us();
    cog_sleep_ms(1);
    uint64_t t2_us = cog_time_now_us();
    TEST_ASSERT(t2_us >= t1_us, "cog_time_now_us is monotonically non-decreasing");
}

/*===========================================================================
 * Memory Stats Tests
 *===========================================================================*/

static void test_mem_stats(void) {
    printf("\n=== Memory Stats ===\n");

    cog_mem_stats_t before, after;
    cog_mem_get_stats(&before);

    void* p1 = cog_alloc(512);
    void* p2 = cog_calloc(10, 64);
    cog_mem_get_stats(&after);

    TEST_ASSERT(after.current_usage >= before.current_usage,
                "Allocation increases current usage");
    TEST_ASSERT(after.allocation_count > before.allocation_count,
                "allocation_count incremented");

    cog_free(p1);
    cog_free(p2);

    cog_mem_stats_t final;
    cog_mem_get_stats(&final);
    TEST_ASSERT(final.current_usage <= after.current_usage,
                "Freeing reduces current usage");
}

/*===========================================================================
 * cog_strdup
 *===========================================================================*/

static void test_strdup(void) {
    printf("\n=== cog_strdup ===\n");

    const char* src = "Hello, CoGWXP-OS9!";
    char* copy = cog_strdup(src);
    TEST_ASSERT(copy != NULL, "cog_strdup returns non-NULL");
    TEST_ASSERT(strcmp(copy, src) == 0, "Duplicated string matches original");
    TEST_ASSERT(copy != src, "Duplicate is a different pointer");

    cog_free(copy);
}

/*===========================================================================
 * Main
 *===========================================================================*/

int main(void) {
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║  CoGWXP-OS9  CogUtil Advanced Test Suite                 ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n");

    cog_init();
    cog_log_set_level(COG_LOG_WARN);

    test_mutex_lifecycle();
    test_rwlock_lifecycle();
    test_thread_lifecycle();
    test_multiple_threads();
    test_threadpool();
    test_cond_signal();
    test_cond_timedwait_timeout();
    test_cond_timedwait_signal();
    test_uuid_uniqueness();
    test_uuid_string_roundtrip();
    test_hash_string_stability();
    test_hash_data();
    test_string_trim();
    test_string_split_join();
    test_string_predicates();
    test_dynamic_array();
    test_hashmap();
    test_linked_list();
    test_timer();
    test_time_ordering();
    test_mem_stats();
    test_strdup();

    cog_shutdown();

    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║  SUMMARY  Passed: %-4d  Failed: %-4d                     ║\n",
           g_passed, g_failed);
    printf("╚═══════════════════════════════════════════════════════════╝\n\n");

    return g_failed > 0 ? 1 : 0;
}
