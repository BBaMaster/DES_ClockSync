#include "clock.h"
#include <stdatomic.h>
#include <time.h>

int64_t mono_raw_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    return (int64_t)ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

static inline int64_t mul_q32(int64_t delta_local, int64_t rate)
{
    __int128 prod = (__int128)delta_local * (__int128)rate;
    return (int64_t)(prod >> 32);
}

static inline void writer_begin(VirtualClock *vc)
{
    atomic_fetch_add_explicit(&vc->seq, 1u, memory_order_relaxed);
    atomic_thread_fence(memory_order_release);
}

static inline void writer_end(VirtualClock *vc)
{
    atomic_thread_fence(memory_order_release);
    atomic_fetch_add_explicit(&vc->seq, 1u, memory_order_relaxed);
}

void vclock_init(VirtualClock *vc)
{
    atomic_store_explicit(&vc->seq, 0u, memory_order_relaxed);
    vc->rate                  = RATE_ONE;
    vc->latency_correction_ns = 0;
    vc->base_local_ns         = mono_raw_ns();
    vc->base_global_ns        = vc->base_local_ns;
}

int64_t vclock_read_at(const VirtualClock *vc, int64_t t_local_ns)
{
    int64_t rate, lat, base_local, base_global;
    unsigned s;
    for (;;) {
        s = atomic_load_explicit(&vc->seq, memory_order_acquire);
        if (s & 1u) continue;
        rate        = vc->rate;
        lat         = vc->latency_correction_ns;
        base_local  = vc->base_local_ns;
        base_global = vc->base_global_ns;
        atomic_thread_fence(memory_order_acquire);
        if (atomic_load_explicit(&vc->seq, memory_order_acquire) == s) break;
    }
    return base_global + mul_q32(t_local_ns - base_local, rate) - lat;
}

int64_t vclock_read(const VirtualClock *vc)
{
    return vclock_read_at(vc, mono_raw_ns());
}

int64_t vclock_local_for_global(const VirtualClock *vc, int64_t t_global_ns)
{
    int64_t rate, lat, base_local, base_global;
    unsigned s;
    for (;;) {
        s = atomic_load_explicit(&vc->seq, memory_order_acquire);
        if (s & 1u) continue;
        rate        = vc->rate;
        lat         = vc->latency_correction_ns;
        base_local  = vc->base_local_ns;
        base_global = vc->base_global_ns;
        atomic_thread_fence(memory_order_acquire);
        if (atomic_load_explicit(&vc->seq, memory_order_acquire) == s) break;
    }
    /* t_global = base_global + (t_local - base_local) * rate / 2^32 - lat
     * → t_local = base_local + (t_global - base_global + lat) * 2^32 / rate */
    int64_t y = t_global_ns - base_global + lat;
    __int128 num = ((__int128)y) << 32;
    int64_t delta_local = (int64_t)(num / (__int128)rate);
    return base_local + delta_local;
}

void vclock_step(VirtualClock *vc, int64_t delta_ns, int64_t t_local_ns)
{
    writer_begin(vc);
    int64_t cur_global = vc->base_global_ns
                       + mul_q32(t_local_ns - vc->base_local_ns, vc->rate)
                       - vc->latency_correction_ns;
    vc->base_local_ns  = t_local_ns;
    vc->base_global_ns = cur_global + delta_ns + vc->latency_correction_ns;
    writer_end(vc);
}

void vclock_set_rate(VirtualClock *vc, int64_t new_rate_q32, int64_t t_local_ns)
{
    writer_begin(vc);
    int64_t cur_global_no_lat = vc->base_global_ns
                              + mul_q32(t_local_ns - vc->base_local_ns, vc->rate);
    vc->base_local_ns  = t_local_ns;
    vc->base_global_ns = cur_global_no_lat;
    vc->rate           = new_rate_q32;
    writer_end(vc);
}
