#include "clock.h"
#include <stdatomic.h>
#include <time.h>

void vclock_init(VirtualClock *vc)
{
    atomic_store_explicit(&vc->rate, RATE_ONE, memory_order_release);
    atomic_store_explicit(&vc->offset, 0, memory_order_release);
    vc->latency_correction_ns = 0;
}

int64_t mono_raw_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    return (int64_t)ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

int64_t vclock_read(const VirtualClock *vc)
{
    int64_t rate   = atomic_load_explicit(&vc->rate,   memory_order_acquire);
    int64_t offset = atomic_load_explicit(&vc->offset, memory_order_acquire);
    int64_t raw    = mono_raw_ns();

    /* Tglobal = (raw * rate >> 32) + offset - latency */
    /* Use __int128 to avoid overflow in the multiply */
    __int128 scaled = (__int128)raw * rate;
    int64_t tglobal = (int64_t)(scaled >> 32) + offset - vc->latency_correction_ns;
    return tglobal;
}
