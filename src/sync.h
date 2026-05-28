#pragma once
#include <stdint.h>

#define SYNC_RTT_WINDOW    10
#define SYNC_RTT_SLACK_NS  10000      /* 10 µs */
#define SYNC_PERIOD_NS     50000000LL /* 50 ms */
#define SLEW_MAX_PPM       1000
#define STEP_THRESHOLD_NS  1000000LL  /* 1 ms */
#define STEP_CONFIRM_CNT   3

/* PI gains in Q16.16 (0.05 and 0.005). */
#define PI_KP_Q16 ((int64_t)3277)
#define PI_KI_Q16 ((int64_t)328)

typedef struct {
    int64_t  integrator_ppm;
    int64_t  rtt_window[SYNC_RTT_WINDOW];
    int      rtt_count;
    int      step_confirm;
} SyncState;

void    sync_init(SyncState *s);

/* Min-delay filter: accept only RTTs within min_rtt + slack. */
int     sync_filter(SyncState *s, int64_t rtt);

/* Positional PI on the closed-loop global offset (leader_global - follower_vclock).
 * Returns the absolute new rate in Q32.32 (nominal + Kp·θ + Ki·∫θdt, ppm-clamped).
 * If |θ| exceeds STEP_THRESHOLD_NS for STEP_CONFIRM_CNT consecutive samples,
 * sets *do_step=1 and *step_delta_ns=-θ so the caller applies a hard step. */
int64_t sync_pi(SyncState *s, int64_t theta_ns,
                int *out_do_step, int64_t *out_step_delta_ns);
