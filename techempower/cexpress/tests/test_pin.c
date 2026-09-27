/* pin_cpu_for_worker (pure mapping) and pin_worker (Linux: the process ends up on exactly that CPU). */
#include "../src/pin.h"
#include <assert.h>
#include <stdio.h>
#ifdef __linux__
#include <sched.h>
#endif

static void test_worker_gets_its_own_cpu(void) {
    const int allowed[] = {0, 1, 2, 3};
    for (int w = 0; w < 4; w++) assert(pin_cpu_for_worker(w, allowed, 4) == w);
}

static void test_more_workers_than_cpus_wraps(void) {
    const int allowed[] = {0, 1, 2};
    assert(pin_cpu_for_worker(3, allowed, 3) == 0);
    assert(pin_cpu_for_worker(7, allowed, 3) == 1);
}

static void test_uses_only_allowed_cpus(void) {
    /* e.g. a container limited with --cpuset-cpus=2,5,9 */
    const int allowed[] = {2, 5, 9};
    assert(pin_cpu_for_worker(0, allowed, 3) == 2);
    assert(pin_cpu_for_worker(1, allowed, 3) == 5);
    assert(pin_cpu_for_worker(2, allowed, 3) == 9);
}

static void test_no_worker_or_no_cpu_is_refused(void) {
    const int allowed[] = {0, 1};
    assert(pin_cpu_for_worker(-1, allowed, 2) == -1); /* not a cluster worker */
    assert(pin_cpu_for_worker(0, allowed, 0) == -1);
    assert(pin_cpu_for_worker(0, NULL, 2) == -1);
}

static void test_pin_worker_negative_id_is_a_noop(void) {
    assert(pin_worker(-1) == 0);
}

#ifdef __linux__
static void test_pin_worker_sets_affinity(void) {
    cpu_set_t before;
    assert(sched_getaffinity(0, sizeof(before), &before) == 0);
    int first = -1;
    for (int c = 0; c < CPU_SETSIZE && first < 0; c++) {
        if (CPU_ISSET(c, &before)) first = c;
    }
    assert(pin_worker(0) == 0); /* worker 0 -> the first allowed CPU */
    cpu_set_t after;
    assert(sched_getaffinity(0, sizeof(after), &after) == 0);
    assert(CPU_COUNT(&after) == 1 && CPU_ISSET(first, &after));
    assert(sched_setaffinity(0, sizeof(before), &before) == 0);
}
#endif

int main(void) {
    test_worker_gets_its_own_cpu();
    test_more_workers_than_cpus_wraps();
    test_uses_only_allowed_cpus();
    test_no_worker_or_no_cpu_is_refused();
    test_pin_worker_negative_id_is_a_noop();
#ifdef __linux__
    test_pin_worker_sets_affinity();
#endif
    printf("all pin tests passed\n");
    return 0;
}
