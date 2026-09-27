#include "pin.h"
#include <stdio.h>
#ifdef __linux__
#include <sched.h>
#endif

int pin_cpu_for_worker(const int worker_id, const int *allowed, const int allowed_count) {
    if (worker_id < 0 || allowed == NULL || allowed_count <= 0) return -1;
    return allowed[worker_id % allowed_count];
}

int pin_worker(const int worker_id) {
#ifdef __linux__
    if (worker_id < 0) return 0;
    cpu_set_t set;
    if (sched_getaffinity(0, sizeof(set), &set) != 0) return -1;
    int allowed[CPU_SETSIZE];
    int n = 0;
    for (int c = 0; c < CPU_SETSIZE; c++) {
        if (CPU_ISSET(c, &set)) allowed[n++] = c;
    }
    const int cpu = pin_cpu_for_worker(worker_id, allowed, n);
    if (cpu < 0) return 0;
    CPU_ZERO(&set);
    CPU_SET(cpu, &set);
    if (sched_setaffinity(0, sizeof(set), &set) != 0) {
        perror("cexpress: sched_setaffinity");
        return -1;
    }
    return 0;
#else
    (void)worker_id;
    return 0;
#endif
}
