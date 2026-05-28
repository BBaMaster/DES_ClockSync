#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <stdatomic.h>

#include "clock.h"
#include "protocol.h"
#include "sync.h"
#include "election.h"
#include "net.h"
#include "gpio.h"
#include "telemetry.h"

#ifdef PLATFORM_rpi
#include <sched.h>
#include <sys/mman.h>
static void rt_thread_setup(void)
{
    mlockall(MCL_CURRENT | MCL_FUTURE);
    struct sched_param sp = { .sched_priority = 85 };
    pthread_setschedparam(pthread_self(), SCHED_FIFO, &sp);
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(3, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpuset), &cpuset);
}
#else
static void rt_thread_setup(void) {}
#endif

static volatile int g_running = 1;
static void sig_handler(int s) { (void)s; g_running = 0; }

typedef struct {
    VirtualClock  *vc;
    ElectionState *es;
    SyncState     *ss;
    NetCtx        *net;
    TelemCtx      *telem;
    uint32_t       node_id;
} SyncThreadArg;

static void *sync_thread(void *arg)
{
    SyncThreadArg *a = arg;
    rt_thread_setup();

    uint16_t seq         = 0;
    int64_t  gpio23_low_since = 0;
    int      gpio23_stable    = 0;
    int64_t  next_wake   = mono_raw_ns() + SYNC_PERIOD_NS;

    gpio_set(GPIO_HEALTH, 1); /* HIGH = not yet synced */

    while (g_running) {
        int64_t now = mono_raw_ns();
        NodeState state = election_tick(a->es, now);

        if (state == STATE_LEADER) {
            /* Leader sends heartbeat ANNOUNCE */
            DrsPacket pkt = {
                .magic         = DRS_MAGIC,
                .version       = DRS_VERSION,
                .msg_type      = MSG_ANNOUNCE,
                .flags         = FLAG_LEADER | FLAG_CALIBRATED,
                .seq           = seq++,
                .node_id       = a->node_id,
                .election_term = a->es->election_term,
            };
            net_send(a->net, &pkt);

            /* GPIO 18: 10 ms pulse once per global second */
            int64_t global = vclock_read(a->vc);
            int64_t second_phase = global % 1000000000LL;
            if (second_phase < 10000000LL) { /* within first 10 ms of second */
                gpio_set(GPIO_SYNC_PULSE, 1);
            } else {
                gpio_set(GPIO_SYNC_PULSE, 0);
            }

            /* Health indicator: always stable for leader (offset ≈ 0) */
            if (!gpio23_stable) {
                if (gpio23_low_since == 0) gpio23_low_since = now;
                if (now - gpio23_low_since >= 10000000000LL) {
                    gpio_set(GPIO_HEALTH, 0);
                    gpio23_stable = 1;
                }
            }

            /* Leader telemetry: emit once per sync period so the receiver
             * can confirm the node is alive and see current rate */
            TelemRecord ltr = {
                .timestamp_ns = now,
                .state        = (int32_t)state,
                .offset_ns    = 0,
                .rtt_ns       = 0,
                .rate_q32     = atomic_load(&a->vc->rate),
            };
            telem_write(a->telem, &ltr);

        } else if (state == STATE_FOLLOWER) {
            /* Send SYNC_REQ */
            int64_t t1 = vclock_read(a->vc);
            DrsPacket req = {
                .magic         = DRS_MAGIC,
                .version       = DRS_VERSION,
                .msg_type      = MSG_SYNC_REQ,
                .seq           = seq++,
                .node_id       = a->node_id,
                .election_term = a->es->election_term,
                .t1            = (uint64_t)t1,
            };
            net_send(a->net, &req);
        }

        /* Non-blocking receive */
        DrsPacket rpkt;
        int64_t rx_ts;
        if (net_recv(a->net, &rpkt, &rx_ts) == 0) {
            if (rpkt.node_id != a->node_id) {
                if (rpkt.msg_type == MSG_ANNOUNCE) {
                    election_on_announce(a->es, rpkt.node_id,
                                         rpkt.election_term, now);
                } else if (rpkt.msg_type == MSG_SYNC_RESP &&
                           state == STATE_FOLLOWER) {
                    int64_t offset, rtt;
                    sync_compute((int64_t)rpkt.t1, (int64_t)rpkt.t2,
                                 (int64_t)rpkt.t3, rx_ts,
                                 &offset, &rtt);

                    if (sync_filter(a->ss, rtt)) {
                        if (sync_needs_step(a->ss, offset)) {
                            int64_t cur = atomic_load(&a->vc->offset);
                            atomic_store_explicit(&a->vc->offset,
                                cur + offset, memory_order_release);
                            a->ss->step_confirm = 0;
                        } else {
                            int64_t delta = sync_pi(a->ss, offset);
                            int64_t rate  = atomic_load(&a->vc->rate);
                            atomic_store_explicit(&a->vc->rate,
                                rate + delta, memory_order_release);
                        }

                        /* Health tracking */
                        int64_t abs_off = offset < 0 ? -offset : offset;
                        if (abs_off < 100000LL) {
                            if (gpio23_low_since == 0) gpio23_low_since = now;
                            if (now - gpio23_low_since >= 10000000000LL &&
                                !gpio23_stable) {
                                gpio_set(GPIO_HEALTH, 0);
                                gpio23_stable = 1;
                            }
                        } else {
                            gpio23_low_since = 0;
                            if (gpio23_stable) {
                                gpio_set(GPIO_HEALTH, 1);
                                gpio23_stable = 0;
                            }
                        }

                        TelemRecord tr = {
                            .timestamp_ns = now,
                            .state        = (int32_t)state,
                            .offset_ns    = offset,
                            .rtt_ns       = rtt,
                            .rate_q32     = atomic_load(&a->vc->rate),
                        };
                        telem_write(a->telem, &tr);
                    }
                }
            }
        }

        /* Yield until next period (CLOCK_MONOTONIC_RAW not supported by
         * clock_nanosleep on Linux; compute remaining time and use nanosleep) */
        int64_t remaining = next_wake - mono_raw_ns();
        if (remaining > 0) {
            struct timespec ts = {
                .tv_sec  = remaining / 1000000000LL,
                .tv_nsec = remaining % 1000000000LL,
            };
            nanosleep(&ts, NULL);
        }
        next_wake += SYNC_PERIOD_NS;
    }

    return NULL;
}

