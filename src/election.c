#include "election.h"
#include "clock.h"

void election_init(ElectionState *e, uint32_t node_id)
{
    e->state             = STATE_GROUND;
    e->node_id           = node_id;
    e->leader_id         = 0;
    e->election_term     = 0;
    e->state_entry_ns    = mono_raw_ns();
    e->last_heartbeat_ns = 0;
    e->missed_heartbeats = 0;
}

static NodeState enter(ElectionState *e, NodeState s, int64_t now_ns)
{
    e->state          = s;
    e->state_entry_ns = now_ns;
    return s;
}

NodeState election_tick(ElectionState *e, int64_t now_ns)
{
    int64_t elapsed = now_ns - e->state_entry_ns;

    switch (e->state) {
    case STATE_GROUND:
        if (elapsed >= GROUND_DURATION_NS)
            return enter(e, STATE_CALIBRATION, now_ns);
        break;

    case STATE_CALIBRATION:
        /* Caller signals calibration done by calling election_tick after
         * setting state to LISTEN externally — or we self-advance after
         * calibration. Handled by main loop; here just advance to LISTEN. */
        return enter(e, STATE_LISTEN, now_ns);

    case STATE_LISTEN:
        /* If no leader seen within one heartbeat timeout, become CANDIDATE. */
        if (e->last_heartbeat_ns == 0 &&
            elapsed >= HEARTBEAT_TIMEOUT_NS) {
            e->election_term++;
            return enter(e, STATE_CANDIDATE, now_ns);
        }
        break;

    case STATE_CANDIDATE:
        /* Simple: lowest ID wins. If we're alone we declare leadership. */
        e->leader_id = e->node_id;
        return enter(e, STATE_LEADER, now_ns);

    case STATE_LEADER:
        /* Nothing to do; leader stays until a lower-ID appears. */
        break;

    case STATE_FOLLOWER:
        /* Check heartbeat timeout */
        if (now_ns - e->last_heartbeat_ns > HEARTBEAT_TIMEOUT_NS) {
            e->missed_heartbeats++;
            if (e->missed_heartbeats >= 3) {
                e->leader_id = 0;
                return enter(e, STATE_HOLDOVER, now_ns);
            }
        } else {
            e->missed_heartbeats = 0;
        }
        break;

    case STATE_HOLDOVER:
        if (elapsed >= HOLDOVER_MAX_NS) {
            e->election_term++;
            return enter(e, STATE_CANDIDATE, now_ns);
        }
        break;
    }

    return e->state;
}

NodeState election_on_announce(ElectionState *e, uint32_t sender_id,
                                uint32_t sender_term, int64_t now_ns)
{
    e->last_heartbeat_ns = now_ns;

    /* Ignore stale terms */
    if (sender_term < e->election_term)
        return e->state;

    if (sender_term > e->election_term)
        e->election_term = sender_term;

    /* Lower ID always wins */
    if (sender_id < e->node_id) {
        e->leader_id         = sender_id;
        e->missed_heartbeats = 0;
        if (e->state != STATE_FOLLOWER)
            return enter(e, STATE_FOLLOWER, now_ns);
    }

    return e->state;
}
