# Leader Election & State Machine

## Election Model

The system implements a modified Bully algorithm.

Lowest NodeID SHALL win leadership.

Election timeout SHALL be randomized within:

```text
250–500 ms
```

to reduce election collisions.


## Node States

| State | Description |
|---|---|
| GROUND | Startup stabilization |
| CALIBRATION | Self-latency measurement |
| LISTEN | Passive discovery |
| CANDIDATE | Election in progress |
| FOLLOWER | Synchronized to leader |
| LEADER | Authoritative clock source |
| HOLDOVER | Temporary freerun mode |
|

## GROUND State

After boot:

- filters SHALL be zeroed
- transmissions SHALL be suppressed
- multicast listening SHALL remain active

Duration:

```text
2 seconds
```


## Heartbeats

| Property | Value |
|---|---|
| Leader announce interval | 100 ms |
| Follower timeout | 3 missed heartbeats |
| Sync exchange period | 50 ms |


## Immediate Demotion

If a lower-ID node appears:

- current leader SHALL demote immediately
- election SHALL restart deterministically

The lower-ID node SHALL assume leadership only after:

- successful calibration
- valid heartbeat transmission


## HOLDOVER Mode

If leader communication is lost:

- node SHALL enter HOLDOVER
- last stable Rate SHALL be maintained
- Offset SHALL be frozen
- GPIO 23 SHALL transition HIGH
- drift adaptation SHALL stop

Maximum HOLDOVER duration:

```text
10 seconds
```

After expiration:

- node SHALL re-enter election