static void *drain_thread(void *arg)
{
    TelemCtx *telem = arg;
    struct timespec ts = { .tv_sec = 0, .tv_nsec = 10000000LL }; /* 10 ms */
    while (g_running) {
        telem_drain(telem);
        nanosleep(&ts, NULL);
    }
    return NULL;
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(stderr, "usage: drs_sync <node_id> [telem_dest_ip]\n");
        return 1;
    }

    uint32_t node_id = (uint32_t)atoi(argv[1]);
    const char *telem_ip = argc >= 3 ? argv[2] : "127.0.0.1";

    signal(SIGINT,  sig_handler);
    signal(SIGTERM, sig_handler);

    VirtualClock  vc;  vclock_init(&vc);
    ElectionState es;  election_init(&es, node_id);
    SyncState     ss;  sync_init(&ss);
    NetCtx        net;
    TelemCtx      telem;

    gpio_init();

    if (net_init(&net, node_id) < 0) {
        fprintf(stderr, "net_init failed\n");
        return 1;
    }
    if (telem_init(&telem, telem_ip) < 0) {
        fprintf(stderr, "telem_init failed\n");
        return 1;
    }

    SyncThreadArg arg = {
        .vc      = &vc,
        .es      = &es,
        .ss      = &ss,
        .net     = &net,
        .telem   = &telem,
        .node_id = node_id,
    };

    pthread_t st, dt;
    pthread_create(&st, NULL, sync_thread, &arg);
    pthread_create(&dt, NULL, drain_thread, &telem);

    pthread_join(st, NULL);
    pthread_join(dt, NULL);

    gpio_cleanup();
    net_close(&net);
    telem_close(&telem);
    return 0;
}
