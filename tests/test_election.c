#include "test_runner.h"
#include "election.h"
#include "clock.h"

static void test_initial_state(TestCtx *ctx)
{
    ElectionState e;
    election_init(&e, 42);
    EXPECT(ctx, e.state   == STATE_GROUND);
    EXPECT(ctx, e.node_id == 42);
}

static void test_ground_expires(TestCtx *ctx)
{
    ElectionState e;
    int64_t t0 = mono_raw_ns();
    election_init(&e, 1);
    /* Before expiry */
    NodeState s = election_tick(&e, t0 + GROUND_DURATION_NS / 2);
    EXPECT(ctx, s == STATE_GROUND);
    /* After expiry */
    s = election_tick(&e, t0 + GROUND_DURATION_NS + 1);
    EXPECT(ctx, s == STATE_CALIBRATION);
}

static void test_listen_to_candidate(TestCtx *ctx)
{
    ElectionState e;
    int64_t t0 = 1000000000LL; /* 1 s baseline */
    election_init(&e, 5);
    e.state          = STATE_LISTEN;
    e.state_entry_ns = t0;
    /* No heartbeat seen, timeout triggers CANDIDATE */
    NodeState s = election_tick(&e, t0 + HEARTBEAT_TIMEOUT_NS + 1);
    EXPECT(ctx, s == STATE_CANDIDATE);
}

static void test_candidate_becomes_leader(TestCtx *ctx)
{
    ElectionState e;
    int64_t t0 = 1000000000LL;
    election_init(&e, 3);
    e.state          = STATE_CANDIDATE;
    e.state_entry_ns = t0;
    NodeState s = election_tick(&e, t0 + 1);
    EXPECT(ctx, s == STATE_LEADER);
}

static void test_lower_id_demotes_leader(TestCtx *ctx)
{
    ElectionState e;
    int64_t t0 = 1000000000LL;
    election_init(&e, 10);
    e.state          = STATE_LEADER;
    e.state_entry_ns = t0;
    e.leader_id      = 10;
    /* Node with lower ID announces itself */
    NodeState s = election_on_announce(&e, 2, e.election_term, t0 + 1);
    EXPECT(ctx, s == STATE_FOLLOWER);
    EXPECT(ctx, e.leader_id == 2);
}

static void test_higher_id_ignored(TestCtx *ctx)
{
    ElectionState e;
    int64_t t0 = 1000000000LL;
    election_init(&e, 3);
    e.state          = STATE_LEADER;
    e.state_entry_ns = t0;
    e.leader_id      = 3;
    /* Higher ID should not demote us */
    NodeState s = election_on_announce(&e, 99, e.election_term, t0 + 1);
    EXPECT(ctx, s == STATE_LEADER);
}

static void test_holdover_on_timeout(TestCtx *ctx)
{
    ElectionState e;
    int64_t t0 = 1000000000LL;
    election_init(&e, 7);
    e.state              = STATE_FOLLOWER;
    e.state_entry_ns     = t0;
    e.last_heartbeat_ns  = t0;
    e.missed_heartbeats  = 0;

    /* Simulate 3 missed heartbeats by calling tick without updating heartbeat */
    NodeState s = STATE_FOLLOWER;
    for (int i = 0; i < 3; i++) {
        t0 += HEARTBEAT_TIMEOUT_NS + 1;
        s = election_tick(&e, t0);
    }
    EXPECT(ctx, s == STATE_HOLDOVER);
}

static void test_holdover_expires(TestCtx *ctx)
{
    ElectionState e;
    int64_t t0 = 1000000000LL;
    election_init(&e, 4);
    e.state          = STATE_HOLDOVER;
    e.state_entry_ns = t0;
    NodeState s = election_tick(&e, t0 + HOLDOVER_MAX_NS + 1);
    EXPECT(ctx, s == STATE_CANDIDATE);
}

void test_election_suite(TestCtx *ctx)
{
    printf("--- election ---\n");
    test_initial_state(ctx);
    test_ground_expires(ctx);
    test_listen_to_candidate(ctx);
    test_candidate_becomes_leader(ctx);
    test_lower_id_demotes_leader(ctx);
    test_higher_id_ignored(ctx);
    test_holdover_on_timeout(ctx);
    test_holdover_expires(ctx);
}
