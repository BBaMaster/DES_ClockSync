#include "sync.h"
#include "clock.h"
#include <string.h>
#include <stdint.h>

/* Kp=0.05, Ki=0.005 scaled to Q32.32 */
#define KP_Q32 ((int64_t)(0.05  * (double)(1LL << 32)))
#define KI_Q32 ((int64_t)(0.005 * (double)(1LL << 32)))

/* 1000 ppm in Q32.32 */
#define SLEW_MAX_Q32 PPM_TO_RATE(SLEW_MAX_PPM)

void sync_init(SyncState *s)
{
    memset(s, 0, sizeof(*s));
    s->kp_num = KP_Q32;
    s->ki_num = KI_Q32;
    for (int i = 0; i < SYNC_RTT_WINDOW; i++)
        s->rtt_window[i] = INT64_MAX;
}

void sync_compute(int64_t t1, int64_t t2, int64_t t3, int64_t t4,
                  int64_t *offset_out, int64_t *rtt_out)
{
    *offset_out = ((t2 - t1) + (t3 - t4)) / 2;
    *rtt_out    = (t4 - t1) - (t3 - t2);
}

int sync_filter(SyncState *s, int64_t rtt)
{
    /* Update rolling window */
    int slot = s->rtt_count % SYNC_RTT_WINDOW;
    s->rtt_window[slot] = rtt;
    s->rtt_count++;

    /* Find minimum RTT in window */
    int64_t min_rtt = INT64_MAX;
    int filled = s->rtt_count < SYNC_RTT_WINDOW ? s->rtt_count : SYNC_RTT_WINDOW;
    for (int i = 0; i < filled; i++) {
        if (s->rtt_window[i] < min_rtt)
            min_rtt = s->rtt_window[i];
    }

    return (rtt <= min_rtt + SYNC_RTT_SLACK_NS) ? 1 : 0;
}

int64_t sync_pi(SyncState *s, int64_t offset)
{
    /* Accumulate integrator; clamp to ±1000 ppm equivalent in ns/cycle */
    s->integrator += offset;

    int64_t integ_max = (int64_t)SLEW_MAX_PPM * SYNC_PERIOD_NS / 1000000LL;
    if (s->integrator >  integ_max) s->integrator =  integ_max;
    if (s->integrator < -integ_max) s->integrator = -integ_max;

    /* rate_delta = Kp*offset + Ki*integrator, in Q32.32 */
    __int128 p_term = (__int128)s->kp_num * offset;
    __int128 i_term = (__int128)s->ki_num * s->integrator;
    int64_t delta = (int64_t)((p_term + i_term) >> 32);

    /* Clamp to ±SLEW_MAX */
    if (delta >  SLEW_MAX_Q32) delta =  SLEW_MAX_Q32;
    if (delta < -SLEW_MAX_Q32) delta = -SLEW_MAX_Q32;

    return delta;
}

int sync_needs_step(SyncState *s, int64_t offset)
{
    int64_t abs_offset = offset < 0 ? -offset : offset;
    if (abs_offset > STEP_THRESHOLD_NS) {
        s->step_confirm++;
    } else {
        s->step_confirm = 0;
    }
    return (s->step_confirm >= STEP_CONFIRM_CNT) ? 1 : 0;
}
