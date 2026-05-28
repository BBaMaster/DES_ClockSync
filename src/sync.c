#include "sync.h"
#include "clock.h"
#include <string.h>
#include <stdint.h>

void sync_init(SyncState *s)
{
    memset(s, 0, sizeof(*s));
    for (int i = 0; i < SYNC_RTT_WINDOW; i++)
        s->rtt_window[i] = INT64_MAX;
}

int sync_filter(SyncState *s, int64_t rtt)
{
    int slot = s->rtt_count % SYNC_RTT_WINDOW;
    s->rtt_window[slot] = rtt;
    s->rtt_count++;

    int64_t min_rtt = INT64_MAX;
    int filled = s->rtt_count < SYNC_RTT_WINDOW ? s->rtt_count : SYNC_RTT_WINDOW;
    for (int i = 0; i < filled; i++)
        if (s->rtt_window[i] < min_rtt) min_rtt = s->rtt_window[i];

    return (rtt <= min_rtt + SYNC_RTT_SLACK_NS) ? 1 : 0;
}

int64_t sync_pi(SyncState *s, int64_t theta_ns,
                int *out_do_step, int64_t *out_step_delta_ns)
{
    *out_do_step       = 0;
    *out_step_delta_ns = 0;

    int64_t abs_theta = theta_ns < 0 ? -theta_ns : theta_ns;

    /* Hard step on confirmed large error; reset integrator so slew doesn't
     * fight the step afterwards. */
    if (abs_theta > STEP_THRESHOLD_NS) {
        s->step_confirm++;
        if (s->step_confirm >= STEP_CONFIRM_CNT) {
            *out_do_step       = 1;
            /* θ = leader_global − follower_vclock. Positive θ means the
             * follower is behind; to drive θ → 0 we must add θ to the
             * vclock. step_delta carries the same sign as the correction
             * we want the vclock to take. */
            *out_step_delta_ns = theta_ns;
            s->integrator_ppm  = 0;
            s->step_confirm    = 0;
        }
        return RATE_ONE;
    }
    s->step_confirm = 0;

    /* Slew PI. ppm = (K_Q16 · θ_ns · 1e6 / period_ns) / 2^16.
     * Explicit /65536 (not >>16) so symmetric truncation handles negative θ. */
    int64_t kp_ppm = (PI_KP_Q16 * theta_ns * 1000000LL / SYNC_PERIOD_NS) / 65536LL;
    int64_t ki_ppm = (PI_KI_Q16 * theta_ns * 1000000LL / SYNC_PERIOD_NS) / 65536LL;

    s->integrator_ppm += ki_ppm;
    if (s->integrator_ppm >  SLEW_MAX_PPM) s->integrator_ppm =  SLEW_MAX_PPM;
    if (s->integrator_ppm < -SLEW_MAX_PPM) s->integrator_ppm = -SLEW_MAX_PPM;

    int64_t total_ppm = kp_ppm + s->integrator_ppm;
    if (total_ppm >  SLEW_MAX_PPM) total_ppm =  SLEW_MAX_PPM;
    if (total_ppm < -SLEW_MAX_PPM) total_ppm = -SLEW_MAX_PPM;

    /* Positional: rate is set absolutely from nominal each tick. The integrator
     * carries the steady-state ppm; adding to current rate would integrate twice. */
    return RATE_ONE + total_ppm * (RATE_ONE / 1000000LL);
}
