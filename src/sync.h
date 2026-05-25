#pragma once
#include <stdint.h>

#define SYNC_RTT_WINDOW   10
#define SYNC_RTT_SLACK_NS 10000   /* 10 µs */
#define SYNC_PERIOD_NS    50000000LL  /* 50 ms */
#define SLEW_MAX_PPM      1000
#define STEP_THRESHOLD_NS 1000000LL  /* 1 ms */
#define STEP_CONFIRM_CNT  3

/* All fields in nanoseconds; rate in Q32.32 */
typedef struct {
    int64_t  kp_num;    /* scaled: actual Kp = kp_num / 2^32 */
    int64_t  ki_num;
    int64_t  integrator;   /* accumulated error, nanoseconds */
    int64_t  rtt_window[SYNC_RTT_WINDOW];
    int      rtt_count;
    int      step_confirm; /* consecutive large-offset samples */
} SyncState;

void    sync_init(SyncState *s);

/* Compute offset (theta) and RTT from T1..T4 timestamps. */
void    sync_compute(int64_t t1, int64_t t2, int64_t t3, int64_t t4,
                     int64_t *offset_out, int64_t *rtt_out);

/* Returns 1 if sample passes min-delay filter, 0 if rejected. */
int     sync_filter(SyncState *s, int64_t rtt);

/* Run PI controller. Returns rate adjustment (Q32.32 delta). */
int64_t sync_pi(SyncState *s, int64_t offset);

/* Returns 1 if a hard step should be applied, 0 for slew. */
int     sync_needs_step(SyncState *s, int64_t offset);
