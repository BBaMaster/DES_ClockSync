#pragma once
#include <stdint.h>

/* Q32.32 fixed-point: 0x0000000100000000 == 1.0 */
#define RATE_ONE   ((int64_t)0x0000000100000000LL)
#define PPM_TO_RATE(ppm) ((int64_t)((ppm) * (RATE_ONE / 1000000LL)))

/* Virtual clock with base-rebase model:
 *   T_global = base_global + (T_local - base_local) * rate / 2^32 - latency
 * Rate scales only the delta since the last rebase, so step corrections and
 * rate updates compose cleanly (rebasing preserves current global time).
 * Fields are seqlock-protected: writer increments seq twice; readers retry
 * on odd seq. */
typedef struct {
    _Atomic unsigned seq;
    int64_t rate;                  /* Q32.32 */
    int64_t latency_correction_ns;
    int64_t base_local_ns;
    int64_t base_global_ns;
} VirtualClock;

void    vclock_init(VirtualClock *vc);
int64_t mono_raw_ns(void);

/* Read global time at a given local-raw timestamp (lock-free, retries on
 * concurrent writer). */
int64_t vclock_read_at(const VirtualClock *vc, int64_t t_local_ns);

/* Convenience: vclock_read_at(mono_raw_ns()). */
int64_t vclock_read(const VirtualClock *vc);

/* Inverse: given a desired global time, return the local-raw time at which
 * the vclock will reach it under the current rate. */
int64_t vclock_local_for_global(const VirtualClock *vc, int64_t t_global_ns);

/* Apply a step correction (delta_ns to global). Rebases base. */
void    vclock_step(VirtualClock *vc, int64_t delta_ns, int64_t t_local_ns);

/* Set the rate; rebases base so the global-time curve stays continuous. */
void    vclock_set_rate(VirtualClock *vc, int64_t new_rate_q32, int64_t t_local_ns);
