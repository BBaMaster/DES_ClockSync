#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <stdatomic.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/timerfd.h>

#include "clock.h"
#include "protocol.h"
#include "sync.h"
#include "election.h"
#include "net.h"
#include "gpio.h"
#include "telemetry.h"
#include "calibrate.h"

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

enum { TAG_SOCK = 1, TAG_TICK, TAG_HB, TAG_PULSE, TAG_PULSE_LOW };

#define PULSE_PERIOD_NS    1000000000LL  /* 1 s */
#define PULSE_HIGH_NS      10000000LL    /* 10 ms */

static void arm_abs(int fd, int64_t abs_ns)
{
    struct itimerspec it = { .it_value = {
        .tv_sec  = abs_ns / 1000000000LL,
        .tv_nsec = abs_ns % 1000000000LL,
    }};
    timerfd_settime(fd, TFD_TIMER_ABSTIME, &it, NULL);
}

/* Arm pulse_fd at the local-raw time when the vclock will next cross a
 * global 1-s boundary. Skip to the next-next boundary if we're within
 * one HIGH-pulse width of the current boundary to avoid double-firing
 * across a step. */
static void arm_pulse_next(int pulse_fd, VirtualClock *vc)
{
    int64_t global   = vclock_read(vc);
    int64_t next_bdy = ((global / PULSE_PERIOD_NS) + 1) * PULSE_PERIOD_NS;
    if (next_bdy - global < PULSE_HIGH_NS)
        next_bdy += PULSE_PERIOD_NS;
    int64_t target_local = vclock_local_for_global(vc, next_bdy);
    int64_t now_local    = mono_raw_ns();
    if (target_local <= now_local)
        target_local = now_local + 1000000LL; /* defensive: never arm in past */
    arm_abs(pulse_fd, target_local);
}

