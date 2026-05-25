#include "test_runner.h"
#include "sync.h"
#include "clock.h"
#include <stdint.h>

static void test_offset_calc(TestCtx *ctx)
{
    /* T1=100, T2=150, T3=160, T4=220
     * offset = ((150-100) + (160-220)) / 2 = (50 - 60) / 2 = -5
     * rtt    = (220-100) - (160-150) = 120 - 10 = 110 */
    int64_t offset, rtt;
    sync_compute(100, 150, 160, 220, &offset, &rtt);
    EXPECT(ctx, offset == -5);
    EXPECT(ctx, rtt    == 110);
}

static void test_offset_positive(TestCtx *ctx)
{
    /* T1=100, T2=180, T3=190, T4=250
     * offset = ((180-100) + (190-250)) / 2 = (80 - 60) / 2 = 10 */
    int64_t offset, rtt;
    sync_compute(100, 180, 190, 250, &offset, &rtt);
    EXPECT(ctx, offset == 10);
}

static void test_filter_accepts_min(TestCtx *ctx)
{
    SyncState s;
    sync_init(&s);
    /* First sample: always accepted (window empty, it becomes min) */
    EXPECT(ctx, sync_filter(&s, 1000) == 1);
}

static void test_filter_rejects_spike(TestCtx *ctx)
{
    SyncState s;
    sync_init(&s);
    /* Fill window with 1000 ns RTT */
    for (int i = 0; i < SYNC_RTT_WINDOW; i++)
        sync_filter(&s, 1000);
    /* A spike 20 µs above min should be rejected */
    EXPECT(ctx, sync_filter(&s, 1000 + SYNC_RTT_SLACK_NS + 1) == 0);
}

static void test_filter_accepts_within_slack(TestCtx *ctx)
{
    SyncState s;
    sync_init(&s);
    for (int i = 0; i < SYNC_RTT_WINDOW; i++)
        sync_filter(&s, 1000);
    /* Within slack: accepted */
    EXPECT(ctx, sync_filter(&s, 1000 + SYNC_RTT_SLACK_NS) == 1);
}

static void test_pi_proportional(TestCtx *ctx)
{
    SyncState s;
    sync_init(&s);
    /* Small offset: should produce a small rate delta, not a step */
    int64_t delta = sync_pi(&s, 500); /* 500 ns offset */
    EXPECT(ctx, delta != 0);
    /* Delta should be much smaller than SLEW_MAX */
    int64_t slew_max = PPM_TO_RATE(SLEW_MAX_PPM);
    int64_t abs_delta = delta < 0 ? -delta : delta;
    EXPECT(ctx, abs_delta < slew_max);
}

static void test_pi_clamp(TestCtx *ctx)
{
    SyncState s;
    sync_init(&s);
    /* Very large offset: delta clamped to slew max */
    int64_t delta = sync_pi(&s, 10000000000LL);
    int64_t slew_max = PPM_TO_RATE(SLEW_MAX_PPM);
    EXPECT(ctx, delta <= slew_max);
}

static void test_step_requires_3_consecutive(TestCtx *ctx)
{
    SyncState s;
    sync_init(&s);
    /* 2 consecutive large samples: no step yet */
    EXPECT(ctx, sync_needs_step(&s, 2000000LL) == 0);
    EXPECT(ctx, sync_needs_step(&s, 2000000LL) == 0);
    /* 3rd: step */
    EXPECT(ctx, sync_needs_step(&s, 2000000LL) == 1);
}

static void test_step_resets_on_small(TestCtx *ctx)
{
    SyncState s;
    sync_init(&s);
    sync_needs_step(&s, 2000000LL);
    sync_needs_step(&s, 2000000LL);
    /* Small offset resets counter */
    EXPECT(ctx, sync_needs_step(&s, 100LL) == 0);
    /* Need 3 more large samples */
    EXPECT(ctx, sync_needs_step(&s, 2000000LL) == 0);
    EXPECT(ctx, sync_needs_step(&s, 2000000LL) == 0);
    EXPECT(ctx, sync_needs_step(&s, 2000000LL) == 1);
}

static void test_integrator_clamp(TestCtx *ctx)
{
    SyncState s;
    sync_init(&s);
    /* Drive integrator to max with large repeated offsets */
    for (int i = 0; i < 1000; i++)
        sync_pi(&s, 1000000000LL);
    int64_t integ_max = (int64_t)SLEW_MAX_PPM * SYNC_PERIOD_NS / 1000000LL;
    EXPECT(ctx, s.integrator <= integ_max);
}

void test_sync_suite(TestCtx *ctx)
{
    printf("--- sync ---\n");
    test_offset_calc(ctx);
    test_offset_positive(ctx);
    test_filter_accepts_min(ctx);
    test_filter_rejects_spike(ctx);
    test_filter_accepts_within_slack(ctx);
    test_pi_proportional(ctx);
    test_pi_clamp(ctx);
    test_step_requires_3_consecutive(ctx);
    test_step_resets_on_small(ctx);
    test_integrator_clamp(ctx);
}
