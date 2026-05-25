#include "test_runner.h"
#include "clock.h"
#include <stdatomic.h>

static void test_init(TestCtx *ctx)
{
    VirtualClock vc;
    vclock_init(&vc);
    EXPECT(ctx, atomic_load(&vc.rate)   == RATE_ONE);
    EXPECT(ctx, atomic_load(&vc.offset) == 0);
    EXPECT(ctx, vc.latency_correction_ns == 0);
}

static void test_rate_one_no_offset(TestCtx *ctx)
{
    /* With rate=1.0 and offset=0, vclock_read() ≈ mono_raw_ns() */
    VirtualClock vc;
    vclock_init(&vc);
    int64_t raw  = mono_raw_ns();
    int64_t glob = vclock_read(&vc);
    int64_t diff = glob - raw;
    if (diff < 0) diff = -diff;
    /* Within 1 ms — accounts for time between the two calls */
    EXPECT(ctx, diff < 1000000LL);
}

static void test_offset_applied(TestCtx *ctx)
{
    VirtualClock vc;
    vclock_init(&vc);
    atomic_store(&vc.offset, 500000LL); /* +500 µs */
    int64_t raw  = mono_raw_ns();
    int64_t glob = vclock_read(&vc);
    EXPECT(ctx, glob > raw);
}

static void test_latency_correction(TestCtx *ctx)
{
    VirtualClock vc;
    vclock_init(&vc);
    vc.latency_correction_ns = 100000LL; /* 100 µs subtracted */
    int64_t raw  = mono_raw_ns();
    int64_t glob = vclock_read(&vc);
    EXPECT(ctx, glob < raw + 1000000LL); /* glob roughly raw - 100 µs */
}

static void test_monotonic(TestCtx *ctx)
{
    VirtualClock vc;
    vclock_init(&vc);
    int64_t prev = vclock_read(&vc);
    for (int i = 0; i < 100; i++) {
        int64_t cur = vclock_read(&vc);
        EXPECT(ctx, cur >= prev);
        prev = cur;
    }
}

static void test_q32_rate_ppm(TestCtx *ctx)
{
    /* PPM_TO_RATE(1000) should be approximately 1000/1e6 * 2^32 */
    int64_t r = PPM_TO_RATE(1000);
    EXPECT(ctx, r > 0);
    /* 1000 ppm of RATE_ONE: r / RATE_ONE ≈ 0.001 */
    double ratio = (double)r / (double)RATE_ONE;
    EXPECT(ctx, ratio > 0.00099 && ratio < 0.00101);
}

void test_clock_suite(TestCtx *ctx)
{
    printf("--- clock ---\n");
    test_init(ctx);
    test_rate_one_no_offset(ctx);
    test_offset_applied(ctx);
    test_latency_correction(ctx);
    test_monotonic(ctx);
    test_q32_rate_ppm(ctx);
}
