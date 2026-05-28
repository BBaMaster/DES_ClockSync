#include "test_runner.h"
#include "clock.h"

static void test_init(TestCtx *ctx)
{
    VirtualClock vc;
    vclock_init(&vc);
    EXPECT(ctx, vc.rate == RATE_ONE);
    EXPECT(ctx, vc.latency_correction_ns == 0);
    EXPECT(ctx, vc.base_local_ns == vc.base_global_ns);
}

static void test_rate_one_no_offset(TestCtx *ctx)
{
    VirtualClock vc;
    vclock_init(&vc);
    int64_t raw  = mono_raw_ns();
    int64_t glob = vclock_read(&vc);
    int64_t diff = glob - raw;
    if (diff < 0) diff = -diff;
    EXPECT(ctx, diff < 1000000LL);
}

static void test_step_advances_global(TestCtx *ctx)
{
    VirtualClock vc;
    vclock_init(&vc);
    int64_t t = mono_raw_ns();
    int64_t before = vclock_read_at(&vc, t);
    vclock_step(&vc, 500000LL, t); /* +500 µs */
    int64_t after = vclock_read_at(&vc, t);
    EXPECT(ctx, after - before == 500000LL);
}

static void test_set_rate_keeps_continuity(TestCtx *ctx)
{
    /* Rebase on set_rate must keep global time continuous at the rebase
     * instant — otherwise the curve would jump every PI update. */
    VirtualClock vc;
    vclock_init(&vc);
    int64_t t = mono_raw_ns();
    int64_t before = vclock_read_at(&vc, t);
    vclock_set_rate(&vc, RATE_ONE + PPM_TO_RATE(100), t);
    int64_t after = vclock_read_at(&vc, t);
    EXPECT(ctx, before == after);
}

static void test_inverse_round_trip(TestCtx *ctx)
{
    VirtualClock vc;
    vclock_init(&vc);
    int64_t t_local  = mono_raw_ns() + 500000000LL; /* 500 ms in the future */
    int64_t t_global = vclock_read_at(&vc, t_local);
    int64_t back     = vclock_local_for_global(&vc, t_global);
    int64_t err      = back - t_local;
    if (err < 0) err = -err;
    EXPECT(ctx, err < 1000LL);
}

static void test_latency_correction(TestCtx *ctx)
{
    VirtualClock vc;
    vclock_init(&vc);
    vc.latency_correction_ns = 100000LL;
    int64_t t    = mono_raw_ns();
    int64_t glob = vclock_read_at(&vc, t);
    EXPECT(ctx, glob < t + 1000000LL);
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
    int64_t r = PPM_TO_RATE(1000);
    EXPECT(ctx, r > 0);
    double ratio = (double)r / (double)RATE_ONE;
    EXPECT(ctx, ratio > 0.00099 && ratio < 0.00101);
}

void test_clock_suite(TestCtx *ctx)
{
    printf("--- clock ---\n");
    test_init(ctx);
    test_rate_one_no_offset(ctx);
    test_step_advances_global(ctx);
    test_set_rate_keeps_continuity(ctx);
    test_inverse_round_trip(ctx);
    test_latency_correction(ctx);
    test_monotonic(ctx);
    test_q32_rate_ppm(ctx);
}
