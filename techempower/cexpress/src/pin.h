#ifndef TFB_PIN_H
#define TFB_PIN_H

/*
 * One CPU per worker process: worker i runs only on the i-th CPU it was allowed to use at startup
 * (wrapping when there are more workers than CPUs). MEASURED locally (fortune, 512 connections, three
 * alternating pairs): +4.0% over unpinned workers; db unchanged. CEXPRESS_PIN_WORKERS=0 turns it off.
 */

/* The CPU for `worker_id` among allowed[0..allowed_count), or -1 (no worker id, or no CPU). Pure. */
int pin_cpu_for_worker(int worker_id, const int *allowed, int allowed_count);

/* Pins the calling process to pin_cpu_for_worker(worker_id, <its current affinity>). Linux only; a no-op
 * elsewhere and for worker_id < 0 (not a cluster worker). Returns 0, or -1 if the kernel refused. */
int pin_worker(int worker_id);

#endif