static void *sync_thread(void *arg)
{
    SyncThreadArg *a = arg;
    rt_thread_setup();

    int ep            = epoll_create1(EPOLL_CLOEXEC);
    int tick_fd       = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    int hb_fd         = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    int pulse_fd      = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    int pulse_low_fd  = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);

    struct itimerspec tick_it = {
        .it_interval = { .tv_sec = SYNC_PERIOD_NS / 1000000000LL,
                         .tv_nsec = SYNC_PERIOD_NS % 1000000000LL },
        .it_value    = { .tv_sec = SYNC_PERIOD_NS / 1000000000LL,
                         .tv_nsec = SYNC_PERIOD_NS % 1000000000LL },
    };
    timerfd_settime(tick_fd, 0, &tick_it, NULL);

    struct itimerspec hb_it = {
        .it_interval = { .tv_sec = HEARTBEAT_INTERVAL_NS / 1000000000LL,
                         .tv_nsec = HEARTBEAT_INTERVAL_NS % 1000000000LL },
        .it_value    = { .tv_sec = HEARTBEAT_INTERVAL_NS / 1000000000LL,
                         .tv_nsec = HEARTBEAT_INTERVAL_NS % 1000000000LL },
    };
    timerfd_settime(hb_fd, 0, &hb_it, NULL);

    struct epoll_event ev;
    ev.events  = EPOLLIN;
    ev.data.u32 = TAG_SOCK;
    epoll_ctl(ep, EPOLL_CTL_ADD, a->net->sock_fd, &ev);
    ev.data.u32 = TAG_TICK;
    epoll_ctl(ep, EPOLL_CTL_ADD, tick_fd, &ev);
    ev.data.u32 = TAG_HB;
    epoll_ctl(ep, EPOLL_CTL_ADD, hb_fd, &ev);
    ev.data.u32 = TAG_PULSE;
    epoll_ctl(ep, EPOLL_CTL_ADD, pulse_fd, &ev);
    ev.data.u32 = TAG_PULSE_LOW;
    epoll_ctl(ep, EPOLL_CTL_ADD, pulse_low_fd, &ev);

    /* Arm the first pulse against the freshly-initialized vclock. The first
     * sync step may shift the boundary, so the very first pulse can fire
     * "early"; subsequent pulses re-arm against the corrected vclock. */
    arm_pulse_next(pulse_fd, a->vc);

    uint16_t seq               = 0;
    int      pending           = 0;
    uint64_t pending_t1        = 0;
    int64_t  last_req_local_ns = 0;
    int      gpio23_stable     = 0;
    int64_t  gpio23_low_since  = 0;

    gpio_set(GPIO_HEALTH, 1);

    while (g_running) {
        struct epoll_event events[8];
        int n = epoll_wait(ep, events, 8, 200);
        if (n < 0) {
            if (errno == EINTR) continue;
            break;
        }
        int64_t now = mono_raw_ns();

        for (int i = 0; i < n; i++) {
            uint32_t tag = events[i].data.u32;

            if (tag == TAG_SOCK) {
                DrsPacket rpkt;
                int64_t   rx_ts;
                while (net_recv(a->net, &rpkt, &rx_ts) == 0) {
                    if (rpkt.node_id == a->node_id) continue;
                    NodeState state = a->es->state;

                    if (rpkt.msg_type == MSG_ANNOUNCE) {
                        election_on_announce(a->es, rpkt.node_id,
                                             rpkt.election_term, now);
                        /* Trigger one SYNC_REQ per ANNOUNCE, gated by pending. */
                        if (a->es->state == STATE_FOLLOWER && !pending) {
                            int64_t t1_raw = mono_raw_ns();
                            int64_t t1_vc  = vclock_read_at(a->vc, t1_raw);
                            DrsPacket req = {
                                .magic         = DRS_MAGIC,
                                .version       = DRS_VERSION,
                                .msg_type      = MSG_SYNC_REQ,
                                .seq           = seq++,
                                .node_id       = a->node_id,
                                .election_term = a->es->election_term,
                                .t1            = (uint64_t)t1_vc,
                            };
                            net_send(a->net, &req);
                            pending           = 1;
                            pending_t1        = (uint64_t)t1_vc;
                            last_req_local_ns = t1_raw;
                        }
                    } else if (rpkt.msg_type == MSG_SYNC_REQ &&
                               state == STATE_LEADER) {
                        int64_t t3_raw = mono_raw_ns();
                        /* Stamp t2/t3 in this node's vclock domain. With the
                         * leader's calibration baked into vclock_read, t2/t3
                         * carry "leader vclock at recv/send" so the follower's
                         * PI can drive vclock-to-vclock alignment, not vclock-
                         * to-raw. Pulse-fires-at-vclock=N then aligns on
                         * both ends. */
                        DrsPacket resp = {
                            .magic         = DRS_MAGIC,
                            .version       = DRS_VERSION,
                            .msg_type      = MSG_SYNC_RESP,
                            .flags         = FLAG_LEADER | FLAG_CALIBRATED,
                            .seq           = seq++,
                            .node_id       = a->node_id,
                            .election_term = a->es->election_term,
                            .t1            = rpkt.t1,
                            .t2            = (uint64_t)vclock_read_at(a->vc, rx_ts),
                            .t3            = (uint64_t)vclock_read_at(a->vc, t3_raw),
                        };
                        net_send(a->net, &resp);
                    } else if (rpkt.msg_type == MSG_SYNC_RESP &&
                               state == STATE_FOLLOWER && pending) {
                        if (rpkt.t1 != pending_t1) continue;

                        int64_t t1 = (int64_t)rpkt.t1;
                        int64_t t2 = (int64_t)rpkt.t2;
                        int64_t t3 = (int64_t)rpkt.t3;
                        int64_t t4 = vclock_read_at(a->vc, rx_ts);

                        int64_t rtt = (t4 - t1) - (t3 - t2);
                        if (rtt < 0) rtt = 0;

                        if (sync_filter(a->ss, rtt)) {
                            /* All four timestamps in vclock domain (each end's
                             * own vclock). PI then drives vclock-to-vclock,
                             * not vclock-to-raw — pulses align at vclock=N. */
                            int64_t fol_mid = (t1 + t4) / 2;
                            int64_t led_mid = (t2 + t3) / 2;
                            int64_t g_off   = led_mid - fol_mid;

                            int     do_step    = 0;
                            int64_t step_delta = 0;
                            int64_t new_rate   = sync_pi(a->ss, g_off,
                                                         &do_step, &step_delta);
                            int64_t t_now = mono_raw_ns();
                            if (do_step) {
                                vclock_step(a->vc, step_delta, t_now);
                            } else {
                                vclock_set_rate(a->vc, new_rate, t_now);
                            }

                            int64_t abs_off = g_off < 0 ? -g_off : g_off;
                            if (abs_off < 100000LL) {
                                if (gpio23_low_since == 0)
                                    gpio23_low_since = now;
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
                                .offset_ns    = g_off,
                                .rtt_ns       = rtt,
                                .rate_q32     = new_rate,
                            };
                            telem_write(a->telem, &tr);
                        }
                        pending = 0;
                    }
                }
            } else if (tag == TAG_TICK) {
                uint64_t exp;
                ssize_t  r = read(tick_fd, &exp, sizeof exp); (void)r;

                NodeState state = election_tick(a->es, now);

                /* Abandon a stuck pending request: either we're no longer
                 * a FOLLOWER (HOLDOVER on leader loss) or the response is
                 * overdue. Without this the next ANNOUNCE never gets a
                 * follow-up SYNC_REQ. */
                if (pending &&
                    (state != STATE_FOLLOWER ||
                     now - last_req_local_ns > HEARTBEAT_TIMEOUT_NS)) {
                    pending = 0;
                }

                if (state == STATE_LEADER) {
                    if (!gpio23_stable) {
                        if (gpio23_low_since == 0) gpio23_low_since = now;
                        if (now - gpio23_low_since >= 10000000000LL) {
                            gpio_set(GPIO_HEALTH, 0);
                            gpio23_stable = 1;
                        }
                    }
                    TelemRecord ltr = {
                        .timestamp_ns = now,
                        .state        = (int32_t)state,
                        .offset_ns    = 0,
                        .rtt_ns       = 0,
                        .rate_q32     = RATE_ONE,
                    };
                    telem_write(a->telem, &ltr);
                }
            } else if (tag == TAG_HB) {
                uint64_t exp;
                ssize_t  r = read(hb_fd, &exp, sizeof exp); (void)r;

                if (a->es->state == STATE_LEADER) {
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
                }
            } else if (tag == TAG_PULSE) {
                uint64_t exp;
                ssize_t  r = read(pulse_fd, &exp, sizeof exp); (void)r;
                gpio_set(GPIO_SYNC_PULSE, 1);
                arm_abs(pulse_low_fd, mono_raw_ns() + PULSE_HIGH_NS);
            } else if (tag == TAG_PULSE_LOW) {
                uint64_t exp;
                ssize_t  r = read(pulse_low_fd, &exp, sizeof exp); (void)r;
                gpio_set(GPIO_SYNC_PULSE, 0);
                arm_pulse_next(pulse_fd, a->vc);
            }
        }
    }

    close(tick_fd);
    close(hb_fd);
    close(pulse_fd);
    close(pulse_low_fd);
    close(ep);
    return NULL;
}

static void *drain_thread(void *arg)
{
    TelemCtx *telem = arg;
    struct timespec ts = { .tv_sec = 0, .tv_nsec = 10000000LL };
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

    uint32_t    node_id  = (uint32_t)atoi(argv[1]);
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

    /* Spec §5.4: self-latency calibration before entering sync states.
     * Single-shot write to a non-atomic field is safe — sync_thread has
     * not been spawned yet. */
    int64_t lat = 0;
    if (calibrate_loopback(&lat) == 0) {
        vc.latency_correction_ns = lat;
        fprintf(stderr, "calibration: LatencyCorrection = %lld ns\n",
                (long long)lat);
    } else {
        fprintf(stderr, "calibration: failed, using 0\n");
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
    pthread_create(&st, NULL, sync_thread,  &arg);
    pthread_create(&dt, NULL, drain_thread, &telem);

    pthread_join(st, NULL);
    pthread_join(dt, NULL);

    gpio_cleanup();
    net_close(&net);
    telem_close(&telem);
    return 0;
}
