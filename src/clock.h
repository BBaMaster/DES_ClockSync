#pragma once
#include <stdint.h>

/* Q32.32 fixed-point: 0x0000000100000000 == 1.0 */
#define RATE_ONE   ((int64_t)0x0000000100000000LL)
#define PPM_TO_RATE(ppm) ((int64_t)((ppm) * (RATE_ONE / 1000000LL)))

typedef struct {
    /* written only by sync thread; read lock-free via atomics */
    _Atomic int64_t rate;     /* Q32.32 */
    _Atomic int64_t offset;   /* nanoseconds */
    int64_t latency_correction_ns;
} VirtualClock;

void     vclock_init(VirtualClock *vc);
int64_t  vclock_read(const VirtualClock *vc);
int64_t  mono_raw_ns(void);
