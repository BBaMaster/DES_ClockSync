#include "test_runner.h"
#include "sync.h"
#include "clock.h"
#include <stdint.h>

static void test_filter_accepts_min(TestCtx *ctx)
{
    SyncState s;
    sync_init(&s);
    EXPECT(ctx, sync_filter(&s, 1000) == 1);
}

static void test_filter_rejects_spike(TestCtx *ctx)
{
    SyncState s;
    sync_init(&s);
    for (int i = 0; i < SYNC_RTT_WINDOW; i++)
        sync_filter(&s, 1000);
    EXPECT(ctx, sync_filter(&s, 1000 + SYNC_RTT_SLACK_NS + 1) == 0);
}

static void test_filter_accepts_within_slack(TestCtx *ctx)
{
    SyncState s;
    sync_init(&s);
    for (int i = 0; i < SYNC_RTT_WINDOW; i++)
        sync_filter(&s, 1000);
    EXPECT(ctx, sync_filter(&s, 1000 + SYNC_RTT_SLACK_NS) == 1);
}

static void test_pi_proportional(TestCtx *ctx)
{
    SyncState s;
    sync_init(&s);
    int do_step = 0; int64_t step_delta = 0;
    int64_t rate = sync_pi(&s, 500, &do_step, &step_delta); /* 500 ns θ */
    EXPECT(ctx, do_step == 0);
    /* Small θ → rate near nominal, well inside the ±SLEW_MAX clamp. */
    int64_t slew_max_q32 = PPM_TO_RATE(SLEW_MAX_PPM);
    EXPECT(ctx, rate > RATE_ONE - slew_max_q32);
    EXPECT(ctx, rate < RATE_ONE + slew_max_q32);
}

static void test_pi_clamp(TestCtx *ctx)
{
    SyncState s;
    sync_init(&s);
    /* Drive integrator with sub-step θ values (under STEP_THRESHOLD) so
     * we exercise the slew path's clamp, not the step branch. */
    int do_step = 0; int64_t step_delta = 0;
    int64_t rate = RATE_ONE;
    for (int i = 0; i < 1000; i++) {
        rate = sync_pi(&s, STEP_THRESHOLD_NS - 1, &do_step, &step_delta);
    }
    int64_t slew_max_q32 = PPM_TO_RATE(SLEW_MAX_PPM);
    EXPECT(ctx, rate <= RATE_ONE + slew_max_q32);
}

static void test_step_requires_3_consecutive(TestCtx *ctx)
{
    SyncState s;
    sync_init(&s);
    int     do_step    = 0;
    int64_t step_delta = 0;
    sync_pi(&s, 2000000LL, &do_step, &step_delta);
    EXPECT(ctx, do_step == 0);
    sync_pi(&s, 2000000LL, &do_step, &step_delta);
    EXPECT(ctx, do_step == 0);
    sync_pi(&s, 2000000LL, &do_step, &step_delta);
    EXPECT(ctx, do_step == 1);
    EXPECT(ctx, step_delta == 2000000LL);
}

static void test_step_resets_on_small(TestCtx *ctx)
{
    SyncState s;
    sync_init(&s);
    int     do_step    = 0;
    int64_t step_delta = 0;
    sync_pi(&s, 2000000LL, &do_step, &step_delta);
    sync_pi(&s, 2000000LL, &do_step, &step_delta);
    sync_pi(&s, 100LL,     &do_step, &step_delta);
    EXPECT(ctx, do_step == 0);
    sync_pi(&s, 2000000LL, &do_step, &step_delta);
    EXPECT(ctx, do_step == 0);
    sync_pi(&s, 2000000LL, &do_step, &step_delta);
    EXPECT(ctx, do_step == 0);
    sync_pi(&s, 2000000LL, &do_step, &step_delta);
    EXPECT(ctx, do_step == 1);
}

static void test_integrator_clamp(TestCtx *ctx)
{
    SyncState s;
    sync_init(&s);
    int do_step = 0; int64_t step_delta = 0;
    for (int i = 0; i < 1000; i++)
        sync_pi(&s, STEP_THRESHOLD_NS - 1, &do_step, &step_delta);
    EXPECT(ctx, s.integrator_ppm <= SLEW_MAX_PPM);
    EXPECT(ctx, s.integrator_ppm >= -SLEW_MAX_PPM);
}

void test_sync_suite(TestCtx *ctx)
{
    printf("--- sync ---\n");
    test_filter_accepts_min(ctx);
    test_filter_rejects_spike(ctx);
    test_filter_accepts_within_slack(ctx);
    test_pi_proportional(ctx);
    test_pi_clamp(ctx);
    test_step_requires_3_consecutive(ctx);
    test_step_resets_on_small(ctx);
    test_integrator_clamp(ctx);
}
