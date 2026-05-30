#pragma once
#include <stdint.h>

typedef enum {
    STATE_GROUND       = 0,
    STATE_CALIBRATION  = 1,
    STATE_LISTEN       = 2,
    STATE_CANDIDATE    = 3,
    STATE_FOLLOWER     = 4,
    STATE_LEADER       = 5,
    STATE_HOLDOVER     = 6,
} NodeState;

#define GROUND_DURATION_NS     2000000000LL  /* 2 s */
#define HEARTBEAT_INTERVAL_NS  100000000LL   /* 100 ms */
#define HEARTBEAT_TIMEOUT_NS   300000000LL   /* 3 × 100 ms */
#define HOLDOVER_MAX_NS        10000000000LL /* 10 s */

typedef struct {
    NodeState state;
    uint32_t  node_id;
    uint32_t  leader_id;
    uint32_t  election_term;
    int64_t   state_entry_ns;    /* mono_raw_ns() when state was entered */
    int64_t   last_heartbeat_ns; /* last heartbeat from a valid lower-ID leader */
    int64_t   listen_timeout_ns; /* randomized LISTEN→CANDIDATE window, §6.1 */
    int       missed_heartbeats;
} ElectionState;

void      election_init(ElectionState *e, uint32_t node_id);

/* Call periodically; returns new state if transition occurred, else current. */
NodeState election_tick(ElectionState *e, int64_t now_ns);

/* Called when an ANNOUNCE packet is received from another node. */
NodeState election_on_announce(ElectionState *e, uint32_t sender_id,
                                uint32_t sender_term, int64_t now_ns);
